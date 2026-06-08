/*
 * MINI - syntax.c
 * Lightweight bracket/quote mismatch detection.
 *
 * Strategy: Single-pass stack-based bracket matching + quote tracking.
 * - Tracks (), [], {} with a stack (max 256 depth)
 * - Tracks single and double quotes (toggle per line)
 * - Marks unmatched openers and closers as ERROR
 * - No full tokenizer — keeps memory and latency minimal
 */

#define _POSIX_C_SOURCE 200809L
#include "syntax.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- Bracket stack ---- */

#define MAX_STACK 256

typedef struct {
    char   ch;        /* the opening bracket character */
    size_t line;      /* line where it was found */
    size_t byte_pos;  /* byte position in that line */
} BracketEntry;

/* ---- Internal: add issue to result ---- */

static void add_issue(SyntaxResult *sr, SyntaxLevel level,
                      size_t line, size_t byte_pos,
                      const char *msg)
{
    if (!sr) return;
    if (sr->count >= sr->cap) {
        size_t new_cap = sr->cap ? sr->cap * 2 : 32;
        SyntaxIssue *new_issues = realloc(sr->issues, new_cap * sizeof(SyntaxIssue));
        if (!new_issues) return;  /* silently skip on OOM */
        sr->issues = new_issues;
        sr->cap = new_cap;
    }
    SyntaxIssue *iss = &sr->issues[sr->count++];
    iss->level = level;
    iss->line = line;
    iss->byte_pos = byte_pos;
    snprintf(iss->msg, sizeof(iss->msg), "%s", msg);
}

/* ---- Public API ---- */

SyntaxResult syntax_check_buffer(Buffer *buf)
{
    SyntaxResult sr;
    sr.issues = NULL;
    sr.count = 0;
    sr.cap = 0;

    if (!buf || buf->count == 0) return sr;

    BracketEntry stack[MAX_STACK];
    int stack_top = 0;

    /* Track quote state: -1 = none, 0 = in single, 1 = in double */
    int in_quote = -1;

    for (size_t li = 0; li < buf->count; li++) {
        Line *ln = &buf->lines[li];
        if (!ln->data) continue;

        /* Reset quote state per line (simplified: no multi-line strings) */
        in_quote = -1;

        for (size_t bi = 0; bi < ln->len; bi++) {
            char c = ln->data[bi];

            /* Skip characters inside quotes for bracket matching */
            if (in_quote == -1) {
                /* Not in a string */
                if (c == '\'' || c == '"') {
                    in_quote = (c == '"') ? 1 : 0;
                    continue;
                }

                /* Opening brackets */
                if (c == '(' || c == '[' || c == '{') {
                    if (stack_top < MAX_STACK) {
                        stack[stack_top].ch = c;
                        stack[stack_top].line = li;
                        stack[stack_top].byte_pos = bi;
                        stack_top++;
                    }
                    continue;
                }

                /* Closing brackets */
                if (c == ')' || c == ']' || c == '}') {
                    char expected_open;
                    if (c == ')') expected_open = '(';
                    else if (c == ']') expected_open = '[';
                    else expected_open = '{';

                    if (stack_top > 0 && stack[stack_top - 1].ch == expected_open) {
                        stack_top--;  /* match found, pop */
                    } else {
                        /* Unmatched closing bracket */
                        char msg[64];
                        snprintf(msg, sizeof(msg),
                                 "Unmatched '%c'", c);
                        add_issue(&sr, SYNTAX_ERROR, li, bi, msg);
                    }
                    continue;
                }
            } else {
                /* Inside a string — look for closing quote */
                if ((in_quote == 0 && c == '\'') ||
                    (in_quote == 1 && c == '"')) {
                    /* Check for escaped quote (preceded by backslash) */
                    if (bi > 0 && ln->data[bi - 1] == '\\') {
                        continue;  /* escaped, stay in string */
                    }
                    in_quote = -1;  /* exit string */
                }
            }
        }

        /* Check for unclosed quote at end of line */
        if (in_quote != -1) {
            char q = (in_quote == 0) ? '\'' : '"';
            char msg[64];
            snprintf(msg, sizeof(msg),
                     "Unclosed '%c' quote", q);
            add_issue(&sr, SYNTAX_ERROR, li, ln->len > 0 ? ln->len - 1 : 0, msg);
        }
    }

    /* Any remaining stack entries are unmatched opening brackets */
    for (int i = 0; i < stack_top; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg),
                 "Unmatched '%c' (opened here)", stack[i].ch);
        add_issue(&sr, SYNTAX_ERROR, stack[i].line, stack[i].byte_pos, msg);
    }

    return sr;
}

void syntax_line_colors(const SyntaxResult *sr, size_t line,
                        TermColor *colors, size_t line_len)
{
    if (!colors || line_len == 0) return;

    /* Default: all none */
    for (size_t i = 0; i < line_len; i++) {
        colors[i] = TERM_COLOR_NONE;
    }

    if (!sr) return;

    /* Mark error/warning positions */
    for (size_t i = 0; i < sr->count; i++) {
        if (sr->issues[i].line == line) {
            size_t pos = sr->issues[i].byte_pos;
            if (pos < line_len) {
                if (sr->issues[i].level == SYNTAX_ERROR) {
                    colors[pos] = TERM_COLOR_RED;
                } else if (sr->issues[i].level == SYNTAX_WARNING) {
                    colors[pos] = TERM_COLOR_YELLOW;
                }
            }
            /* Also mark the character right after for visibility */
            if (pos + 1 < line_len && colors[pos + 1] == TERM_COLOR_NONE) {
                /* For unmatched openers, highlight the bracket char */
                /* This is already handled by marking the exact position */
            }
        }
    }
}

int syntax_line_has_error(const SyntaxResult *sr, size_t line)
{
    if (!sr) return 0;
    for (size_t i = 0; i < sr->count; i++) {
        if (sr->issues[i].line == line && sr->issues[i].level == SYNTAX_ERROR)
            return 1;
    }
    return 0;
}

int syntax_line_has_warning(const SyntaxResult *sr, size_t line)
{
    if (!sr) return 0;
    for (size_t i = 0; i < sr->count; i++) {
        if (sr->issues[i].line == line && sr->issues[i].level == SYNTAX_WARNING)
            return 1;
    }
    return 0;
}

void syntax_status_summary(const SyntaxResult *sr, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;

    if (!sr || sr->count == 0) {
        snprintf(out, out_size, "OK");
        return;
    }

    int errors = 0, warnings = 0;
    for (size_t i = 0; i < sr->count; i++) {
        if (sr->issues[i].level == SYNTAX_ERROR) errors++;
        else if (sr->issues[i].level == SYNTAX_WARNING) warnings++;
    }

    if (errors > 0 && warnings > 0) {
        snprintf(out, out_size, "%d ERR %d WARN", errors, warnings);
    } else if (errors > 0) {
        snprintf(out, out_size, "%d ERR", errors);
    } else if (warnings > 0) {
        snprintf(out, out_size, "%d WARN", warnings);
    } else {
        snprintf(out, out_size, "OK");
    }
}

void syntax_result_free(SyntaxResult *sr)
{
    if (!sr) return;
    free(sr->issues);
    sr->issues = NULL;
    sr->count = 0;
    sr->cap = 0;
}
