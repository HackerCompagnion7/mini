/*
 * MINI - buffer.c
 * Line-based text buffer with UTF-8 support.
 */

#define _POSIX_C_SOURCE 200809L
#include "buffer.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- Internal helpers ---- */

int line_init(Line *ln)
{
    ln->len = 0;   /* M-1: initialize len before malloc check */
    ln->cap = 64;
    ln->data = malloc(ln->cap);
    if (!ln->data) return -1;
    ln->data[0] = '\0';
    return 0;
}

static int line_ensure_cap(Line *ln, size_t needed)
{
    if (needed <= ln->cap) return 0;
    size_t new_cap = ln->cap;
    while (new_cap < needed) new_cap *= 2;
    char *new_data = realloc(ln->data, new_cap);
    if (!new_data) return -1;
    ln->data = new_data;
    ln->cap = new_cap;
    return 0;
}

void line_free(Line *ln)
{
    free(ln->data);
    ln->data = NULL;
    ln->len = 0;
    ln->cap = 0;
}

static int buffer_ensure_cap(Buffer *buf, size_t needed)
{
    if (needed <= buf->cap) return 0;
    size_t new_cap = buf->cap;
    while (new_cap < needed) new_cap *= 2;
    Line *new_lines = realloc(buf->lines, new_cap * sizeof(Line));
    if (!new_lines) return -1;
    buf->lines = new_lines;
    buf->cap = new_cap;
    return 0;
}

/* ---- Public API ---- */

Buffer buffer_create(void)
{
    Buffer buf;
    buf.lines = NULL;
    buf.count = 0;
    buf.cap = 0;
    buf.modified = false;
    buf.filepath = NULL;

    buf.cap = 64;
    buf.lines = malloc(buf.cap * sizeof(Line));
    if (!buf.lines) {
        buf.cap = 0;
        return buf;
    }

    /* Start with one empty line */
    if (line_init(&buf.lines[0]) < 0) {
        free(buf.lines);
        buf.lines = NULL;
        buf.cap = 0;
        return buf;
    }
    buf.count = 1;

    return buf;
}

void buffer_destroy(Buffer *buf)
{
    if (!buf) return;
    for (size_t i = 0; i < buf->count; i++) {
        line_free(&buf->lines[i]);
    }
    free(buf->lines);
    free(buf->filepath);
    buf->lines = NULL;
    buf->count = 0;
    buf->cap = 0;
    buf->filepath = NULL;
}

Line *buffer_get_line(Buffer *buf, size_t index)
{
    if (!buf || index >= buf->count) return NULL;
    return &buf->lines[index];
}

int buffer_insert_line(Buffer *buf, size_t at)
{
    if (!buf) return -1;
    if (at > buf->count) at = buf->count;

    if (buffer_ensure_cap(buf, buf->count + 1) < 0) return -1;

    /* Shift lines down */
    memmove(&buf->lines[at + 1], &buf->lines[at],
            (buf->count - at) * sizeof(Line));

    if (line_init(&buf->lines[at]) < 0) {
        /* H-5: rollback memmove on line_init failure */
        memmove(&buf->lines[at], &buf->lines[at + 1],
                (buf->count - at) * sizeof(Line));
        return -1;
    }

    buf->count++;
    buf->modified = true;
    return 0;
}

int buffer_delete_line(Buffer *buf, size_t at)
{
    if (!buf || at >= buf->count || buf->count <= 1) return -1;

    line_free(&buf->lines[at]);

    /* Shift lines up */
    memmove(&buf->lines[at], &buf->lines[at + 1],
            (buf->count - at - 1) * sizeof(Line));

    buf->count--;
    buf->modified = true;
    return 0;
}

int line_insert_char(Line *ln, size_t byte_pos, const char *utf8_char, size_t char_len)
{
    if (!ln || !utf8_char || char_len == 0) return -1;
    if (byte_pos > ln->len) byte_pos = ln->len;

    if (line_ensure_cap(ln, ln->len + char_len + 1) < 0) return -1;

    /* Shift existing bytes right */
    memmove(ln->data + byte_pos + char_len, ln->data + byte_pos,
            ln->len - byte_pos + 1); /* +1 for null terminator */

    memcpy(ln->data + byte_pos, utf8_char, char_len);
    ln->len += char_len;

    return 0;
}

int line_delete_char_range(Line *ln, size_t byte_pos, size_t byte_len)
{
    if (!ln || byte_pos >= ln->len) return -1;
    if (byte_pos + byte_len > ln->len) byte_len = ln->len - byte_pos;

    memmove(ln->data + byte_pos, ln->data + byte_pos + byte_len,
            ln->len - byte_pos - byte_len + 1); /* +1 for null */
    ln->len -= byte_len;

    return 0;
}

int line_append_str(Line *ln, const char *str, size_t len)
{
    if (!ln || !str || len == 0) return -1;

    if (line_ensure_cap(ln, ln->len + len + 1) < 0) return -1;

    memcpy(ln->data + ln->len, str, len);
    ln->len += len;
    ln->data[ln->len] = '\0';

    return 0;
}

/* ---- UTF-8 helpers ---- */

size_t utf8_char_len(unsigned char byte)
{
    if (byte < 0x80)        return 1;
    else if ((byte & 0xE0) == 0xC0) return 2;
    else if ((byte & 0xF0) == 0xE0) return 3;
    else if ((byte & 0xF8) == 0xF0) return 4;
    else return 1; /* invalid byte, treat as single byte */
}

size_t utf8_prev_char_pos(const char *data, size_t byte_pos)
{
    if (!data || byte_pos == 0) return 0;

    size_t pos = byte_pos - 1;
    /* Walk backwards past continuation bytes (10xxxxxx) */
    while (pos > 0 && ((unsigned char)data[pos] & 0xC0) == 0x80) {
        pos--;
    }
    return pos;
}

size_t utf8_next_char_pos(const char *data, size_t byte_pos, size_t data_len)
{
    if (!data || byte_pos >= data_len) return data_len;

    size_t clen = utf8_char_len((unsigned char)data[byte_pos]);
    size_t next = byte_pos + clen;

    /* Verify we don't exceed bounds */
    if (next > data_len) return data_len;
    return next;
}

size_t utf8_char_count(const char *data, size_t byte_len)
{
    if (!data) return 0;
    size_t count = 0;
    size_t i = 0;
    while (i < byte_len) {
        size_t clen = utf8_char_len((unsigned char)data[i]);
        /* Validate continuation bytes */
        int valid = 1;
        for (size_t j = 1; j < clen && i + j < byte_len; j++) {
            if (((unsigned char)data[i + j] & 0xC0) != 0x80) {
                valid = 0;
                break;
            }
        }
        if (valid && i + clen <= byte_len) {
            count++;
            i += clen;
        } else {
            count++;
            i++;
        }
    }
    return count;
}

size_t utf8_display_width(const char *data, size_t byte_len)
{
    /* For now, treat each codepoint as width 1.
     * Tab is counted as 1 column (editor will handle tab→spaces later).
     * Full CJK width support could be added later.
     */
    return utf8_char_count(data, byte_len);
}
