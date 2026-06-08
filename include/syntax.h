/*
 * MINI - syntax.h
 * Lightweight bracket/quote mismatch detection for error highlighting.
 * Not a full parser — only catches structural errors.
 */

#ifndef MINI_SYNTAX_H
#define MINI_SYNTAX_H

#include "buffer.h"
#include "term.h"      /* for TermColor */
#include <stddef.h>

/* Severity levels for detected issues */
typedef enum {
    SYNTAX_OK = 0,      /* no issue */
    SYNTAX_WARNING,     /* potential problem (yellow) */
    SYNTAX_ERROR        /* definite problem (red) */
} SyntaxLevel;

/* A single detected issue */
typedef struct {
    SyntaxLevel level;  /* severity */
    size_t line;        /* line index (0-based) */
    size_t byte_pos;    /* byte position in line */
    char msg[96];       /* human-readable description */
} SyntaxIssue;

/* Result of full-buffer syntax check */
typedef struct {
    SyntaxIssue *issues;    /* dynamic array of issues */
    size_t       count;     /* number of issues */
    size_t       cap;       /* capacity */
} SyntaxResult;

/* Analyze entire buffer for bracket/quote mismatches.
 * Returns a SyntaxResult that must be freed with syntax_result_free(). */
SyntaxResult syntax_check_buffer(Buffer *buf);

/* Build per-byte color array for a single line.
 * `colors` must be at least `line_len` elements.
 * Fills each position with the appropriate TermColor. */
void syntax_line_colors(const SyntaxResult *sr, size_t line,
                        TermColor *colors, size_t line_len);

/* Check if a specific line has any issues (for status bar indicator). */
int syntax_line_has_error(const SyntaxResult *sr, size_t line);
int syntax_line_has_warning(const SyntaxResult *sr, size_t line);

/* Get summary string for status bar: "OK" / "1 ERROR" / "2 WARNINGS" etc. */
void syntax_status_summary(const SyntaxResult *sr, char *out, size_t out_size);

/* Free a SyntaxResult. */
void syntax_result_free(SyntaxResult *sr);

#endif /* MINI_SYNTAX_H */
