/*
 * MINI - editor.h
 * Main editor state and operations.
 */

#ifndef MINI_EDITOR_H
#define MINI_EDITOR_H

#include "buffer.h"
#include "term.h"
#include "syntax.h"

/* Editor state */
typedef struct {
    Buffer       buf;          /* text buffer */
    Screen       scr;          /* screen/scroll state */
    size_t       cur_line;     /* cursor line index (0-based) */
    size_t       cur_byte;     /* cursor byte position within current line */
    int          cur_col_want; /* desired column for vertical movement */
    int          running;      /* main loop flag */
    char         status_msg[256]; /* status bar message */
    char         error_msg[256];  /* error bar message */
    int          error_has_err;   /* 1 if error, 0 if warning only */
    char        *search_query; /* current search pattern (or NULL) */
    SyntaxResult syntax;       /* syntax check results */
    int          syntax_dirty; /* 1 if syntax needs re-check */
} Editor;

/* Lifecycle */
int  editor_init(Editor *ed, const char *filepath);
void editor_destroy(Editor *ed);

/* Main loop */
void editor_run(Editor *ed);

/* Operations */
void editor_move_cursor(Editor *ed, int key);
void editor_insert_char(Editor *ed, const char *ch, size_t len);
void editor_delete_char(Editor *ed);
void editor_insert_newline(Editor *ed);
void editor_open_file(Editor *ed, const char *path);
void editor_save_file(Editor *ed);
void editor_search(Editor *ed);

/* Syntax */
void editor_syntax_update(Editor *ed);

/* Utility: byte position to display column */
size_t byte_to_display_col(const char *data, size_t byte_pos);

/* Utility: calculate line number gutter width (3-6 chars) */
int editor_num_width(size_t line_count);

#endif /* MINI_EDITOR_H */
