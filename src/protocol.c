#include "protocol.h"
#include "network.h"

// Send exactly length bytes to socket.
int send_all(SOCKET sock, const void *data, uint32_t length) {
  const char *ptr = (const char *)data;
  uint32_t remaining = length;
  while (remaining > 0) {
    int sent = send(sock, ptr, remaining, 0);
    if (sent == SOCKET_ERROR) {
      log_message(LOG_ERROR, "send failed: %d", WSAGetLastError());
      return -1;
    }
    ptr += sent;
    remaining -= sent;
  }
  return 0;
}

// Send a protocol message over a socket.
int send_message(SOCKET sock, MessageType type, const void *data, uint32_t length) {
  MessageHeader header;
  header.type = htonl(type);
  header.length = htonl(length);

  // Send header
  if (send_all(sock, &header, sizeof(header)) != 0) {
    return -1;
  }

  // Send data if any
  if (data && length > 0) {
    if (send_all(sock, data, length) != 0) {
      return -1;
    }
  }

  return 0;
}

// Receive exactly length bytes from socket.
int recv_all(SOCKET sock, void *data, uint32_t length) {
  char *ptr = (char *)data;
  uint32_t remaining = length;
  while (remaining > 0) {
    int received = recv(sock, ptr, remaining, 0);
    if (received == 0) {
      log_message(LOG_WARN, "Connection closed by peer during recv");
      return -1;
    }
    if (received == SOCKET_ERROR) {
      log_message(LOG_ERROR, "recv failed: %d", WSAGetLastError());
      return -1;
    }
    ptr += received;
    remaining -= received;
  }
  return 0;
}

// Receive a protocol message from a socket.
int recv_message(SOCKET sock, MessageType *type, void *data, uint32_t *length) {
  MessageHeader header;

  // Receive header
  if (recv_all(sock, &header, sizeof(header)) != 0) {
    return -1;
  }

  *type = ntohl(header.type);
  *length = ntohl(header.length);

  // Receive data if any
  if (*length > 0 && data) {
    if (recv_all(sock, data, *length) != 0) {
      return -1;
    }
  }

  return 0;
}

// Broadcast the server presence over UDP.
int send_broadcast(SOCKET sock, const char *server_name) {
  BroadcastMessage msg;
  memset(&msg, 0, sizeof(msg));
  strncpy(msg.magic, BROADCAST_TAG, sizeof(msg.magic) - 1);
  strncpy(msg.server_name, server_name, sizeof(msg.server_name) - 1);
  msg.port = TRANSFER_PORT;

  struct sockaddr_in broadcast_addr;
  memset(&broadcast_addr, 0, sizeof(broadcast_addr));
  broadcast_addr.sin_family = AF_INET;
  broadcast_addr.sin_port = htons(BROADCAST_PORT);
  broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;

  int sent = sendto(sock, (char *)&msg, sizeof(msg), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
  return (sent == sizeof(msg)) ? 0 : -1;
}

// Receive a broadcast presence message over UDP.
int recv_broadcast(SOCKET sock, BroadcastMessage *msg, struct sockaddr_in *sender) {
  socklen_t sender_len = sizeof(*sender);
  int received;
  
  received = recvfrom(sock, (char *)msg, sizeof(*msg), 0, (struct sockaddr *)sender, &sender_len);

  if (received != sizeof(*msg)) {
    return -1;
  }

  // Validate magic
  if (strncmp(msg->magic, BROADCAST_TAG, strlen(BROADCAST_TAG)) != 0) {
    return -1;
  }

  return 0;
}
