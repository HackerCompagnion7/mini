/*
 * MINI - main.c
 * Entry point and main keyboard loop.
 */

#define _POSIX_C_SOURCE 200809L
#include "editor.h"
#include "term.h"
#include "buffer.h"
#include "syntax.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Rendering ---- */

static void editor_render(Editor *ed)
{
    if (!ed) return;

    /* Update syntax if dirty */
    editor_syntax_update(ed);

    int rows, cols;
    term_get_size(&rows, &cols);

    int num_width = editor_num_width(ed->buf.count);

    term_cursor_hide();

    /* Draw each visible line (rows - 3: text, status, error) */
    int text_rows = rows - 3;
    if (text_rows < 1) text_rows = 1;

    for (int y = 0; y < text_rows; y++) {
        int file_line = ed->scr.row_off + y;

        term_cursor_move(y, 0);

        if ((size_t)file_line < ed->buf.count) {
            Line *ln = buffer_get_line(&ed->buf, (size_t)file_line);
            int is_current = ((size_t)file_line == ed->cur_line);

            /* Highlight current line background */
            if (is_current) {
                term_write("\x1b[48;5;235m", 11); /* dark gray bg */
            }

            /* Line number with dynamic width */
            term_draw_line_number(file_line + 1, num_width - 1, is_current);

            /* Text content with syntax coloring */
            if (ln && ln->data) {
                /* Build color array for this line */
                TermColor *colors = NULL;
                if (ed->syntax.count > 0) {
                    colors = malloc(ln->len * sizeof(TermColor));
                    if (colors) {
                        syntax_line_colors(&ed->syntax, (size_t)file_line,
                                           colors, ln->len);
                    }
                }

                term_draw_text_colored(ln->data, ln->len,
                                       cols - num_width, ed->scr.col_off,
                                       colors, ln->len);
                free(colors);
            } else {
                term_write("\x1b[K", 3);
            }

            if (is_current) {
                term_write("\x1b[0m", 4); /* reset */
            }
        } else {
            /* Beyond file: tilde marker */
            term_write("\x1b[90m", 5); /* gray */
            /* Align tilde with line number area */
            for (int i = 0; i < num_width - 2; i++) {
                term_write(" ", 1);
            }
            term_write("~", 1);
            term_write("\x1b[0m", 4);
            term_write("\x1b[K", 3);
        }
    }

    /* Status bar (row - 3) */
    term_cursor_move(rows - 3, 0);

    char left[160], right[160];
    const char *fname = ed->buf.filepath ? ed->buf.filepath : "[No Name]";
    const char *modified = ed->buf.modified ? " [+]" : "";

    /* Extract just the basename for display */
    const char *basename = fname;
    const char *slash = strrchr(fname, '/');
    if (slash) basename = slash + 1;

    snprintf(left, sizeof(left), " MINI | %s%s | L%d/%zu ",
             basename, modified,
             (int)(ed->cur_line + 1), ed->buf.count);

    /* Syntax status on the right */
    char syn_summary[32];
    syntax_status_summary(&ed->syntax, syn_summary, sizeof(syn_summary));

    TermColor right_color = TERM_COLOR_BGREEN;
    if (ed->syntax.count > 0) {
        int has_err = 0;
        for (size_t i = 0; i < ed->syntax.count; i++) {
            if (ed->syntax.issues[i].level == SYNTAX_ERROR) {
                has_err = 1;
                break;
            }
        }
        right_color = has_err ? TERM_COLOR_BRED : TERM_COLOR_YELLOW;
    }

    snprintf(right, sizeof(right), " %s | ^X:Quit ^S:Save ^R:Search ", syn_summary);

    term_draw_status_colored(left, right, TERM_COLOR_NONE, right_color);

    /* Error bar (row - 2) — only shown if there are issues */
    term_cursor_move(rows - 2, 0);
    if (ed->error_msg[0]) {
        term_draw_error_bar(ed->error_msg, ed->error_has_err);
    } else {
        term_clear_message();
    }

    /* Message bar (row - 1) */
    term_cursor_move(rows - 1, 0);
    if (ed->status_msg[0]) {
        term_draw_message(ed->status_msg);
    } else {
        term_clear_message();
    }

    /* Position cursor */
    Line *cur_ln = buffer_get_line(&ed->buf, ed->cur_line);
    int cur_display = 0;
    if (cur_ln) {
        cur_display = (int)byte_to_display_col(cur_ln->data, ed->cur_byte);
    }
    int screen_row = (int)ed->cur_line - ed->scr.row_off;
    int screen_col = num_width + cur_display - ed->scr.col_off;

    if (screen_row >= 0 && screen_row < text_rows &&
        screen_col >= num_width && screen_col < cols) {
        term_cursor_move(screen_row, screen_col);
    }

    term_cursor_show();
    term_flush();
}

/* ---- Main ---- */

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [filename]\n", prog);
    fprintf(stderr, "MINI - A lightweight terminal text editor\n");
}

int main(int argc, char *argv[])
{
    const char *filepath = NULL;

    if (argc > 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (argc == 2) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        filepath = argv[1];
    }

    /* Initialize terminal */
    if (term_init() < 0) {
        fprintf(stderr, "Error: cannot initialize terminal\n");
        return 1;
    }

    /* Initialize editor */
    Editor ed;
    if (editor_init(&ed, filepath) < 0) {
        term_cleanup();
        fprintf(stderr, "Error: cannot initialize editor\n");
        return 1;
    }

    /* Initial render */
    editor_render(&ed);

    /* Main loop */
    while (ed.running) {
        int c = term_read_key();
        if (c == KEY_NONE) continue;

        /* Clear status message on any key (except search continuation) */
        if (c != KEY_CTRL_R && c != KEY_CTRL_W) {
            ed.status_msg[0] = '\0';
        }

        switch (c) {
        /* Movement */
        case KEY_UP:
        case KEY_DOWN:
        case KEY_LEFT:
        case KEY_RIGHT:
        case KEY_HOME:
        case KEY_END:
        case KEY_PAGE_UP:
        case KEY_PAGE_DOWN:
            editor_move_cursor(&ed, c);
            break;

        /* Editing */
        case KEY_BACKSPACE:
        case 127:
            editor_delete_char(&ed);
            break;

        case KEY_DELETE: {
            Line *ln = buffer_get_line(&ed.buf, ed.cur_line);
            if (ln && ed.cur_byte < ln->len) {
                size_t next = utf8_next_char_pos(ln->data, ed.cur_byte, ln->len);
                line_delete_char_range(ln, ed.cur_byte, next - ed.cur_byte);
                ed.buf.modified = true;
                ed.syntax_dirty = 1;
            } else if (ed.cur_line + 1 < ed.buf.count) {
                ed.cur_line++;
                ed.cur_byte = 0;
                editor_delete_char(&ed);
            }
            break;
        }

        case KEY_ENTER:
        case '\n':
            editor_insert_newline(&ed);
            break;

        case KEY_TAB: {
            for (int i = 0; i < 4; i++) {
                editor_insert_char(&ed, " ", 1);
            }
            break;
        }

        /* Commands */
        case KEY_CTRL_S:
            editor_save_file(&ed);
            break;

        case KEY_CTRL_R:
            editor_search(&ed);
            break;

        case KEY_CTRL_X:
            ed.running = 0;
            break;

        case KEY_CTRL_G: {
            snprintf(ed.status_msg, sizeof(ed.status_msg),
                     "^S:Save  ^R:Search  ^X:Quit  ^K:CutLine  ^A/^E:Home/End");
            break;
        }

        case KEY_CTRL_K: {
            if (ed.buf.count > 1) {
                buffer_delete_line(&ed.buf, ed.cur_line);
                if (ed.cur_line >= ed.buf.count) {
                    ed.cur_line = ed.buf.count - 1;
                }
                ed.cur_byte = 0;
                ed.buf.modified = true;
                ed.syntax_dirty = 1;
            }
            break;
        }

        case KEY_CTRL_A:
            ed.cur_byte = 0;
            ed.cur_col_want = 0;
            break;

        case KEY_CTRL_E:
            {
                Line *ln = buffer_get_line(&ed.buf, ed.cur_line);
                if (ln) {
                    ed.cur_byte = ln->len;
                    ed.cur_col_want = (int)byte_to_display_col(ln->data, ln->len);
                }
            }
            break;

        default:
            /* Insert printable character */
            if (c >= 32 && c < 127) {
                char ch = (char)c;
                editor_insert_char(&ed, &ch, 1);
            }
            /* Handle UTF-8 multi-byte input */
            else if (c >= 0xC0) {
                char utf8[4] = {(char)c, 0, 0, 0};
                size_t clen = utf8_char_len((unsigned char)c);
                size_t got = 1;
                for (size_t i = 1; i < clen; i++) {
                    int cb = term_read_key();
                    if (cb == KEY_NONE || (cb & 0xC0) != 0x80) break;
                    utf8[got++] = (char)cb;
                }
                if (got == clen) {
                    editor_insert_char(&ed, utf8, clen);
                }
            }
            break;
        }

        editor_render(&ed);
    }

    /* Cleanup */
    editor_destroy(&ed);
    term_cleanup();

    return 0;
}
