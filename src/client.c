#include "common.h"
#include "crypto.h"
#include "filebrowser.h"
#include "network.h"
#include "protocol.h"

#define MAX_SERVERS 10

typedef struct {
  char ip[INET_ADDRSTRLEN];
  char name[64];
  uint16_t port;
} DiscoveredServer;


// Discover available servers broadcasting on the network.

int discover_servers(DiscoveredServer *servers, int max_servers,
                     int timeout_seconds) {
  SOCKET sock = create_udp_socket();
  if (sock == INVALID_SOCKET)
    return 0;
  enable_reuse_addr(sock);
  if (bind_socket(sock, BROADCAST_PORT) != 0) {
    closesocket(sock);
    return 0;
  }
  set_socket_timeout(sock, 1000);
  int server_count = 0;
  time_t start_time = time(NULL);
  printf("Searching for servers");
  fflush(stdout);
  while (time(NULL) - start_time < timeout_seconds &&
         server_count < max_servers) {
    BroadcastMessage msg;
    struct sockaddr_in sender;
    if (recv_broadcast(sock, &msg, &sender) == 0) {
      char ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &sender.sin_addr, ip, sizeof(ip));
      int found = 0;
      for (int i = 0; i < server_count; i++)
        if (strcmp(servers[i].ip, ip) == 0) {
          found = 1;
          break;
        }
      if (!found) {
        strncpy(servers[server_count].ip, ip, INET_ADDRSTRLEN);
        strncpy(servers[server_count].name, msg.server_name,
                sizeof(servers[server_count].name));
        servers[server_count].port = msg.port ? msg.port : TRANSFER_PORT;
        server_count++;
        printf("\n  Found: %s (%s)", msg.server_name, ip);
        fflush(stdout);
      }
    }
    printf(".");
    fflush(stdout);
  }
  printf("\n");
  closesocket(sock);
  return server_count;
}


// Prompt the user to select a server from the discovered list.
int select_server(DiscoveredServer *servers, int count) {
  if (count == 0)
    return -1;
  printf("\n=== Available Servers ===\n");
  for (int i = 0; i < count; i++)
    printf("  [%d] %s (%s:%d)\n", i + 1, servers[i].name, servers[i].ip,
           servers[i].port);
  printf("  [0] Cancel\n\nSelect server: ");
  int choice;
  if (scanf("%d", &choice) != 1 || choice < 0 || choice > count)
    return -1;
  while (getchar() != '\n')
    ;
  return choice - 1;
}


// Send a single file to the connected server.
int send_file(SOCKET sock, CryptoContext *crypto, const char *full_path, const char *rel_path) {
#ifdef _WIN32
  wchar_t *w_full_path = get_wide_path(full_path);
  FILE *fp = NULL;
  if (w_full_path) {
      HANDLE hFile = CreateFileW(w_full_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
      if (hFile != INVALID_HANDLE_VALUE) {
          int fd = _open_osfhandle((intptr_t)hFile, _O_RDONLY | _O_BINARY);
          if (fd != -1) {
              fp = _fdopen(fd, "rb");
          } else {
              CloseHandle(hFile);
          }
      }
      free(w_full_path);
  }
#else
  FILE *fp = fopen(full_path, "rb");
#endif
  if (!fp) {
      printf("\nFailed to open file: %s\n", full_path);
      return -1;
  }

  // Get file size
  fseek(fp, 0L, SEEK_END);
#ifdef _WIN32
  uint64_t filesize = _ftelli64(fp);
#else
  uint64_t filesize = ftello(fp);
#endif
  rewind(fp);

  FileInfoMessage info;
  memset(&info, 0, sizeof(info));
  strncpy(info.filename, rel_path, MAX_FILENAME_LEN - 1);
  info.filesize = filesize;
  
  printf("Sending: %s (%" PRIu64 " bytes)\n", rel_path, filesize);

  if(send_message(sock, MSG_FILE_INFO, &info, sizeof(info)) != 0) {
      printf("Error sending file info for %s\n", rel_path);
      fclose(fp);
      return -1;
  }

  unsigned char buffer[BUFFER_SIZE];
  uint64_t sent = 0;
  
  while (!feof(fp)) {
    size_t read = fread(buffer, 1, BUFFER_SIZE - 32, fp);
    if (read == 0)
      break;

    FileDataMessage data;
    memset(&data, 0, sizeof(data));
    int encrypted =
        crypto_encrypt(crypto, buffer, (int)read, data.data, data.tag);
    data.chunk_size = encrypted;

    if (send_message(sock, MSG_FILE_DATA, &data, sizeof(data)) != 0) {
      printf("\nError sending file data\n");
      fclose(fp);
      return -1;
    }
    sent += read;
    if(filesize > 0)
        printf("\rProgress: %d%%", (int)((sent * 100) / filesize));
  }
  printf("\n");
  fclose(fp);
  return 0;
}


// Send a directory and its contents recursively to the server.
int send_directory_recursive(SOCKET sock, CryptoContext *crypto, const char *base_path, const char *rel_path) {
#ifdef _WIN32
    wchar_t *w_current_dir = get_wide_path(base_path);
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

    if (find_handle == INVALID_HANDLE_VALUE) {
        return -1;
    }

    do {
        if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0) {
            continue;
        }

        char utf8_name[MAX_FILENAME_LEN];
        WideCharToMultiByte(CP_UTF8, 0, find_data.cFileName, -1, utf8_name, MAX_FILENAME_LEN, NULL, NULL);

        char new_base_path[MAX_PATH_LEN];
        char new_rel_path[MAX_PATH_LEN];
        
        snprintf(new_base_path, sizeof(new_base_path), "%s%s%s", base_path, PATH_SEP_STR, utf8_name);
        
        if (strlen(rel_path) > 0) {
            snprintf(new_rel_path, sizeof(new_rel_path), "%s%s%s", rel_path, PATH_SEP_STR, utf8_name);
        } else {
            strncpy(new_rel_path, utf8_name, sizeof(new_rel_path) - 1);
            new_rel_path[sizeof(new_rel_path) - 1] = '\0';
        }

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            send_directory_recursive(sock, crypto, new_base_path, new_rel_path);
        } else {
            send_file(sock, crypto, new_base_path, new_rel_path);
        }
    } while (FindNextFileW(find_handle, &find_data));

    FindClose(find_handle);
#else
    DIR *dir = opendir(base_path);
    if (!dir) return -1;
    struct dirent *dp;
    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
            continue;
        }

        char new_base_path[MAX_PATH_LEN];
        char new_rel_path[MAX_PATH_LEN];
        
        snprintf(new_base_path, sizeof(new_base_path), "%s%s%s", base_path, PATH_SEP_STR, dp->d_name);
        
        if (strlen(rel_path) > 0) {
            snprintf(new_rel_path, sizeof(new_rel_path), "%s%s%s", rel_path, PATH_SEP_STR, dp->d_name);
        } else {
            strncpy(new_rel_path, dp->d_name, sizeof(new_rel_path) - 1);
            new_rel_path[sizeof(new_rel_path) - 1] = '\0';
        }

        struct stat st;
        if (stat(new_base_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                send_directory_recursive(sock, crypto, new_base_path, new_rel_path);
            } else {
                send_file(sock, crypto, new_base_path, new_rel_path);
            }
        }
    }
    closedir(dir);
#endif
    return 0;
}


// Send all selected files from the file browser to the server.
int send_files(SOCKET sock, CryptoContext *crypto, FileBrowser *fb) {
  for (int i = 0; i < fb->selected_count; i++) {
    FileEntry *entry = &fb->selected_files[i];
    if (entry->is_directory) {
        send_directory_recursive(sock, crypto, entry->full_path, entry->name);
    } else {
        send_file(sock, crypto, entry->full_path, entry->name);
    }
  }
  return 0;
}


// Main entry point for the client.
int run_client(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  DiscoveredServer servers[MAX_SERVERS];
  int count = discover_servers(servers, MAX_SERVERS, 5);
  if (count == 0) {
    return 1;
  }
  int selected = select_server(servers, count);
  if (selected < 0) {
    return 0;
  }
  DiscoveredServer *server = &servers[selected];
  SOCKET sock = create_tcp_socket();
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(server->port);
  inet_pton(AF_INET, server->ip, &addr.sin_addr);
  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
    closesocket(sock);
    return 1;
  }
  MessageType type;
  uint32_t len;
  
  KeyExchangeMessage server_key;
  memset(&server_key, 0, sizeof(server_key));
  if (recv_message(sock, &type, &server_key, &len) != 0 || type != MSG_KEY_EXCHANGE) {
      closesocket(sock);
      return 1;
  }
  
  CryptoContext crypto;
  memset(&crypto, 0, sizeof(crypto));
  
  if (crypto_generate_key(&crypto, 0) != 0) {
      crypto_free_context(&crypto);
      closesocket(sock);
      return 1;
  }
  
  memcpy(crypto.iv, server_key.iv, AES_IV_SIZE);
  
  if (crypto_derive_aes_key(&crypto, server_key.pubkey, server_key.pubkey_len) != 0) {
      crypto_free_context(&crypto);
      closesocket(sock);
      return 1;
  }
  
  KeyExchangeMessage client_key;
  memset(&client_key, 0, sizeof(client_key));
  crypto_export_public_key(&crypto, client_key.pubkey, &client_key.pubkey_len);
  
  if (send_message(sock, MSG_KEY_EXCHANGE, &client_key, sizeof(client_key)) != 0) {
      crypto_free_context(&crypto);
      closesocket(sock);
      return 1;
  }
  
  recv_message(sock, &type, NULL, &len); // wait for MSG_VERIFY_REQ
  printf("\n  Enter the verification code shown on the server: ");
  VerifyMessage verify;
  memset(&verify, 0, sizeof(verify));
  if (fgets(verify.code, sizeof(verify.code), stdin) == NULL)
    return 1;
  verify.code[strcspn(verify.code, "\r\n")] = '\0';
  send_message(sock, MSG_VERIFY_RESP, &verify, sizeof(verify));
  recv_message(sock, &type, NULL, &len);
  if (type == MSG_VERIFY_FAIL) {
    closesocket(sock);
    return 1;
  }
  FileBrowser fb;
  filebrowser_init(&fb, NULL);
  int selected_count = filebrowser_run(&fb);
  if (selected_count > 0)
    send_files(sock, &crypto, &fb);
  send_message(sock, MSG_SESSION_END, NULL, 0);
  filebrowser_cleanup(&fb);
  crypto_free_context(&crypto);
  closesocket(sock);
  return 0;
}
