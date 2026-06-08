/*
 * MINI - file.h
 * File I/O operations for loading and saving text files.
 */

#ifndef MINI_FILE_H
#define MINI_FILE_H

#include "buffer.h"

/* Load file contents into buffer. Returns 0 on success, -1 on error. */
int file_load(Buffer *buf, const char *path);

/* Save buffer contents to file. Returns 0 on success, -1 on error. */
int file_save(Buffer *buf);

/* Check if file exists. Returns 1 if yes, 0 if no, -1 on error. */
int file_exists(const char *path);

#endif /* MINI_FILE_H */
