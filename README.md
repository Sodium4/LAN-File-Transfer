# Secure File Transfer System

A cross-platform, encrypted file transfer application for sending files between machines on a local network. The server advertises itself via UDP broadcast, and clients auto-discover it — no manual IP entry required. All file data is encrypted end-to-end using **ECDH key exchange** and **AES-256-GCM**.

## Features

- **Zero-configuration discovery** — The server broadcasts its presence over UDP so clients find it automatically.
- **End-to-end encryption** — ECDH (Elliptic Curve Diffie-Hellman) key agreement followed by AES-256-GCM authenticated encryption for every file chunk.
- **Verification codes** — A 6-digit code displayed on the server must be entered on the client before any files are transferred, preventing unauthorized connections.
- **Interactive file browser** — A built-in terminal UI lets you navigate directories, select files and folders, and queue them for transfer.
- **Recursive directory transfer** — Select entire folders; the directory structure is preserved on the receiving end.
- **Cross-platform** — Builds and runs on both Windows and Linux/macOS (POSIX).
- **Unicode path support** — On Windows, long and Unicode file paths are handled via wide-character APIs with the `\\?\` prefix.

## Prerequisites

| Dependency | Notes |
|---|---|
| **GCC** (or compatible C compiler) | Used by the Makefile |
| **OpenSSL** (libssl + libcrypto) | Provides ECDH, AES-256-GCM, and CSPRNG |
| **Make** | GNU Make or NMAKE on Windows |
| **Winsock2** (Windows only) | Included with Windows SDK, linked automatically |
| **pthreads** (Linux/macOS only) | Linked automatically on POSIX systems |

## Building

```bash
# Build both dynamically-linked and static binaries
make

# Build only the dynamically-linked binary
make nonstatic

# Build only the static binary
make static

# Clean build artifacts
make clean
```

Output binaries are placed in the `build/` directory:

| Binary | Description |
|---|---|
| `filetransfer` (`.exe` on Windows) | Dynamically linked against OpenSSL |
| `filetransfer_static` (`.exe` on Windows) | Statically linked — portable, no runtime dependencies |

## Usage

```
filetransfer <server|client> [options]
```

The first argument selects the **mode** — either `server` or `client`.

---

### Server Mode

Start receiving files:

```bash
filetransfer server [-d <directory>]
```

| Option | Argument | Description | Default |
|---|---|---|---|
| `-d` | `<directory>` | Directory where received files are saved | `./received` |
| `-h`, `--help` | — | Print usage instructions and exit | — |

On startup, the server displays its IP address, listening port, and a **6-digit verification code** that the connecting client must enter.

Each transfer session creates a timestamped subdirectory (e.g. `received/transfer_20260522_141300/`) to keep files organized. After a client disconnects, a new verification code is generated automatically.

**Example:**

```bash
# Save incoming files to a custom directory
filetransfer server -d C:\Downloads\incoming
```

---

### Client Mode

Send files to a server:

```bash
filetransfer client
```

The client does not accept any additional command-line arguments — the entire workflow is interactive:

1. **Auto-discovery** — Listens for server UDP broadcasts for 5 seconds and displays all found servers.
2. **Server selection** — Pick a server from the list.
3. **Verification** — Enter the 6-digit code shown on the server's console.
4. **File browser** — Navigate and select files/folders to send.
5. **Transfer** — Selected files are encrypted and streamed to the server with real-time progress.

#### File Browser Controls

| Key | Action |
|---|---|
| `↑` / `↓` | Navigate up/down |
| `Space` | Toggle selection on the current item |
| `Enter` | Open a directory, or confirm and begin transfer (when files are selected) |
| `Q` | Finish selection and send (or cancel if nothing selected) |

## Network Details

| Parameter | Value |
|---|---|
| **Discovery (UDP broadcast)** | Port `9999` |
| **File transfer (TCP)** | Port `9998` |
| **Broadcast tag** | `FTRANSFER` |
| **Chunk size** | 8 KB (`BUFFER_SIZE = 8192`) |

> **Note:** Both the server and client must be on the same local network (broadcast domain) for auto-discovery to work.

## Security Model

```
Client                                      Server
  │                                            │
  │◄──────── ECDH Public Key + IV ─────────────│  Server generates EC keypair + random IV
  │                                            │
  │────────── ECDH Public Key ────────────────►│  Client generates EC keypair
  │                                            │
  │  (Both sides derive shared AES-256 key)    │
  │                                            │
  │◄──────── Verification Request ─────────────│
  │────────── 6-digit Code ───────────────────►│  User types the code from server console
  │◄──────── Verification OK/Fail ─────────────│
  │                                            │
  │────────── Encrypted File Chunks ──────────►│  AES-256-GCM with auth tags
  │────────── Session End ────────────────────►│
```

- **Key exchange:** ECDH ensures the AES key is never sent over the wire.
- **Authenticated encryption:** AES-256-GCM provides both confidentiality and integrity — tampered chunks are detected and rejected.
- **Verification code:** Prevents unauthorized clients from connecting, acting as a simple out-of-band authentication step.

## Project Structure

```
File-Transfer-System/
├── include/
│   ├── client.h          # Client entry point declaration
│   ├── server.h          # Server entry point declaration
│   ├── common.h          # Shared types, constants, platform abstractions
│   ├── crypto.h          # ECDH + AES-256-GCM encryption interface
│   ├── filebrowser.h     # Interactive terminal file browser
│   ├── network.h         # Socket creation and utility functions
│   └── protocol.h        # Wire protocol — message types and serialization
├── src/
│   ├── main.c            # Entry point — dispatches to server or client
│   ├── server.c          # Server logic — broadcast, verify, receive files
│   ├── client.c          # Client logic — discover, verify, send files
│   ├── common.c          # Logging, timestamps, directory creation, Winsock init
│   ├── crypto.c          # OpenSSL ECDH key exchange and AES-256-GCM encrypt/decrypt
│   ├── filebrowser.c     # Terminal UI file browser with keyboard navigation
│   ├── network.c         # Socket helpers (TCP, UDP, broadcast, timeouts)
│   └── protocol.c        # Message framing — send/recv with headers
├── build/                # Compiled binaries (created by make)
├── received/             # Default directory for received files
├── Makefile              # Build system (supports Windows + POSIX)
└── README.md
```

## License

This project is licensed under the Apache License 2.0. See the LICENSE file for details.
