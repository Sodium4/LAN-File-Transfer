CC = gcc
CFLAGS = -Wall -Wextra -I./include

ifeq ($(OS),Windows_NT)
	SHELL = cmd.exe
	LDFLAGS = -lssl -lcrypto -lws2_32
	LDFLAGS_STATIC = -static -lssl -lcrypto -lws2_32 -lgdi32 -lcrypt32
	EXE_EXT = .exe
	MKDIR_CMD = @if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	RM_CMD = if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)
else
	LDFLAGS = -lssl -lcrypto -lpthread
	LDFLAGS_STATIC = -static -lssl -lcrypto -lpthread -lz -lzstd -ldl
	EXE_EXT = 
	MKDIR_CMD = mkdir -p $(BUILD_DIR)
	RM_CMD = rm -rf $(BUILD_DIR)
endif

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# Source files
COMMON_SRCS = $(SRC_DIR)/common.c $(SRC_DIR)/crypto.c $(SRC_DIR)/protocol.c $(SRC_DIR)/network.c
APP_SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/server.c $(SRC_DIR)/client.c $(SRC_DIR)/filebrowser.c $(COMMON_SRCS)

# Targets
APP = $(BUILD_DIR)/filetransfer$(EXE_EXT)
APP_STATIC = $(BUILD_DIR)/filetransfer_static$(EXE_EXT)

.PHONY: all clean dirs static nonstatic

all: dirs $(APP) $(APP_STATIC)

nonstatic: dirs $(APP)

static: dirs $(APP_STATIC)

dirs:
	$(MKDIR_CMD)

$(APP): $(APP_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(APP_STATIC): $(APP_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_STATIC)

clean:
	-$(RM_CMD)
