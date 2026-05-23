#include "common.h"
#include <stdarg.h>

#ifdef _WIN32
static WSADATA wsa_data;
#endif

// Convert a UTF-8 path to a wide character path with the "\\?\" prefix.
#ifdef _WIN32
wchar_t* get_wide_path(const char *path) {
    if (!path) return NULL;
    
    // First, convert UTF-8 to wide string
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (wlen <= 0) return NULL;
    
    wchar_t *wpath = (wchar_t *)malloc(wlen * sizeof(wchar_t));
    if (!wpath) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen);
    
    // Get full absolute path
    DWORD full_len = GetFullPathNameW(wpath, 0, NULL, NULL);
    if (full_len == 0) {
        free(wpath);
        return NULL;
    }
    
    wchar_t *full_wpath = (wchar_t *)malloc(full_len * sizeof(wchar_t));
    if (!full_wpath) {
        free(wpath);
        return NULL;
    }
    GetFullPathNameW(wpath, full_len, full_wpath, NULL);
    free(wpath);
    
    // If it already has \\?\, return it
    if (wcsncmp(full_wpath, L"\\\\?\\", 4) == 0) {
        return full_wpath;
    }
    
    // Check for UNC path
    int is_unc = (wcsncmp(full_wpath, L"\\\\", 2) == 0);
    size_t prefix_len = is_unc ? 8 : 4; // "\\?\UNC\" or "\\?\"
    size_t final_len = prefix_len + wcslen(full_wpath) + 1;
    
    wchar_t *final_wpath = (wchar_t *)malloc(final_len * sizeof(wchar_t));
    if (!final_wpath) {
        free(full_wpath);
        return NULL;
    }
    
    if (is_unc) {
        wcscpy(final_wpath, L"\\\\?\\UNC\\");
        wcscat(final_wpath, full_wpath + 2); // Skip the initial
    } else {
        wcscpy(final_wpath, L"\\\\?\\");
        wcscat(final_wpath, full_wpath);
    }
    
    free(full_wpath);
    return final_wpath;
}
#endif


// Initialize Winsock.
int init_winsock(void) {
#ifdef _WIN32
  int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (result != 0) {
    fprintf(stderr, "WSAStartup failed: %d\n", result);
    return -1;
  }
#endif
  return 0;
}


// Cleanup Winsock resources.
void cleanup_winsock(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}


// Get the current timestamp string.
void get_timestamp(char *buffer, size_t size) {
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  strftime(buffer, size, "%Y%m%d_%H%M%S", tm_info);
}


// Log a message to the console.
void log_message(LogLevel level, const char *format, ...) {
  const char *level_str;
  switch (level) {
  case LOG_INFO:
    level_str = "INFO";
    break;
  case LOG_WARN:
    level_str = "WARN";
    break;
  case LOG_ERROR:
    level_str = "ERROR";
    break;
  default:
    level_str = "???";
    break;
  }

  char timestamp[32];
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);

  printf("[%s] [%s] ", timestamp, level_str);

  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);

  printf("\n");
}


// Create a directory recursively.

int create_directory_recursive(const char *path) {
  char tmp[MAX_PATH_LEN];
  char *p = NULL;
  size_t len;

  strncpy(tmp, path, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = '\0';
  len = strlen(tmp);

  // Remove trailing slash
  if (tmp[len - 1] == '\\' || tmp[len - 1] == '/') {
    tmp[len - 1] = '\0';
  }

  for (p = tmp + 1; *p; p++) {
    if (*p == '\\' || *p == '/') {
      char sep = *p;
      *p = '\0';
#ifdef _WIN32
      wchar_t *w_tmp = get_wide_path(tmp);
      if (w_tmp) {
        CreateDirectoryW(w_tmp, NULL);
        free(w_tmp);
      }
#else
      mkdir(tmp, 0777);
#endif
      *p = sep; // restore slash
    }
  }
  
#ifdef _WIN32
  wchar_t *w_tmp = get_wide_path(tmp);
  int res = -1;
  if (w_tmp) {
    if (CreateDirectoryW(w_tmp, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        res = 0;
    }
    free(w_tmp);
  }
  return res;
#else
  int res = mkdir(tmp, 0777);
  if (res == 0 || errno == EEXIST) {
      return 0;
  }
  return -1;
#endif
}
