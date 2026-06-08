/*
 * MINI - term.h
 * Raw terminal mode and ANSI escape sequence rendering.
 * No ncurses dependency — pure POSIX termios + ANSI escapes.
 */

#ifndef MINI_TERM_H
#define MINI_TERM_H

#include <stddef.h>

/* Screen size and scroll state */
typedef struct {
    int rows;          /* terminal rows */
    int cols;          /* terminal columns */
    int row_off;       /* vertical scroll offset (first visible line) */
    int col_off;       /* horizontal scroll offset */
} Screen;

/* Color codes for syntax highlighting */
typedef enum {
    TERM_COLOR_NONE  = 0,
    TERM_COLOR_RED,        /* errors */
    TERM_COLOR_YELLOW,     /* warnings */
    TERM_COLOR_CYAN,       /* matching brackets / line numbers */
    TERM_COLOR_GREEN,      /* strings */
    TERM_COLOR_GRAY,       /* dim text */
    TERM_COLOR_BRED,       /* bright red for error bar */
    TERM_COLOR_BGREEN      /* bright green for OK status */
} TermColor;

/* Terminal lifecycle */
int  term_init(void);
void term_cleanup(void);
void term_get_size(int *rows, int *cols);

/* Raw output (low-level) */
void term_clear(void);
void term_cursor_move(int row, int col);
void term_cursor_hide(void);
void term_cursor_show(void);
void term_write(const char *s, size_t len);
void term_flush(void);

/* Apply an ANSI color */
void term_set_color(TermColor color);
void term_reset_color(void);

/* High-level rendering */
void term_draw_line_number(int num, int width, int is_current);
void term_draw_text(const char *data, size_t len, int max_cols, int col_off);
void term_draw_text_colored(const char *data, size_t len, int max_cols, int col_off,
                            const TermColor *colors, size_t colors_len);
void term_draw_status(const char *left, const char *right);
void term_draw_status_colored(const char *left, const char *right,
                              TermColor left_color, TermColor right_color);
void term_draw_message(const char *msg);
void term_draw_error_bar(const char *msg, int has_error);
void term_clear_message(void);

/* Input */
int  term_read_key(void);

/* Key codes (beyond ASCII) */
#define KEY_NONE      (-1)
#define KEY_UP        (-100)
#define KEY_DOWN      (-101)
#define KEY_LEFT      (-102)
#define KEY_RIGHT     (-103)
#define KEY_HOME      (-104)
#define KEY_END       (-105)
#define KEY_PAGE_UP   (-106)
#define KEY_PAGE_DOWN (-107)
#define KEY_DELETE    (-108)
#define KEY_BACKSPACE (8)
#define KEY_ENTER     (13)
#define KEY_TAB       (9)
#define KEY_ESCAPE    (27)
#define KEY_CTRL_A    (1)
#define KEY_CTRL_E    (5)
#define KEY_CTRL_K    (11)
#define KEY_CTRL_O    (15)
#define KEY_CTRL_R    (18)
#define KEY_CTRL_S    (19)
#define KEY_CTRL_W    (23)
#define KEY_CTRL_X    (24)
#define KEY_CTRL_G    (7)
#define KEY_CTRL_Y    (25)
#define KEY_CTRL_V    (22)

#endif /* MINI_TERM_H */
