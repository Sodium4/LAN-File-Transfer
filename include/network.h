#ifndef NETWORK_H
#define NETWORK_H

#include "common.h"


/**
 * Create a UDP socket.
 * 
 * @return The created socket or INVALID_SOCKET on failure.
 */
SOCKET create_udp_socket(void);

/**
 * Create a TCP socket.
 * 
 * @return The created socket or INVALID_SOCKET on failure.
 */
SOCKET create_tcp_socket(void);

/**
 * Enable the SO_BROADCAST option on a socket.
 * 
 * @param sock The socket.
 * @return 0 on success, non-zero on error.
 */
int enable_broadcast(SOCKET sock);

/**
 * Enable the SO_REUSEADDR option on a socket.
 * 
 * @param sock The socket.
 * @return 0 on success, non-zero on error.
 */
int enable_reuse_addr(SOCKET sock);

/**
 * Bind a socket to a given port on all interfaces.
 * 
 * @param sock The socket.
 * @param port The port to bind to.
 * @return 0 on success, non-zero on error.
 */
int bind_socket(SOCKET sock, uint16_t port);

/**
 * Set send and receive timeout on a socket.
 * 
 * @param sock The socket.
 * @param timeout_ms Timeout value in milliseconds.
 * @return 0 on success, non-zero on error.
 */
int set_socket_timeout(SOCKET sock, int timeout_ms);

/**
 * Get the local IP address string.
 * 
 * @return A statically allocated string containing the local IP address.
 */
char *get_local_ip(void);

#endif
