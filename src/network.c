#include "network.h"

// Create a UDP socket.
SOCKET create_udp_socket(void) {
  SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET) {
    log_message(LOG_ERROR, "Failed to create UDP socket: %d",
                (int)WSAGetLastError());
    return INVALID_SOCKET;
  }
  return sock;
}

// Create a TCP socket.
SOCKET create_tcp_socket(void) {
  SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock == INVALID_SOCKET) {
    log_message(LOG_ERROR, "Failed to create TCP socket: %d",
                (int)WSAGetLastError());
    return INVALID_SOCKET;
  }
  return sock;
}

// Enable the SO_BROADCAST option on a socket.
int enable_broadcast(SOCKET sock) {
#ifdef _WIN32
  BOOL broadcast = TRUE;
#else
  int broadcast = 1;
#endif
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&broadcast,
                 sizeof(broadcast)) == SOCKET_ERROR) {
    log_message(LOG_ERROR, "Failed to enable broadcast: %d",
                (int)WSAGetLastError());
    return -1;
  }
  return 0;
}

// Enable the SO_REUSEADDR option on a socket.
int enable_reuse_addr(SOCKET sock) {
#ifdef _WIN32
  BOOL reuse = TRUE;
#else
  int reuse = 1;
#endif
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse,
                 sizeof(reuse)) == SOCKET_ERROR) {
    log_message(LOG_ERROR, "Failed to enable reuse addr: %d",
                (int)WSAGetLastError());
    return -1;
  }
  return 0;
}

// Bind a socket to a given port on all interfaces.
int bind_socket(SOCKET sock, uint16_t port) {
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
    log_message(LOG_ERROR, "Failed to bind socket: %d", (int)WSAGetLastError());
    return -1;
  }
  return 0;
}


// Set send and receive timeout on a socket.
int set_socket_timeout(SOCKET sock, int timeout_ms) {
#ifdef _WIN32
  DWORD timeout = timeout_ms;
  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                 sizeof(timeout)) == SOCKET_ERROR) {
#else
  struct timeval timeout;
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;
  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                 sizeof(timeout)) == SOCKET_ERROR) {
#endif
    log_message(LOG_ERROR, "Failed to set socket timeout: %d",
                (int)WSAGetLastError());
    return -1;
  }
  return 0;
}

// Get the local IP address string.
char *get_local_ip(void) {
  static char ip_buffer[INET_ADDRSTRLEN];
  char hostname[256];

  if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
    strcpy(ip_buffer, "127.0.0.1");
    return ip_buffer;
  }

  struct addrinfo hints, *info;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(hostname, NULL, &hints, &info) != 0) {
    strcpy(ip_buffer, "127.0.0.1");
    return ip_buffer;
  }

  struct sockaddr_in *addr = (struct sockaddr_in *)info->ai_addr;
  inet_ntop(AF_INET, &addr->sin_addr, ip_buffer, INET_ADDRSTRLEN);

  freeaddrinfo(info);
  return ip_buffer;
}
