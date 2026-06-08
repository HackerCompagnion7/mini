/*
 * MINI - editor.c
 * Main editor state and operations.
 */

#define _POSIX_C_SOURCE 200809L
#include "editor.h"
#include "file.h"
#include "search.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- Internal helpers ---- */

/* Calculate display column (character count) from byte position */
size_t byte_to_display_col(const char *data, size_t byte_pos)
{
    return utf8_char_count(data, byte_pos);
}

/* Calculate byte position from display column */
static size_t display_col_to_byte(const char *data, size_t data_len, int col)
{
    size_t byte_i = 0;
    int display = 0;

    while (byte_i < data_len && display < col) {
        size_t clen = utf8_char_len((unsigned char)data[byte_i]);
        if (byte_i + clen > data_len) break;

        /* Validate continuation bytes */
        int valid = 1;
        for (size_t j = 1; j < clen && byte_i + j < data_len; j++) {
            if (((unsigned char)data[byte_i + j] & 0xC0) != 0x80) {
                valid = 0;
                break;
            }
        }

        if (valid) {
            byte_i += clen;
        } else {
            byte_i++;
        }
        display++;
    }

    return byte_i;
}

/* Calculate dynamic line number width (3 to 6 chars) */
int editor_num_width(size_t line_count)
{
    if (line_count < 10)    return 3;   /* "1  " */
    if (line_count < 100)   return 4;   /* " 1  " */
    if (line_count < 1000)  return 4;   /* " 99 " */
    if (line_count < 10000) return 5;   /* "9999 " */
    return 6;                           /* "99999 " */
}

/* Clamp cursor to valid positions */
static void editor_clamp_cursor(Editor *ed)
{
    if (!ed) return;
    if (ed->buf.count == 0) return;
    if (ed->cur_line >= ed->buf.count) {
        ed->cur_line = ed->buf.count - 1;
    }
    Line *ln = buffer_get_line(&ed->buf, ed->cur_line);
    if (!ln) return;
    if (ed->cur_byte > ln->len) {
        ed->cur_byte = ln->len;
    }
}

/* Adjust scroll offsets to keep cursor visible */
static void editor_scroll(Editor *ed)
{
    if (!ed) return;
    int rows, cols;
    term_get_size(&rows, &cols);
    (void)cols;

    int num_width = editor_num_width(ed->buf.count);
    int text_cols = cols - num_width;

    Line *ln = buffer_get_line(&ed->buf, ed->cur_line);
    if (!ln) return;

    int cur_display = (int)byte_to_display_col(ln->data, ed->cur_byte);

    /* Vertical scrolling: 3 rows reserved for status + error bar */
    if ((int)ed->cur_line < ed->scr.row_off) {
        ed->scr.row_off = (int)ed->cur_line;
    }
    if ((int)ed->cur_line >= ed->scr.row_off + rows - 3) {
        ed->scr.row_off = (int)ed->cur_line - rows + 4;
    }

    /* Horizontal scrolling */
    if (cur_display < ed->scr.col_off) {
        ed->scr.col_off = cur_display;
    }
    if (cur_display >= ed->scr.col_off + text_cols) {
        ed->scr.col_off = cur_display - text_cols + 1;
    }
    if (ed->scr.col_off < 0) ed->scr.col_off = 0;
}

/* ---- Public API ---- */

int editor_init(Editor *ed, const char *filepath)
{
    if (!ed) return -1;

    memset(ed, 0, sizeof(Editor));
    ed->buf = buffer_create();
    if (!ed->buf.lines) return -1;

    ed->running = 1;
    ed->search_query = NULL;
    ed->syntax_dirty = 1;

    if (filepath) {
        if (file_load(&ed->buf, filepath) < 0) {
            snprintf(ed->status_msg, sizeof(ed->status_msg),
                     "New file: %s", filepath);
            free(ed->buf.filepath);
            ed->buf.filepath = strdup(filepath);
        } else {
            snprintf(ed->status_msg, sizeof(ed->status_msg),
                     "Loaded: %s", filepath);
        }
    } else {
        snprintf(ed->status_msg, sizeof(ed->status_msg),
                 "MINI - New file");
    }

    return 0;
}

void editor_destroy(Editor *ed)
{
    if (!ed) return;
    buffer_destroy(&ed->buf);
    free(ed->search_query);
    ed->search_query = NULL;
    syntax_result_free(&ed->syntax);
}

void editor_syntax_update(Editor *ed)
{
    if (!ed || !ed->syntax_dirty) return;
    syntax_result_free(&ed->syntax);
    ed->syntax = syntax_check_buffer(&ed->buf);
    ed->syntax_dirty = 0;

    /* Update error bar message */
    char summary[64];
    syntax_status_summary(&ed->syntax, summary, sizeof(summary));

    if (ed->syntax.count > 0) {
        /* Find first error/warning message */
        for (size_t i = 0; i < ed->syntax.count; i++) {
            if (ed->syntax.issues[i].level == SYNTAX_ERROR) {
                snprintf(ed->error_msg, sizeof(ed->error_msg),
                         " L%zu: %s ", ed->syntax.issues[i].line + 1,
                         ed->syntax.issues[i].msg);
                ed->error_has_err = 1;
                return;
            }
        }
        for (size_t i = 0; i < ed->syntax.count; i++) {
            if (ed->syntax.issues[i].level == SYNTAX_WARNING) {
                snprintf(ed->error_msg, sizeof(ed->error_msg),
                         " L%zu: %s ", ed->syntax.issues[i].line + 1,
                         ed->syntax.issues[i].msg);
                ed->error_has_err = 0;
                return;
            }
        }
    }

    ed->error_msg[0] = '\0';
    ed->error_has_err = 0;
}

void editor_move_cursor(Editor *ed, int key)
{
    if (!ed) return;

    Line *ln = buffer_get_line(&ed->buf, ed->cur_line);
    if (!ln) return;

    switch (key) {
    case KEY_LEFT:
        if (ed->cur_byte > 0) {
            ed->cur_byte = utf8_prev_char_pos(ln->data, ed->cur_byte);
        } else if (ed->cur_line > 0) {
            ed->cur_line--;
            ln = buffer_get_line(&ed->buf, ed->cur_line);
            if (ln) ed->cur_byte = ln->len;
        }
        if (ln) ed->cur_col_want = (int)byte_to_display_col(ln->data, ed->cur_byte);
        break;

    case KEY_RIGHT:
        if (ed->cur_byte < ln->len) {
            ed->cur_byte = utf8_next_char_pos(ln->data, ed->cur_byte, ln->len);
        } else if (ed->cur_line + 1 < ed->buf.count) {
            ed->cur_line++;
            ed->cur_byte = 0;
            ln = buffer_get_line(&ed->buf, ed->cur_line);
        }
        if (ln) ed->cur_col_want = (int)byte_to_display_col(ln->data, ed->cur_byte);
        break;

    case KEY_UP:
        if (ed->cur_line > 0) {
            ed->cur_line--;
            ln = buffer_get_line(&ed->buf, ed->cur_line);
            if (ln) {
                ed->cur_byte = display_col_to_byte(ln->data, ln->len,
                                                     ed->cur_col_want);
            }
        }
        break;

    case KEY_DOWN:
        if (ed->cur_line + 1 < ed->buf.count) {
            ed->cur_line++;
            ln = buffer_get_line(&ed->buf, ed->cur_line);
            if (ln) {
                ed->cur_byte = display_col_to_byte(ln->data, ln->len,
                                                     ed->cur_col_want);
            }
        }
        break;

    case KEY_HOME:
        ed->cur_byte = 0;
        ed->cur_col_want = 0;
        break;

    case KEY_END:
        ln = buffer_get_line(&ed->buf, ed->cur_line);
        if (ln) {
            ed->cur_byte = ln->len;
            ed->cur_col_want = (int)byte_to_display_col(ln->data, ln->len);
        }
        break;

    case KEY_PAGE_UP: {
        int rows, cols;
        term_get_size(&rows, &cols);
        (void)cols;
        ed->cur_line = (ed->cur_line > (size_t)(rows - 3))
                        ? ed->cur_line - (rows - 3) : 0;
        ln = buffer_get_line(&ed->buf, ed->cur_line);
        if (ln) {
            ed->cur_byte = display_col_to_byte(ln->data, ln->len,
                                                 ed->cur_col_want);
        }
        break;
    }

    case KEY_PAGE_DOWN: {
        int rows, cols;
        term_get_size(&rows, &cols);
        (void)cols;
        ed->cur_line += (rows - 3);
        if (ed->cur_line >= ed->buf.count) {
            ed->cur_line = ed->buf.count - 1;
        }
        ln = buffer_get_line(&ed->buf, ed->cur_line);
        if (ln) {
            ed->cur_byte = display_col_to_byte(ln->data, ln->len,
                                                 ed->cur_col_want);
        }
        break;
    }
    }

    editor_clamp_cursor(ed);
    editor_scroll(ed);
}

void editor_insert_char(Editor *ed, const char *ch, size_t len)
{
    if (!ed || !ch || len == 0) return;

    Line *ln = buffer_get_line(&ed->buf, ed->cur_line);
    if (!ln) return;

    if (line_insert_char(ln, ed->cur_byte, ch, len) == 0) {
        ed->cur_byte += len;
        ed->buf.modified = true;
        ed->cur_col_want = (int)byte_to_display_col(ln->data, ed->cur_byte);
        ed->syntax_dirty = 1;
    }

    editor_scroll(ed);
}

void editor_delete_char(Editor *ed)
{
    if (!ed) return;

    Line *ln = buffer_get_line(&ed->buf, ed->cur_line);
    if (!ln) return;

    if (ed->cur_byte > 0) {
        size_t prev = utf8_prev_char_pos(ln->data, ed->cur_byte);
        size_t char_len = ed->cur_byte - prev;
        if (line_delete_char_range(ln, prev, char_len) == 0) {
            ed->cur_byte = prev;
            ed->buf.modified = true;
            ed->syntax_dirty = 1;
        }
    } else if (ed->cur_line > 0) {
        size_t prev_line = ed->cur_line - 1;
        Line *prev_ln = buffer_get_line(&ed->buf, prev_line);
        if (prev_ln) {
            size_t old_len = prev_ln->len;
            if (line_append_str(prev_ln, ln->data, ln->len) == 0) {
                buffer_delete_line(&ed->buf, ed->cur_line);
                ed->cur_line = prev_line;
                ed->cur_byte = old_len;
                ed->buf.modified = true;
                ed->syntax_dirty = 1;
            }
        }
    }

    editor_clamp_cursor(ed);
    editor_scroll(ed);
}

void editor_insert_newline(Editor *ed)
{
    if (!ed) return;

    Line *ln = buffer_get_line(&ed->buf, ed->cur_line);
    if (!ln) return;

    size_t rest_len = ln->len - ed->cur_byte;
    char *tail = NULL;
    if (rest_len > 0) {
        tail = malloc(rest_len);
        if (!tail) return;
        memcpy(tail, ln->data + ed->cur_byte, rest_len);
    }

    if (buffer_insert_line(&ed->buf, ed->cur_line + 1) != 0) {
        free(tail);
        return;
    }

    ln = buffer_get_line(&ed->buf, ed->cur_line);
    Line *new_ln = buffer_get_line(&ed->buf, ed->cur_line + 1);
    if (!ln || !new_ln) { free(tail); return; }

    if (rest_len > 0 && tail) {
        if (line_append_str(new_ln, tail, rest_len) != 0) {
            free(tail);
            buffer_delete_line(&ed->buf, ed->cur_line + 1);
            return;
        }
        ln->data[ed->cur_byte] = '\0';
        ln->len = ed->cur_byte;
    }
    free(tail);

    ed->cur_line++;
    ed->cur_byte = 0;
    ed->cur_col_want = 0;
    ed->buf.modified = true;
    ed->syntax_dirty = 1;

    editor_scroll(ed);
}

void editor_open_file(Editor *ed, const char *path)
{
    if (!ed || !path) return;

    if (file_load(&ed->buf, path) < 0) {
        snprintf(ed->status_msg, sizeof(ed->status_msg),
                 "Error opening: %s", path);
    } else {
        ed->cur_line = 0;
        ed->cur_byte = 0;
        ed->cur_col_want = 0;
        ed->scr.row_off = 0;
        ed->scr.col_off = 0;
        ed->syntax_dirty = 1;
        snprintf(ed->status_msg, sizeof(ed->status_msg),
                 "Loaded: %s", path);
    }
}

void editor_save_file(Editor *ed)
{
    if (!ed) return;

    if (!ed->buf.filepath) {
        snprintf(ed->status_msg, sizeof(ed->status_msg),
                 "No filename set");
        return;
    }

    if (file_save(&ed->buf) == 0) {
        snprintf(ed->status_msg, sizeof(ed->status_msg),
                 "Saved: %s (%zu lines)", ed->buf.filepath, ed->buf.count);
    } else {
        snprintf(ed->status_msg, sizeof(ed->status_msg),
                 "Error saving: %s", ed->buf.filepath);
    }
}

void editor_search(Editor *ed)
{
    if (!ed) return;

    char query[256] = {0};
    int qlen = 0;

    snprintf(ed->status_msg, sizeof(ed->status_msg), "Search: ");
    term_draw_status(ed->status_msg, "");
    int rows, cols;
    term_get_size(&rows, &cols);

    term_cursor_move(rows - 3, 8 + qlen);
    term_cursor_show();
    term_flush();

    while (1) {
        int c = term_read_key();
        if (c == KEY_NONE) continue;

        if (c == KEY_ENTER || c == '\n') {
            break;
        } else if (c == KEY_ESCAPE) {
            qlen = 0;
            query[0] = '\0';
            break;
        } else if (c == KEY_BACKSPACE || c == 127) {
            if (qlen > 0) {
                size_t prev = 0;
                size_t i = 0;
                while (i < (size_t)qlen) {
                    size_t clen = utf8_char_len((unsigned char)query[i]);
                    if (i + clen > (size_t)qlen) break;
                    prev = i;
                    i += clen;
                }
                qlen = (int)prev;
                query[qlen] = '\0';
            }
        } else if (c >= 32 && c < 127 && qlen < (int)sizeof(query) - 2) {
            query[qlen++] = (char)c;
            query[qlen] = '\0';
        }
        else if (c >= 0xC0 && qlen < (int)sizeof(query) - 4) {
            size_t clen = utf8_char_len((unsigned char)c);
            query[qlen++] = (char)c;
            for (size_t j = 1; j < clen && qlen < (int)sizeof(query) - 2; j++) {
                int cb = term_read_key();
                if (cb == KEY_NONE) break;
                query[qlen++] = (char)cb;
            }
            query[qlen] = '\0';
        }

        char prompt[512];
        snprintf(prompt, sizeof(prompt), "Search: %s", query);
        term_draw_status(prompt, "");
        term_cursor_move(rows - 3, 8 + qlen);
        term_flush();
    }

    if (qlen == 0) {
        free(ed->search_query);
        ed->search_query = NULL;
        snprintf(ed->status_msg, sizeof(ed->status_msg), "Search cancelled");
        return;
    }

    free(ed->search_query);
    ed->search_query = strdup(query);

    SearchMatch match;
    if (search_find(&ed->buf, query, ed->cur_line, ed->cur_byte,
                    SEARCH_FORWARD, &match)) {
        ed->cur_line = match.line;
        ed->cur_byte = match.byte_pos;
        editor_clamp_cursor(ed);
        editor_scroll(ed);
        snprintf(ed->status_msg, sizeof(ed->status_msg),
                 "Found at line %zu", match.line + 1);
    } else {
        if (search_find(&ed->buf, query, 0, 0, SEARCH_FORWARD, &match)) {
            ed->cur_line = match.line;
            ed->cur_byte = match.byte_pos;
            editor_clamp_cursor(ed);
            editor_scroll(ed);
            snprintf(ed->status_msg, sizeof(ed->status_msg),
                     "Wrapped: found at line %zu", match.line + 1);
        } else {
            snprintf(ed->status_msg, sizeof(ed->status_msg),
                     "Not found: %s", query);
        }
    }
}
