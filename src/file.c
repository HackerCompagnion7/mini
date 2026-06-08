/*
 * MINI - file.c
 * File I/O operations.
 */

#define _POSIX_C_SOURCE 200809L
#include "file.h"
#include "buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int file_exists(const char *path)
{
    if (!path) return -1;
    FILE *f = fopen(path, "r");
    if (!f) {
        if (errno == ENOENT) return 0;
        return -1; /* other error */
    }
    fclose(f);
    return 1;
}

int file_load(Buffer *buf, const char *path)
{
    if (!buf || !path) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    /* Get file size */
    if (fseek(f, 0, SEEK_END) < 0) {
        fclose(f);
        return -1;
    }
    long fsize = ftell(f);
    if (fsize < 0) {
        fclose(f);
        return -1;
    }
    rewind(f);

    /* Free existing buffer content (also for empty files: H-3 fix) */
    for (size_t i = 0; i < buf->count; i++) {
        line_free(&buf->lines[i]);
    }
    free(buf->lines);
    buf->lines = NULL;
    buf->count = 0;
    buf->cap = 0;

    /* Empty file: create single empty line */
    if (fsize == 0) {
        fclose(f);
        buf->cap = 64;
        buf->lines = malloc(buf->cap * sizeof(Line));
        if (!buf->lines) return -1;
        if (line_init(&buf->lines[0]) < 0) {
            free(buf->lines);
            buf->lines = NULL;
            buf->cap = 0;
            return -1;
        }
        buf->count = 1;
        free(buf->filepath);
        buf->filepath = strdup(path);
        buf->modified = false;
        return 0;
    }

    /* Read entire file */
    char *content = malloc((size_t)fsize + 1);
    if (!content) {
        fclose(f);
        return -1;
    }

    size_t nread = fread(content, 1, (size_t)fsize, f);
    fclose(f);

    if ((long)nread != fsize) {
        free(content);
        return -1;
    }
    content[nread] = '\0';

    /* Count lines to pre-allocate */
    size_t line_count = 1;
    for (size_t i = 0; i < nread; i++) {
        if (content[i] == '\n') line_count++;
    }

    buf->cap = line_count + 16;
    buf->lines = malloc(buf->cap * sizeof(Line));
    if (!buf->lines) {
        free(content);
        return -1;
    }
    buf->count = 0;

    /* Split into lines */
    size_t start = 0;
    for (size_t i = 0; i <= nread; i++) {
        if (i == nread || content[i] == '\n') {
            size_t line_len = i - start;

            /* Handle \r\n: strip the \r */
            if (line_len > 0 && content[start + line_len - 1] == '\r') {
                line_len--;
            }

            /* Skip trailing empty line from final newline */
            if (i == nread && line_len == 0 && buf->count > 0) {
                start = i + 1;
                continue;
            }

            Line ln;
            ln.len = line_len;  /* M-1: initialize len before use */
            ln.cap = line_len + 1;
            if (ln.cap < 16) ln.cap = 16;
            ln.data = malloc(ln.cap);
            if (!ln.data) {
                free(content);
                /* M-7: full cleanup on error */
                for (size_t j = 0; j < buf->count; j++) {
                    line_free(&buf->lines[j]);
                }
                free(buf->lines);
                buf->lines = NULL;
                buf->count = 0;
                buf->cap = 0;
                return -1;
            }
            memcpy(ln.data, content + start, line_len);
            ln.data[line_len] = '\0';

            buf->lines[buf->count++] = ln;
            start = i + 1;
        }
    }

    /* Ensure at least one line */
    if (buf->count == 0) {
        if (line_init(&buf->lines[0]) < 0) {
            free(content);
            return -1;
        }
        buf->count = 1;
    }

    free(content);

    /* Set filepath */
    free(buf->filepath);
    buf->filepath = strdup(path);
    buf->modified = false;

    return 0;
}

int file_save(Buffer *buf)
{
    if (!buf || !buf->filepath) return -1;

    /* C-3: Atomic save — write to temp file, then rename.
     * This prevents data loss if write fails mid-operation. */
    size_t pathlen = strlen(buf->filepath);
    char *tmp_path = malloc(pathlen + 8);
    if (!tmp_path) return -1;
    memcpy(tmp_path, buf->filepath, pathlen);
    memcpy(tmp_path + pathlen, ".XXXXXX", 8); /* includes null */

    int fd = mkstemp(tmp_path);
    if (fd < 0) {
        free(tmp_path);
        return -1;
    }

    FILE *f = fdopen(fd, "w");
    if (!f) {
        close(fd);
        unlink(tmp_path);
        free(tmp_path);
        return -1;
    }

    for (size_t i = 0; i < buf->count; i++) {
        Line *ln = &buf->lines[i];
        if (ln->len > 0) {
            if (fwrite(ln->data, 1, ln->len, f) != ln->len) {
                fclose(f);
                unlink(tmp_path);
                free(tmp_path);
                return -1;
            }
        }
        /* Write newline after every line except last if empty */
        if (i < buf->count - 1 || ln->len > 0) {
            if (fputc('\n', f) == EOF) {
                fclose(f);
                unlink(tmp_path);
                free(tmp_path);
                return -1;
            }
        }
    }

    if (fclose(f) != 0) {
        unlink(tmp_path);
        free(tmp_path);
        return -1;
    }

    /* Atomic rename */
    if (rename(tmp_path, buf->filepath) != 0) {
        unlink(tmp_path);
        free(tmp_path);
        return -1;
    }

    free(tmp_path);
    buf->modified = false;
    return 0;
}
