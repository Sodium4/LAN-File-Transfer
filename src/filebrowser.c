#include "filebrowser.h"
#include "common.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

// ANSI escape codes for terminal control
#define CLEAR_SCREEN "\033[2J\033[H"
#define CURSOR_HOME "\033[H"
#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"
#define COLOR_RESET "\033[0m"
#define COLOR_DIR "\033[1;34m"
#define COLOR_FILE "\033[0;37m"
#define COLOR_SELECT "\033[1;32m"
#define COLOR_CURSOR "\033[7m"
#define COLOR_HEADER "\033[1;36m"


// Compare two file entries for sorting.
static int compare_entries(const void *a, const void *b) {
  const FileEntry *ea = (const FileEntry *)a;
  const FileEntry *eb = (const FileEntry *)b;

  if (ea->is_directory && !eb->is_directory)
    return -1;
  if (!ea->is_directory && eb->is_directory)
    return 1;

#ifdef _WIN32
  return _stricmp(ea->name, eb->name);
#else
  return strcasecmp(ea->name, eb->name);
#endif
}


// Initialize the file browser with a starting directory path.
int filebrowser_init(FileBrowser *fb, const char *start_path) {
  memset(fb, 0, sizeof(FileBrowser));

  if (start_path && strlen(start_path) > 0) {
    strncpy(fb->current_path, start_path, MAX_PATH_LEN - 1);
  } else {
#ifdef _WIN32
    if (_getcwd(fb->current_path, MAX_PATH_LEN) == NULL) {
      strcpy(fb->current_path, "C:\\");
    }
#else
    if (getcwd(fb->current_path, MAX_PATH_LEN) == NULL) {
      strcpy(fb->current_path, "/");
    }
#endif
  }

  fb->visible_rows = 20;
  fb->selected_files = malloc(sizeof(FileEntry) * MAX_FILES);
  if (!fb->selected_files)
    return -1;

  return filebrowser_refresh(fb);
}


// Cleanup the file browser resources.
void filebrowser_cleanup(FileBrowser *fb) {
  if (fb->selected_files) {
    free(fb->selected_files);
    fb->selected_files = NULL;
  }
}


// Refresh the current directory entries in the file browser.
int filebrowser_refresh(FileBrowser *fb) {
  fb->entry_count = 0;

#ifdef _WIN32
  if (strlen(fb->current_path) == 0) {
    // List drives
    char drives[256];
    if (GetLogicalDriveStringsA(sizeof(drives), drives)) {
      char *drive = drives;
      while (*drive && fb->entry_count < MAX_FILES) {
        FileEntry *entry = &fb->entries[fb->entry_count++];
        strncpy(entry->name, drive, MAX_FILENAME_LEN - 1);
        strncpy(entry->full_path, drive, MAX_PATH_LEN - 1);
        entry->is_directory = 1;
        entry->size = 0;
        entry->selected = 0;
        drive += strlen(drive) + 1;
      }
    }
    return 0;
  }
#else
  if (strlen(fb->current_path) == 0) {
      strcpy(fb->current_path, "/");
  }
#endif

  int is_root = 0;
#ifdef _WIN32
  if (strlen(fb->current_path) <= 3) is_root = 1;
#else
  if (strcmp(fb->current_path, "/") == 0) is_root = 1;
#endif

  if (!is_root) {
      FileEntry *entry = &fb->entries[fb->entry_count++];
      strcpy(entry->name, "..");
      strcpy(entry->full_path, ".."); 
      entry->is_directory = 1;
      entry->size = 0;
      entry->selected = 0;
  }

#ifdef _WIN32
  wchar_t *w_current_dir = get_wide_path(fb->current_path);
  if (!w_current_dir) return -1;
  
  size_t w_len = wcslen(w_current_dir);
  wchar_t *w_search_path = (wchar_t *)malloc((w_len + 5) * sizeof(wchar_t));
  wcscpy(w_search_path, w_current_dir);
  if (w_search_path[w_len - 1] != L'\\') {
      wcscat(w_search_path, L"\\*");
  } else {
      wcscat(w_search_path, L"*");
  }
  free(w_current_dir);

  WIN32_FIND_DATAW find_data;
  HANDLE find_handle = FindFirstFileW(w_search_path, &find_data);
  free(w_search_path);

  if (find_handle != INVALID_HANDLE_VALUE) {
    do {
      if (fb->entry_count >= MAX_FILES) break;

      if (wcscmp(find_data.cFileName, L".") == 0 ||
          wcscmp(find_data.cFileName, L"..") == 0) {
        continue;
      }

      char utf8_name[MAX_FILENAME_LEN];
      WideCharToMultiByte(CP_UTF8, 0, find_data.cFileName, -1, utf8_name, MAX_FILENAME_LEN, NULL, NULL);

      FileEntry *entry = &fb->entries[fb->entry_count++];
      strncpy(entry->name, utf8_name, MAX_FILENAME_LEN - 1);

      snprintf(entry->full_path, MAX_PATH_LEN, "%.*s\\%.*s",
               MAX_PATH_LEN - MAX_FILENAME_LEN - 2, fb->current_path,
               MAX_FILENAME_LEN, utf8_name);

      entry->is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
      entry->size = ((uint64_t)find_data.nFileSizeHigh << 32) | find_data.nFileSizeLow;
      entry->selected = 0;
    } while (FindNextFileW(find_handle, &find_data));
    FindClose(find_handle);
  }
#else
  DIR *dir = opendir(fb->current_path);
  if (dir) {
      struct dirent *dp;
      while ((dp = readdir(dir)) != NULL) {
          if (fb->entry_count >= MAX_FILES) break;
          if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
              continue;
          }
          FileEntry *entry = &fb->entries[fb->entry_count++];
          strncpy(entry->name, dp->d_name, MAX_FILENAME_LEN - 1);
          
          const char *sep = "";
          if (fb->current_path[strlen(fb->current_path) - 1] != '/') {
              sep = "/";
          }
          snprintf(entry->full_path, MAX_PATH_LEN, "%.*s%s%.*s",
                   MAX_PATH_LEN - MAX_FILENAME_LEN - 2, fb->current_path, sep,
                   MAX_FILENAME_LEN, dp->d_name);
                   
          struct stat st;
          if (stat(entry->full_path, &st) == 0) {
              entry->is_directory = S_ISDIR(st.st_mode);
              entry->size = st.st_size;
          } else {
              entry->is_directory = 0;
              entry->size = 0;
          }
          entry->selected = 0;
      }
      closedir(dir);
  }
#endif

  // Sort
  if (fb->entry_count > 1) {
      int start_idx = is_root ? 0 : 1;
      if (fb->entry_count > start_idx + 1) {
          qsort(fb->entries + start_idx, fb->entry_count - start_idx, sizeof(FileEntry), compare_entries);
      }
  }

  return 0;
}


// Format a file size in bytes to a human-readable string.
static void format_size(uint64_t size, char *buffer, size_t buf_size) {
  const char *units[] = {"B", "KB", "MB", "GB", "TB"};
  int unit = 0;
  double dsize = (double)size;

  while (dsize >= 1024 && unit < 4) {
    dsize /= 1024;
    unit++;
  }

  if (unit == 0) {
    snprintf(buffer, buf_size, "%" PRIu64 " %s", size, units[unit]);
  } else {
    snprintf(buffer, buf_size, "%.1f %s", dsize, units[unit]);
  }
}


// Render the current file browser state to the console.
void filebrowser_render(FileBrowser *fb) {
  printf(CLEAR_SCREEN);
  printf(COLOR_HEADER);
  printf("=== File Browser ===\n");
  printf(COLOR_RESET);
  printf("Path: %s\n", fb->current_path);
  printf("Selected: %d file(s)\n", fb->selected_count);
  printf("----------------------------------------\n");
  printf("[Arrow Keys] Navigate | [Space] Select | [Enter] Open/Confirm | [Q] Done\n");
  printf("----------------------------------------\n");

  int start = fb->scroll_offset;
  int end = start + fb->visible_rows;
  if (end > fb->entry_count)
    end = fb->entry_count;

  for (int i = start; i < end; i++) {
    FileEntry *entry = &fb->entries[i];
    if (i == fb->cursor)
      printf(COLOR_CURSOR);
    if (entry->selected)
      printf(COLOR_SELECT "[*] ");
    else
      printf("[ ] ");

    if (entry->is_directory)
      printf(COLOR_DIR);
    else
      printf(COLOR_FILE);

    char display_name[40];
    if (strlen(entry->name) > 35) {
      strncpy(display_name, entry->name, 32);
      strcpy(display_name + 32, "...");
    } else {
      strcpy(display_name, entry->name);
    }
    printf("%-38s", display_name);

    if (!entry->is_directory) {
      char size_str[16];
      format_size(entry->size, size_str, sizeof(size_str));
      printf(" %10s", size_str);
    } else {
      printf(" %10s", "<DIR>");
    }
    printf(COLOR_RESET "\n");
  }
}

#ifndef _WIN32
static int get_key(void) {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    if (ch == '\033') {
        getchar(); // '['
        int code = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        if (code == 'A') return 72;
        if (code == 'B') return 80;
        return 0;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    if (ch == '\n') return 13;
    return ch;
}
#else
static int get_key(void) {
    int ch = _getch();
    if (ch == 0 || ch == 224) {
        return _getch();
    }
    return ch;
}
#endif


// Run the interactive file browser interface loop.
int filebrowser_run(FileBrowser *fb) {
#ifdef _WIN32
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD dwMode = 0;
  GetConsoleMode(hOut, &dwMode);
  SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
  printf(HIDE_CURSOR);

  int running = 1;
  while (running) {
    filebrowser_render(fb);
    int ch = get_key();
    switch (ch) {
    case 72:
      if (fb->cursor > 0) {
        fb->cursor--;
        if (fb->cursor < fb->scroll_offset) {
          fb->scroll_offset = fb->cursor;
        }
      }
      break;
    case 80:
      if (fb->cursor < fb->entry_count - 1) {
        fb->cursor++;
        if (fb->cursor >= fb->scroll_offset + fb->visible_rows) {
          fb->scroll_offset = fb->cursor - fb->visible_rows + 1;
        }
      }
      break;
    case ' ':
      if (fb->cursor < fb->entry_count) {
        FileEntry *entry = &fb->entries[fb->cursor];
        entry->selected = !entry->selected;
        if (entry->selected) {
           int found = 0;
           for(int i=0; i<fb->selected_count; i++) {
               if(strcmp(fb->selected_files[i].full_path, entry->full_path) == 0) {
                   found = 1;
                   break;
               }
           }
           if(!found && fb->selected_count < MAX_FILES) {
               fb->selected_files[fb->selected_count++] = *entry;
           }
        } else {
          for (int i = 0; i < fb->selected_count; i++) {
            if (strcmp(fb->selected_files[i].full_path, entry->full_path) == 0) {
              memmove(&fb->selected_files[i], &fb->selected_files[i + 1],
                      (fb->selected_count - i - 1) * sizeof(FileEntry));
              fb->selected_count--;
              break;
            }
          }
        }
      }
      break;
    case 13:
      if (fb->cursor < fb->entry_count) {
        FileEntry *entry = &fb->entries[fb->cursor];
        if (entry->is_directory) {
          if (strcmp(entry->name, "..") == 0) {
#ifdef _WIN32
            size_t len = strlen(fb->current_path);
            if (len == 3 && fb->current_path[1] == ':' && fb->current_path[2] == '\\') {
               fb->current_path[0] = '\0';
            } else {
                char *last = strrchr(fb->current_path, '\\');
                if (last) {
                  if (last == fb->current_path + 2)
                    last[1] = '\0'; 
                  else
                    *last = '\0'; 
                }
            }
#else
            char *last = strrchr(fb->current_path, '/');
            if (last) {
              if (last == fb->current_path) {
                  strcpy(fb->current_path, "/");
              } else {
                  *last = '\0';
              }
            }
#endif
          } else {
            strncpy(fb->current_path, entry->full_path, MAX_PATH_LEN - 1);
          }
          fb->cursor = 0;
          fb->scroll_offset = 0;
          filebrowser_refresh(fb);
        } else if (fb->selected_count > 0) {
          running = 0;
        }
      }
      break;
    case 'q':
    case 'Q':
      running = 0;
      break;
    }
  }
  printf(SHOW_CURSOR);
  printf(CLEAR_SCREEN);
  return fb->selected_count;
}
