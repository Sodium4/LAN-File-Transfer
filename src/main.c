#include "common.h"
#include "crypto.h"
#include "client.h"
#include "server.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <server|client> [args...]\n", argv[0]);
        return 1;
    }

    if (init_winsock() != 0) {
        return 1;
    }

    if (crypto_init() != 0) {
        cleanup_winsock();
        return 1;
    }

    int result = 1;
    if (strcmp(argv[1], "server") == 0) {
        result = run_server(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "client") == 0) {
        result = run_client(argc - 1, argv + 1);
    } else {
        printf("Unknown mode: %s\n", argv[1]);
        printf("Usage: %s <server|client> [args...]\n", argv[0]);
    }

    crypto_cleanup();
    cleanup_winsock();

    return result;
}
