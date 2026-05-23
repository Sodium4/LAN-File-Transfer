#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#include "common.h"

#define MAX_FILES 256

// File entry
typedef struct {
  char name[MAX_FILENAME_LEN];
  char full_path[MAX_PATH_LEN];
  int is_directory;
  uint64_t size;
  int selected;
} FileEntry;

// File browser state
typedef struct {
  char current_path[MAX_PATH_LEN];
  FileEntry entries[MAX_FILES];
  int entry_count;
  int cursor;
  int scroll_offset;
  int visible_rows;
  FileEntry *selected_files;
  int selected_count;
} FileBrowser;


/**
 * Initialize the file browser with a starting directory path.
 * 
 * @param fb File browser state structure.
 * @param start_path The initial directory path.
 * @return 0 on success, non-zero on failure.
 */
int filebrowser_init(FileBrowser *fb, const char *start_path);

/**
 * Cleanup the file browser resources.
 * 
 * @param fb File browser state structure.
 */
void filebrowser_cleanup(FileBrowser *fb);

/**
 * Refresh the current directory entries in the file browser.
 * 
 * @param fb File browser state structure.
 * @return 0 on success, non-zero on failure.
 */
int filebrowser_refresh(FileBrowser *fb);

/**
 * Run the interactive file browser interface loop.
 * 
 * @param fb File browser state structure.
 * @return 0 on completing execution or user cancellation, < 0 on error.
 */
int filebrowser_run(FileBrowser *fb);

/**
 * Render the current file browser state to the console.
 * 
 * @param fb File browser state structure.
 */
void filebrowser_render(FileBrowser *fb);

#endif
