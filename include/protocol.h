#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "common.h"
#include "crypto.h"

// Message types
typedef enum {
  MSG_DISCOVER = 1, // UDP broadcast
  MSG_KEY_EXCHANGE, // Server sends encryption key
  MSG_VERIFY_REQ,   // Server requests verification
  MSG_VERIFY_RESP,  // Client sends code
  MSG_VERIFY_OK,    // Verification passed
  MSG_VERIFY_FAIL,  // Verification failed
  MSG_FILE_INFO,    // File metadata
  MSG_FILE_DATA,    // Encrypted file chunk
  MSG_FILE_DONE,    // Transfer complete
  MSG_SESSION_END,  // End of session
  MSG_ERROR         // Error message
} MessageType;

// Message header
#pragma pack(push, 1)
typedef struct {
  uint32_t type;
  uint32_t length;
} MessageHeader;

// Broadcast message
typedef struct {
  char magic[16];
  char server_name[64];
  uint16_t port;
} BroadcastMessage;

// Key exchange message
typedef struct {
  uint32_t pubkey_len;
  unsigned char pubkey[256];
  unsigned char iv[AES_IV_SIZE];
} KeyExchangeMessage;

// Verification response
typedef struct {
  char code[VERIFICATION_CODE_LEN + 1];
} VerifyMessage;

// File info message
typedef struct {
  char filename[MAX_FILENAME_LEN];
  uint64_t filesize;
} FileInfoMessage;

// File data message
typedef struct {
  uint32_t chunk_size;
  unsigned char tag[AES_TAG_SIZE];
  unsigned char data[BUFFER_SIZE];
} FileDataMessage;
#pragma pack(pop)


/**
 * Send a protocol message over a socket.
 * 
 * @param sock The connected TCP socket.
 * @param type The type of message to send.
 * @param data Buffer containing message payload data.
 * @param length Length of the payload data.
 * @return 0 on success, non-zero on error.
 */
int send_message(SOCKET sock, MessageType type, const void *data, uint32_t length);

/**
 * Receive a protocol message from a socket.
 * 
 * @param sock The connected TCP socket.
 * @param type Pointer to store the received message type.
 * @param data Buffer to store the received message payload.
 * @param length Pointer that receives length on success, input defines max capacity.
 * @return 0 on success, non-zero on error.
 */
int recv_message(SOCKET sock, MessageType *type, void *data, uint32_t *length);

/**
 * Broadcast the server presence over UDP.
 * 
 * @param sock The UDP broadcast socket.
 * @param server_name Name of the server.
 * @return 0 on success, non-zero on error.
 */
int send_broadcast(SOCKET sock, const char *server_name);

/**
 * Receive a broadcast presence message over UDP.
 * 
 * @param sock The UDP socket to listen on.
 * @param msg Buffer to store the received broadcast message.
 * @param sender Address structure to store sender information.
 * @return 0 on success, non-zero on error.
 */
int recv_broadcast(SOCKET sock, BroadcastMessage *msg, struct sockaddr_in *sender);

/**
 * Send exactly length bytes to socket.
 * 
 * @param sock The destination socket.
 * @param data Pointer to the buffer to send.
 * @param length The exact number of bytes to send.
 * @return length on success, < 0 on error.
 */
int send_all(SOCKET sock, const void *data, uint32_t length);

/**
 * Receive exactly length bytes from socket.
 * 
 * @param sock The source socket.
 * @param data Pointer to the buffer to receive data.
 * @param length The exact number of bytes to receive.
 * @return length on success, < 0 on error.
 */
int recv_all(SOCKET sock, void *data, uint32_t length);

#endif
