/*
 * MINI - search.c
 * Text search within the buffer.
 */

#define _POSIX_C_SOURCE 200809L
#include "search.h"

#include <string.h>

/* ---- Portable memmem implementation ---- */

static const char *mini_memmem(const char *haystack, size_t hay_len,
                               const char *needle, size_t needle_len)
{
    if (needle_len == 0) return haystack;
    if (needle_len > hay_len) return NULL;

    /* Simple two-way search (adequate for editor-scale text) */
    for (size_t i = 0; i <= hay_len - needle_len; i++) {
        if (haystack[i] == needle[0] &&
            memcmp(haystack + i, needle, needle_len) == 0) {
            return haystack + i;
        }
    }
    return NULL;
}

/* ---- Public API ---- */

int search_find(Buffer *buf, const char *pattern,
                size_t start_line, size_t start_byte,
                SearchDir dir, SearchMatch *match)
{
    if (!buf || !pattern || !match || pattern[0] == '\0') return 0;

    size_t plen = strlen(pattern);

    if (dir == SEARCH_FORWARD) {
        for (size_t li = start_line; li < buf->count; li++) {
            Line *ln = &buf->lines[li];
            size_t start = (li == start_line) ? start_byte : 0;

            if (ln->len >= plen && start <= ln->len - plen) {
                const char *haystack = ln->data + start;
                size_t hay_len = ln->len - start;
                const char *found = mini_memmem(haystack, hay_len, pattern, plen);
                if (found) {
                    match->line = li;
                    match->byte_pos = (size_t)(found - ln->data);
                    return 1;
                }
            }
        }
    } else {
        /* Search backward */
        for (size_t li = start_line + 1; li > 0; li--) {
            size_t actual_li = li - 1;
            Line *ln = &buf->lines[actual_li];

            size_t search_end;
            if (actual_li == start_line) {
                search_end = (start_byte > 0) ? start_byte : 0;
            } else {
                search_end = ln->len;
            }

            if (ln->len < plen) continue;

            /* Find last occurrence within search range */
            size_t last_found = (size_t)-1;
            size_t pos = 0;
            while (pos + plen <= search_end) {
                const char *found = mini_memmem(ln->data + pos, search_end - pos,
                                                pattern, plen);
                if (!found) break;
                last_found = (size_t)(found - ln->data);
                pos = last_found + 1;
            }

            if (last_found != (size_t)-1) {
                match->line = actual_li;
                match->byte_pos = last_found;
                return 1;
            }
        }
    }

    return 0;
}

int search_find_next(Buffer *buf, const char *pattern,
                     size_t cur_line, size_t cur_byte,
                     SearchDir dir, int wrap, SearchMatch *match)
{
    if (!buf || !pattern || !match || pattern[0] == '\0') return 0;
    /* H-4: bounds check */
    if (cur_line >= buf->count) return 0;

    if (dir == SEARCH_FORWARD) {
        size_t start_byte = cur_byte + strlen(pattern);
        if (start_byte <= buf->lines[cur_line].len) {
            if (search_find(buf, pattern, cur_line, start_byte, dir, match)) {
                return 1;
            }
        }
        if (cur_line + 1 < buf->count) {
            if (search_find(buf, pattern, cur_line + 1, 0, dir, match)) {
                return 1;
            }
        }
        if (wrap && buf->count > 0) {
            if (search_find(buf, pattern, 0, 0, dir, match)) {
                return 1;
            }
        }
    } else {
        if (cur_byte > 0) {
            if (search_find(buf, pattern, cur_line, 0, dir, match)) {
                if (match->line < cur_line ||
                    (match->line == cur_line && match->byte_pos < cur_byte)) {
                    return 1;
                }
            }
        }
        if (cur_line > 0) {
            if (search_find(buf, pattern, cur_line - 1, 0, dir, match)) {
                return 1;
            }
        }
        if (wrap && buf->count > 0) {
            size_t last = buf->count - 1;
            size_t last_byte = buf->lines[last].len;
            if (search_find(buf, pattern, last, last_byte, dir, match)) {
                return 1;
            }
        }
    }

    return 0;
}
