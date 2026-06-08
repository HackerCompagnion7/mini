/*
 * MINI - term.c
 * Raw terminal mode and ANSI escape sequence rendering.
 * Pure POSIX termios + ANSI escapes — no ncurses dependency.
 */

#define _POSIX_C_SOURCE 200809L
#include "term.h"
#include "buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>

/* ---- Private state ---- */

static struct termios orig_termios;
static int term_initialized = 0;

/* Escape sequence buffer for batched output */
#define ESC_BUF_SIZE 16384
static char esc_buf[ESC_BUF_SIZE];
static size_t esc_len = 0;

/* ---- Internal helpers ---- */

static void esc_append(const char *s, size_t len)
{
    if (esc_len + len >= ESC_BUF_SIZE) {
        term_flush();
    }
    if (len > ESC_BUF_SIZE - 1) {
        /* Too large, write directly */
        write(STDOUT_FILENO, s, len);
        return;
    }
    memcpy(esc_buf + esc_len, s, len);
    esc_len += len;
}

static void esc_str(const char *s)
{
    esc_append(s, strlen(s));
}

/* ---- Lifecycle ---- */

int term_init(void)
{
    if (term_initialized) return 0;

    if (tcgetattr(STDIN_FILENO, &orig_termios) < 0) return -1;

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) return -1;

    term_initialized = 1;
    esc_len = 0;

    /* Enter alternate screen buffer */
    esc_str("\x1b[?1049h");
    term_cursor_hide();
    term_flush();

    return 0;
}

void term_cleanup(void)
{
    if (!term_initialized) return;

    /* Exit alternate screen buffer */
    esc_str("\x1b[?1049l");
    term_cursor_show();
    term_flush();

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    term_initialized = 0;
}

void term_get_size(int *rows, int *cols)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0 || ws.ws_row == 0) {
        *rows = 24;
        *cols = 80;
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    }
}

/* ---- Raw output ---- */

void term_clear(void)
{
    esc_str("\x1b[2J");
}

void term_cursor_move(int row, int col)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row + 1, col + 1);
    esc_str(buf);
}

void term_cursor_hide(void)
{
    esc_str("\x1b[?25l");
}

void term_cursor_show(void)
{
    esc_str("\x1b[?25h");
}

void term_write(const char *s, size_t len)
{
    esc_append(s, len);
}

void term_flush(void)
{
    if (esc_len > 0) {
        /* M-3: handle partial writes */
        size_t written = 0;
        while (written < esc_len) {
            ssize_t n = write(STDOUT_FILENO, esc_buf + written,
                              esc_len - written);
            if (n < 0) {
                if (errno == EINTR) continue;
                break;
            }
            written += (size_t)n;
        }
        esc_len = 0;
    }
}

/* ---- Color management ---- */

void term_set_color(TermColor color)
{
    switch (color) {
    case TERM_COLOR_NONE:   esc_str("\x1b[0m");   break; /* reset */
    case TERM_COLOR_RED:    esc_str("\x1b[31m");   break;
    case TERM_COLOR_YELLOW: esc_str("\x1b[33m");   break;
    case TERM_COLOR_CYAN:   esc_str("\x1b[36m");   break;
    case TERM_COLOR_GREEN:  esc_str("\x1b[32m");   break;
    case TERM_COLOR_GRAY:   esc_str("\x1b[90m");   break; /* bright black = gray */
    case TERM_COLOR_BRED:   esc_str("\x1b[91m");   break; /* bright red */
    case TERM_COLOR_BGREEN: esc_str("\x1b[92m");   break; /* bright green */
    }
}

void term_reset_color(void)
{
    esc_str("\x1b[0m");
}

/* ---- High-level rendering ---- */

void term_draw_line_number(int num, int width, int is_current)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%*d ", width, num);
    if (is_current) {
        esc_str("\x1b[1;36m"); /* bold cyan for current line */
    } else {
        esc_str("\x1b[90m");   /* gray for other lines */
    }
    esc_str(buf);
    esc_str("\x1b[0m");  /* reset */
}

void term_draw_text(const char *data, size_t len, int max_cols, int col_off)
{
    /* Skip col_off display characters */
    int display_pos = 0;
    size_t byte_i = 0;

    while (byte_i < len && display_pos < col_off) {
        size_t clen = utf8_char_len((unsigned char)data[byte_i]);
        if (byte_i + clen > len) break;
        display_pos++;
        byte_i += clen;
    }

    /* Draw up to max_cols display characters */
    int col = 0;
    while (byte_i < len && col < max_cols) {
        size_t clen = utf8_char_len((unsigned char)data[byte_i]);

        /* Validate continuation bytes */
        int valid = 1;
        if (clen > 1) {
            for (size_t j = 1; j < clen && byte_i + j < len; j++) {
                if (((unsigned char)data[byte_i + j] & 0xC0) != 0x80) {
                    valid = 0;
                    break;
                }
            }
        }

        if (valid && byte_i + clen <= len) {
            esc_append(data + byte_i, clen);
            byte_i += clen;
        } else {
            /* Invalid byte: show replacement */
            esc_str("\xef\xbf\xbd"); /* U+FFFD */
            byte_i++;
        }
        col++;
    }

    /* Clear to end of line */
    esc_str("\x1b[K");
}

void term_draw_text_colored(const char *data, size_t len, int max_cols, int col_off,
                            const TermColor *colors, size_t colors_len)
{
    /* Skip col_off display characters */
    int display_pos = 0;
    size_t byte_i = 0;

    while (byte_i < len && display_pos < col_off) {
        size_t clen = utf8_char_len((unsigned char)data[byte_i]);
        if (byte_i + clen > len) break;
        display_pos++;
        byte_i += clen;
    }

    /* Draw up to max_cols display characters with color */
    int col = 0;
    TermColor current_color = TERM_COLOR_NONE;

    while (byte_i < len && col < max_cols) {
        size_t clen = utf8_char_len((unsigned char)data[byte_i]);

        /* Validate continuation bytes */
        int valid = 1;
        if (clen > 1) {
            for (size_t j = 1; j < clen && byte_i + j < len; j++) {
                if (((unsigned char)data[byte_i + j] & 0xC0) != 0x80) {
                    valid = 0;
                    break;
                }
            }
        }

        /* Determine color for this character */
        TermColor char_color = TERM_COLOR_NONE;
        if (colors && byte_i < colors_len) {
            char_color = colors[byte_i];
        }

        /* Apply color change if needed */
        if (char_color != current_color) {
            if (char_color == TERM_COLOR_NONE) {
                term_reset_color();
            } else {
                term_set_color(char_color);
            }
            current_color = char_color;
        }

        if (valid && byte_i + clen <= len) {
            esc_append(data + byte_i, clen);
            byte_i += clen;
        } else {
            /* Invalid byte: show replacement */
            esc_str("\xef\xbf\xbd"); /* U+FFFD */
            byte_i++;
        }
        col++;
    }

    /* Reset color at end */
    if (current_color != TERM_COLOR_NONE) {
        term_reset_color();
    }

    /* Clear to end of line */
    esc_str("\x1b[K");
}

void term_draw_status(const char *left, const char *right)
{
    esc_str("\x1b[7m"); /* reverse video */

    int rows, cols;
    term_get_size(&rows, &cols);

    size_t left_len = left ? strlen(left) : 0;
    size_t right_len = right ? strlen(right) : 0;

    if (left) esc_str(left);

    /* Pad between left and right */
    int padding = cols - (int)left_len - (int)right_len;
    if (padding < 0) padding = 0;
    for (int i = 0; i < padding; i++) {
        esc_str(" ");
    }

    if (right) esc_str(right);

    esc_str("\x1b[0m"); /* reset */
    esc_str("\x1b[K");
}

void term_draw_status_colored(const char *left, const char *right,
                              TermColor left_color, TermColor right_color)
{
    int rows, cols;
    term_get_size(&rows, &cols);

    /* Status bar background: dark blue (1;34 on reverse) */
    esc_str("\x1b[44m"); /* blue bg */

    /* Left side with color */
    if (left_color != TERM_COLOR_NONE) {
        term_set_color(left_color);
    } else {
        esc_str("\x1b[37m"); /* white fg */
    }
    esc_str("\x1b[1m"); /* bold */
    if (left) esc_str(left);

    size_t left_len = left ? strlen(left) : 0;
    size_t right_len = right ? strlen(right) : 0;

    /* Pad between left and right */
    int padding = cols - (int)left_len - (int)right_len;
    if (padding < 0) padding = 0;
    esc_str("\x1b[37m"); /* white fg for padding */
    esc_str("\x1b[22m"); /* reset bold */
    for (int i = 0; i < padding; i++) {
        esc_str(" ");
    }

    /* Right side with color */
    if (right_color != TERM_COLOR_NONE) {
        term_set_color(right_color);
    } else {
        esc_str("\x1b[37m"); /* white fg */
    }
    esc_str("\x1b[1m"); /* bold */
    if (right) esc_str(right);

    esc_str("\x1b[0m"); /* full reset */
    esc_str("\x1b[K");
}

void term_draw_message(const char *msg)
{
    esc_str("\x1b[0m");
    if (msg) esc_str(msg);
    esc_str("\x1b[K");
}

void term_draw_error_bar(const char *msg, int has_error)
{
    if (has_error) {
        esc_str("\x1b[41m");   /* red bg */
        esc_str("\x1b[1;37m"); /* bold white fg */
    } else {
        esc_str("\x1b[43m");   /* yellow bg */
        esc_str("\x1b[30m");   /* black fg */
    }
    if (msg) esc_str(msg);
    esc_str("\x1b[0m");
    esc_str("\x1b[K");
}

void term_clear_message(void)
{
    esc_str("\x1b[0m");
    esc_str("\x1b[K");
}

/* ---- Input ---- */

int term_read_key(void)
{
    char c;
    ssize_t n;

    while (1) {
        n = read(STDIN_FILENO, &c, 1);
        if (n == 1) break;
        if (n == -1 && errno == EINTR) continue;
        return KEY_NONE;
    }

    /* Ctrl key combinations (c <= 26, can never equal KEY_ESCAPE=27) */
    if (c >= 1 && c <= 26) {
        return (int)c;
    }

    /* Escape sequences */
    if (c == '\x1b') {
        /* Wait a bit for the rest of the sequence */
        char seq[4] = {0};
        n = read(STDIN_FILENO, &seq[0], 1);
        if (n != 1) return KEY_ESCAPE;

        if (seq[0] == '[') {
            n = read(STDIN_FILENO, &seq[1], 1);
            if (n != 1) return KEY_ESCAPE;

            if (seq[1] >= '0' && seq[1] <= '9') {
                n = read(STDIN_FILENO, &seq[2], 1);
                if (n != 1) return KEY_ESCAPE;

                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return KEY_HOME;
                        case '3': return KEY_DELETE;
                        case '4': return KEY_END;
                        case '5': return KEY_PAGE_UP;
                        case '6': return KEY_PAGE_DOWN;
                        case '7': return KEY_HOME;
                        case '8': return KEY_END;
                    }
                } else if (seq[2] == ';') {
                    /* Extended: \x1b[1;xC where x is modifier+1 */
                    n = read(STDIN_FILENO, &seq[3], 1);
                    if (n != 1) return KEY_ESCAPE;
                    /* seq[3] is the key letter */
                    /* For now, ignore modifiers and treat as base key */
                }
            } else {
                switch (seq[1]) {
                    case 'A': return KEY_UP;
                    case 'B': return KEY_DOWN;
                    case 'C': return KEY_RIGHT;
                    case 'D': return KEY_LEFT;
                    case 'H': return KEY_HOME;
                    case 'F': return KEY_END;
                }
            }
        } else if (seq[0] == 'O') {
            n = read(STDIN_FILENO, &seq[1], 1);
            if (n != 1) return KEY_ESCAPE;
            switch (seq[1]) {
                case 'H': return KEY_HOME;
                case 'F': return KEY_END;
            }
        }

        return KEY_ESCAPE;
    }

    /* Regular character */
    return (int)(unsigned char)c;
}
