#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <inttypes.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <conio.h>
#include <direct.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <io.h>

#define PATH_SEP '\\'
#define PATH_SEP_STR "\\"
typedef int socklen_t;

#else
// POSIX includes
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <termios.h>
#include <errno.h>
#include <pthread.h>
#include <netdb.h>

// POSIX network abstractions
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#define WSAGetLastError() errno

#define PATH_SEP '/'
#define PATH_SEP_STR "/"

#endif

// Network ports
#define BROADCAST_PORT 9999
#define TRANSFER_PORT 9998

// Buffer sizes
#define BUFFER_SIZE 8192
#define MAX_PATH_LEN 2048
#define MAX_FILENAME_LEN 1024
#define VERIFICATION_CODE_LEN 6

// Broadcast packet tag
#define BROADCAST_TAG "FTRANSFER"


typedef enum { LOG_INFO, LOG_WARN, LOG_ERROR } LogLevel;



#ifdef _WIN32
/**
 * Convert a UTF-8 path to a wide character path with the "\\?\" prefix.
 *
 * The returned pointer must be freed by the caller.
 * 
 * @param path The UTF-8 path.
 * @return Allocated wide string, or NULL on error.
 */
wchar_t* get_wide_path(const char* path);
#endif

/**
 * Initialize Winsock.
 * 
 * @return 0 on success, non-zero on failure.
 */
int init_winsock(void);

/**
 * Cleanup Winsock resources.
 */
void cleanup_winsock(void);

/**
 * Log a message to the console.
 * 
 * @param level The log level (LOG_INFO, LOG_WARN, LOG_ERROR).
 * @param format Format string, similar to printf.
 * @param ... Additional arguments.
 */
void log_message(LogLevel level, const char *format, ...);

/**
 * Get the current timestamp string.
 * 
 * @param buffer Output buffer for the timestamp.
 * @param size Size of the output buffer.
 */
void get_timestamp(char *buffer, size_t size);

/**
 * Create a directory recursively.
 * 
 * @param path The path of the directory to create.
 * @return 0 on success, non-zero on failure.
 */
int create_directory_recursive(const char *path);

#endif // COMMON_H
