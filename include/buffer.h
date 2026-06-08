/*
 * MINI - buffer.h
 * Line-based text buffer with UTF-8 support.
 *
 * Storage: dynamic array of lines (heap-allocated strings).
 * Each line is a null-terminated UTF-8 string without the newline character.
 */

#ifndef MINI_BUFFER_H
#define MINI_BUFFER_H

#include <stddef.h>
#include <stdbool.h>

/* A single line in the buffer */
typedef struct {
    char *data;       /* UTF-8 string, null-terminated (no trailing \n) */
    size_t len;       /* byte length (not counting null terminator) */
    size_t cap;       /* allocated capacity in bytes */
} Line;

/* The text buffer */
typedef struct {
    Line  *lines;     /* dynamic array of lines */
    size_t count;     /* number of lines */
    size_t cap;       /* capacity of lines array */
    bool   modified;  /* dirty flag for save tracking */
    char  *filepath;  /* associated file path (may be NULL for new buffer) */
} Buffer;

/* Line lifecycle (used by file.c for bulk construction) */
int  line_init(Line *ln);
void line_free(Line *ln);

/* Lifecycle */
Buffer buffer_create(void);
void   buffer_destroy(Buffer *buf);

/* Line operations */
int    buffer_insert_line(Buffer *buf, size_t at);
int    buffer_delete_line(Buffer *buf, size_t at);
Line  *buffer_get_line(Buffer *buf, size_t index);

/* Character operations within a line */
int    line_insert_char(Line *ln, size_t byte_pos, const char *utf8_char, size_t char_len);
int    line_delete_char_range(Line *ln, size_t byte_pos, size_t byte_len);
int    line_append_str(Line *ln, const char *str, size_t len);

/* UTF-8 helpers */
size_t utf8_char_len(unsigned char byte);
size_t utf8_prev_char_pos(const char *data, size_t byte_pos);
size_t utf8_next_char_pos(const char *data, size_t byte_pos, size_t data_len);
size_t utf8_display_width(const char *data, size_t byte_len);
size_t utf8_char_count(const char *data, size_t byte_len);

#endif /* MINI_BUFFER_H */
