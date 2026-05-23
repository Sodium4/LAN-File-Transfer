#include "common.h"
#include "crypto.h"
#include "network.h"
#include "protocol.h"
#include <signal.h>

static volatile int running = 1;
#ifdef _WIN32
static char save_directory[MAX_PATH_LEN] = ".\\received";
#else
static char save_directory[MAX_PATH_LEN] = "./received";
#endif


// Signal handler to initiate graceful shutdown.
void signal_handler(int sig) {
  (void)sig;
  running = 0;
}

// Print command-line usage instructions.
void print_usage(const char *program) {
  printf("Usage: %s [-d <save_directory>]\n", program);
  printf("  -d <directory>  Directory to save received files (default: .\\received)\n");
}

// Generate a random verification code for client authentication.
char *generate_verification_code(void) {
  static char code[VERIFICATION_CODE_LEN + 1];
#ifdef _WIN32
  srand((unsigned int)time(NULL) ^ GetCurrentProcessId());
#else
  srand((unsigned int)time(NULL) ^ getpid());
#endif
  for (int i = 0; i < VERIFICATION_CODE_LEN; i++) {
    code[i] = '0' + (rand() % 10);
  }
  code[VERIFICATION_CODE_LEN] = '\0';
  return code;
}

typedef struct {
  SOCKET sock;
  volatile int *running;
} BroadcastThreadData;

// Thread routine to broadcast the server's presence over UDP.
#ifdef _WIN32
DWORD WINAPI broadcast_thread(LPVOID param) {
#else
void *broadcast_thread(void *param) {
#endif
  BroadcastThreadData *data = (BroadcastThreadData *)param;
  char hostname[64];
  gethostname(hostname, sizeof(hostname));

  while (*data->running) {
    send_broadcast(data->sock, hostname);
#ifdef _WIN32
    Sleep(2000); // Broadcast every 2 seconds
#else
    sleep(2);
#endif
  }
  return 0;
}

// Handle an incoming client connection, including verification and file reception.
int handle_client(SOCKET client_sock, const char *verification_code, CryptoContext *crypto) {
  char transfer_dir[MAX_PATH_LEN + 64]; // Increased size for timestamp
  char timestamp[32];
  get_timestamp(timestamp, sizeof(timestamp));

  // Use precision to ensure we don't overflow even theoretically, though buffer
  // is now larger
  snprintf(transfer_dir, sizeof(transfer_dir), "%.*s%stransfer_%s", MAX_PATH_LEN, save_directory, PATH_SEP_STR, timestamp);

  if (create_directory_recursive(transfer_dir) != 0) {
    log_message(LOG_WARN, "Directory may already exist: %s", transfer_dir);
  }

  KeyExchangeMessage key_msg;
  memset(&key_msg, 0, sizeof(key_msg));
  crypto_export_public_key(crypto, key_msg.pubkey, &key_msg.pubkey_len);
  memcpy(key_msg.iv, crypto->iv, AES_IV_SIZE);

  if (send_message(client_sock, MSG_KEY_EXCHANGE, &key_msg, sizeof(key_msg)) !=
      0) {
    log_message(LOG_ERROR, "Failed to send key exchange");
    return -1;
  }
  log_message(LOG_INFO, "Sent ECDH public key to client");

  // Receive Client's Public Key
  MessageType msg_type;
  uint32_t msg_len;
  KeyExchangeMessage client_key_msg;
  
  if (recv_message(client_sock, &msg_type, &client_key_msg, &msg_len) != 0) {
      log_message(LOG_ERROR, "Failed to receive client key exchange");
      return -1;
  }
  
  if (msg_type != MSG_KEY_EXCHANGE) {
      log_message(LOG_ERROR, "Unexpected message type: %d", msg_type);
      return -1;
  }
  
  if (crypto_derive_aes_key(crypto, client_key_msg.pubkey, client_key_msg.pubkey_len) != 0) {
      log_message(LOG_ERROR, "Failed to derive AES key");
      return -1;
  }
  log_message(LOG_INFO, "AES key derived successfully");

  if (send_message(client_sock, MSG_VERIFY_REQ, NULL, 0) != 0) {
    log_message(LOG_ERROR, "Failed to send verification request");
    return -1;
  }

  VerifyMessage verify_msg;

  if (recv_message(client_sock, &msg_type, &verify_msg, &msg_len) != 0) {
    log_message(LOG_ERROR, "Failed to receive verification response");
    return -1;
  }

  if (msg_type != MSG_VERIFY_RESP) {
    log_message(LOG_ERROR, "Unexpected message type: %d", msg_type);
    return -1;
  }

  if (strcmp(verify_msg.code, verification_code) != 0) {
    log_message(LOG_WARN, "Invalid verification code: %s", verify_msg.code);
    send_message(client_sock, MSG_VERIFY_FAIL, NULL, 0);
    return -1;
  }

  log_message(LOG_INFO, "Client verified successfully");
  send_message(client_sock, MSG_VERIFY_OK, NULL, 0);

  int files_received = 0;

  while (1) {
    MessageHeader header;
    // Read the next message header manually
    if (recv_all(client_sock, &header, sizeof(header)) != 0) {
      log_message(LOG_INFO, "Connection closed or error reading header");
      break;
    }

    uint32_t type = ntohl(header.type);
    uint32_t length = ntohl(header.length);

    if (type == MSG_SESSION_END) {
      log_message(LOG_INFO, "Session ended by client");
      // Consume any payload if present (though usually 0)
      if (length > 0) {
        char *dummy = malloc(length);
        if (dummy) {
          recv_all(client_sock, dummy, length);
          free(dummy);
        }
      }
      break;
    }

    if (type == MSG_FILE_INFO) {
      FileInfoMessage file_info;
      if (length != sizeof(file_info)) {
        log_message(LOG_ERROR, "Invalid file info length: %u", length);
        break;
      }

      if (recv_all(client_sock, &file_info, sizeof(file_info)) != 0) {
        log_message(LOG_ERROR, "Failed to receive file info body");
        break;
      }

      // Normalize path separators to current OS
      for (int i = 0; file_info.filename[i] != '\0'; i++) {
          if (file_info.filename[i] == '/' || file_info.filename[i] == '\\') {
              file_info.filename[i] = PATH_SEP;
          }
      }

      char file_path[MAX_PATH_LEN + MAX_FILENAME_LEN + 128];
      snprintf(file_path, sizeof(file_path), "%s%s%s", transfer_dir, PATH_SEP_STR, file_info.filename);

      // Ensure directory exists
      char temp_path[MAX_PATH_LEN + MAX_FILENAME_LEN + 128];
      strncpy(temp_path, file_path, sizeof(temp_path) - 1);
      char *last_sep = strrchr(temp_path, PATH_SEP);
      if (last_sep) {
          *last_sep = '\0';
          create_directory_recursive(temp_path);
      }

      log_message(LOG_INFO, "Receiving: %s (%llu bytes)", file_info.filename, file_info.filesize);

#ifdef _WIN32
      wchar_t *w_file_path = get_wide_path(file_path);
      FILE *fp = NULL;
      if (w_file_path) {
          HANDLE hFile = CreateFileW(w_file_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
          if (hFile != INVALID_HANDLE_VALUE) {
              int fd = _open_osfhandle((intptr_t)hFile, _O_WRONLY | _O_BINARY);
              if (fd != -1) {
                  fp = _fdopen(fd, "wb");
              } else {
                  CloseHandle(hFile);
              }
          }
          free(w_file_path);
      }
#else
      FILE *fp = fopen(file_path, "wb");
#endif
      
      if (!fp) {
        log_message(LOG_ERROR, "Failed to create file: %s", file_path);
        // consume the file data to keep stream sync
        uint64_t bytes_skipped = 0;
        while (bytes_skipped < file_info.filesize) {
            FileDataMessage data_msg;
            MessageType data_type;
            uint32_t data_len;
            if (recv_message(client_sock, &data_type, &data_msg, &data_len) != 0) {
                log_message(LOG_ERROR, "Failed to receive file data message (skip)");
                break;
            }
            if (data_type != MSG_FILE_DATA) {
              log_message(LOG_ERROR, "Unexpected message type during skip: %d", data_type);
              break;
            }
            unsigned char decrypted[BUFFER_SIZE];
            int decrypted_len = crypto_decrypt(crypto, data_msg.data, data_msg.chunk_size, data_msg.tag, decrypted);
            if (decrypted_len > 0)
              bytes_skipped += decrypted_len;
            else {
              log_message(LOG_ERROR, "Decryption failed during skip");
              break;
            }
        }
        continue;
      }

      uint64_t bytes_received = 0;

      while (bytes_received < file_info.filesize) {
        FileDataMessage data_msg;
        MessageType data_type;
        uint32_t data_len;

        if (recv_message(client_sock, &data_type, &data_msg, &data_len) != 0) {
          log_message(LOG_ERROR, "Failed to receive file data message");
          break;
        }

        if (data_type != MSG_FILE_DATA) {
          log_message(LOG_ERROR, "Unexpected message type during transfer: %d",
                      data_type);
          break;
        }

        unsigned char decrypted[BUFFER_SIZE];
        int decrypted_len =
            crypto_decrypt(crypto, data_msg.data, data_msg.chunk_size, data_msg.tag, decrypted);

        if (decrypted_len < 0) {
          log_message(LOG_ERROR, "Decryption failed");
          break;
        }

        fwrite(decrypted, 1, decrypted_len, fp);
        bytes_received += decrypted_len;

        // Visual progress
        if (file_info.filesize > 0) {
          printf("\rReceiving %s: %d%%", file_info.filename, (int)((bytes_received * 100) / file_info.filesize));
        }
      }
      printf("\n");

      fclose(fp);

      if (bytes_received == file_info.filesize) {
        log_message(LOG_INFO, "File received successfully: %s", file_info.filename);
        files_received++;
      }
    } else {
      log_message(LOG_WARN, "Unknown message type in main loop: %d", type);
      // Desync likely, abort
      break;
    }
  }

  log_message(LOG_INFO, "Transfer complete: %d file(s) received", files_received);
  log_message(LOG_INFO, "Files saved to: %s", transfer_dir);

  return 0;
}

// Main entry point for the server.
int run_server(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
      strncpy(save_directory, argv[++i], MAX_PATH_LEN - 1);
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      print_usage(argv[0]);
      return 0;
    }
  }

  signal(SIGINT, signal_handler);

  create_directory_recursive(save_directory);

  SOCKET broadcast_sock = create_udp_socket();
  if (broadcast_sock == INVALID_SOCKET) {
    return 1;
  }
  enable_broadcast(broadcast_sock);

  SOCKET server_sock = create_tcp_socket();
  if (server_sock == INVALID_SOCKET) {
    closesocket(broadcast_sock);
    return 1;
  }

  enable_reuse_addr(server_sock);

  if (bind_socket(server_sock, TRANSFER_PORT) != 0) {
    closesocket(broadcast_sock);
    closesocket(server_sock);
    return 1;
  }

  if (listen(server_sock, 5) == SOCKET_ERROR) {
    log_message(LOG_ERROR, "Failed to listen: %d", WSAGetLastError());
    closesocket(broadcast_sock);
    closesocket(server_sock);
    return 1;
  }

  char *verification_code = generate_verification_code();

  printf("\n");
  printf("============================================\n");
  printf("    Secure File Transfer Server V2\n");
  printf("============================================\n");
  printf("  IP Address:  %s\n", get_local_ip());
  printf("  Port:        %d\n", TRANSFER_PORT);
  printf("  Save Path:   %s\n", save_directory);
  printf("--------------------------------------------\n");
  printf("  VERIFICATION CODE:  %s\n", verification_code);
  printf("--------------------------------------------\n");
  printf("  Waiting for connections...\n");
  printf("  Press Ctrl+C to stop\n");
  printf("============================================\n\n");

  BroadcastThreadData thread_data = {broadcast_sock, &running};
#ifdef _WIN32
  HANDLE broadcast_handle =
      CreateThread(NULL, 0, broadcast_thread, &thread_data, 0, NULL);
#else
  pthread_t broadcast_handle;
  pthread_create(&broadcast_handle, NULL, broadcast_thread, &thread_data);
#endif

  while (running) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(server_sock, &read_fds);

    struct timeval timeout = {1, 0};

    int ready = select(server_sock + 1, &read_fds, NULL, NULL, &timeout);
    if (ready > 0 && FD_ISSET(server_sock, &read_fds)) {
      struct sockaddr_in client_addr;
      socklen_t addr_len = sizeof(client_addr);

      SOCKET client_sock =
          accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
      if (client_sock != INVALID_SOCKET) {
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        log_message(LOG_INFO, "Client connected: %s", client_ip);

        CryptoContext crypto;
        memset(&crypto, 0, sizeof(crypto));
        if (crypto_generate_key(&crypto, 1) != 0) {
          log_message(LOG_ERROR, "Failed to generate encryption key");
          closesocket(client_sock);
          continue;
        }

        handle_client(client_sock, verification_code, &crypto);

        crypto_free_context(&crypto);
        closesocket(client_sock);

        verification_code = generate_verification_code();
        printf("\n--------------------------------------------\n");
        printf("  NEW VERIFICATION CODE:  %s\n", verification_code);
        printf("--------------------------------------------\n\n");
      }
    }
  }

  running = 0;
#ifdef _WIN32
  WaitForSingleObject(broadcast_handle, 1000);
  CloseHandle(broadcast_handle);
#else
  pthread_join(broadcast_handle, NULL);
#endif

  closesocket(broadcast_sock);
  closesocket(server_sock);

  log_message(LOG_INFO, "Server stopped");
  return 0;
}
