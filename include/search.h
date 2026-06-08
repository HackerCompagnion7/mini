/*
 * MINI - search.h
 * Text search within the buffer.
 */

#ifndef MINI_SEARCH_H
#define MINI_SEARCH_H

#include "buffer.h"

/* Search result */
typedef struct {
    size_t line;     /* line index where match was found */
    size_t byte_pos; /* byte position within the line */
} SearchMatch;

/* Search direction */
typedef enum {
    SEARCH_FORWARD,
    SEARCH_BACKWARD
} SearchDir;

/*
 * Search for `pattern` in `buf` starting from (start_line, start_byte).
 * Returns 1 if found (result stored in `match`), 0 if not found.
 */
int search_find(Buffer *buf, const char *pattern,
                size_t start_line, size_t start_byte,
                SearchDir dir, SearchMatch *match);

/*
 * Find next match after current position.
 * Wraps around the buffer if `wrap` is true.
 */
int search_find_next(Buffer *buf, const char *pattern,
                     size_t cur_line, size_t cur_byte,
                     SearchDir dir, int wrap, SearchMatch *match);

#endif /* MINI_SEARCH_H */
