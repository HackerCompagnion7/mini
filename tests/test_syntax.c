/*
 * MINI - test_syntax.c
 * Tests for syntax checking module.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "syntax.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %-50s", #name); } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

static void test_balanced_parens(void)
{
    TEST(balanced_parentheses);
    Buffer buf = buffer_create();
    line_append_str(&buf.lines[0], "int main() { return 0; }", 24);

    SyntaxResult sr = syntax_check_buffer(&buf);
    ASSERT(sr.count == 0, "should have 0 issues for balanced code");
    syntax_result_free(&sr);
    buffer_destroy(&buf);
    PASS();
}

static void test_unmatched_closing(void)
{
    TEST(unmatched_closing_bracket);
    Buffer buf = buffer_create();
    line_append_str(&buf.lines[0], "x = foo)", 9);

    SyntaxResult sr = syntax_check_buffer(&buf);
    ASSERT(sr.count >= 1, "should detect unmatched ')'");
    ASSERT(sr.issues[0].level == SYNTAX_ERROR, "should be an error");
    syntax_result_free(&sr);
    buffer_destroy(&buf);
    PASS();
}

static void test_unmatched_opening(void)
{
    TEST(unmatched_opening_bracket);
    Buffer buf = buffer_create();
    line_append_str(&buf.lines[0], "if (x > 0 {", 12);

    SyntaxResult sr = syntax_check_buffer(&buf);
    ASSERT(sr.count >= 1, "should detect unmatched '{'");
    int found_brace = 0;
    for (size_t i = 0; i < sr.count; i++) {
        if (sr.issues[i].level == SYNTAX_ERROR) found_brace = 1;
    }
    ASSERT(found_brace, "should report an error for unmatched '{'");
    syntax_result_free(&sr);
    buffer_destroy(&buf);
    PASS();
}

static void test_unclosed_single_quote(void)
{
    TEST(unclosed_single_quote);
    Buffer buf = buffer_create();
    line_append_str(&buf.lines[0], "char c = 'a;", 13);

    SyntaxResult sr = syntax_check_buffer(&buf);
    ASSERT(sr.count >= 1, "should detect unclosed single quote");
    int found_quote = 0;
    for (size_t i = 0; i < sr.count; i++) {
        if (sr.issues[i].level == SYNTAX_ERROR) found_quote = 1;
    }
    ASSERT(found_quote, "should report an error for unclosed quote");
    syntax_result_free(&sr);
    buffer_destroy(&buf);
    PASS();
}

static void test_unclosed_double_quote(void)
{
    TEST(unclosed_double_quote);
    Buffer buf = buffer_create();
    line_append_str(&buf.lines[0], "puts(\"hello);", 14);

    SyntaxResult sr = syntax_check_buffer(&buf);
    ASSERT(sr.count >= 1, "should detect unclosed double quote");
    syntax_result_free(&sr);
    buffer_destroy(&buf);
    PASS();
}

static void test_escaped_quote(void)
{
    TEST(escaped_quote_ok);
    Buffer buf = buffer_create();
    line_append_str(&buf.lines[0], "char *s = \"hello \\\"world\\\"\";", 28);

    SyntaxResult sr = syntax_check_buffer(&buf);
    /* Escaped quotes should not cause false positives */
    ASSERT(sr.count == 0, "escaped quotes should not be flagged");
    syntax_result_free(&sr);
    buffer_destroy(&buf);
    PASS();
}

static void test_nested_brackets(void)
{
    TEST(nested_brackets_ok);
    Buffer buf = buffer_create();
    line_append_str(&buf.lines[0], "arr[func(x)] = {1, 2, 3};", 26);

    SyntaxResult sr = syntax_check_buffer(&buf);
    ASSERT(sr.count == 0, "nested balanced brackets should be OK");
    syntax_result_free(&sr);
    buffer_destroy(&buf);
    PASS();
}

static void test_mixed_mismatch(void)
{
    TEST(mixed_bracket_mismatch);
    Buffer buf = buffer_create();
    line_append_str(&buf.lines[0], "arr[func(x)}", 13);

    SyntaxResult sr = syntax_check_buffer(&buf);
    ASSERT(sr.count >= 1, "mismatched [ vs } should be detected");
    syntax_result_free(&sr);
    buffer_destroy(&buf);
    PASS();
}

static void test_multiline_brackets(void)
{
    TEST(multiline_bracket_matching);
    Buffer buf = buffer_create();
    line_append_str(&buf.lines[0], "int main() {", 13);
    buffer_insert_line(&buf, 1);
    line_append_str(&buf.lines[1], "    return 0;", 14);
    buffer_insert_line(&buf, 2);
    line_append_str(&buf.lines[2], "}", 1);

    SyntaxResult sr = syntax_check_buffer(&buf);
    ASSERT(sr.count == 0, "multi-line balanced braces should be OK");
    syntax_result_free(&sr);
    buffer_destroy(&buf);
    PASS();
}

static void test_line_colors(void)
{
    TEST(syntax_line_colors);
    Buffer buf = buffer_create();
    line_append_str(&buf.lines[0], "x = foo)", 9);

    SyntaxResult sr = syntax_check_buffer(&buf);

    TermColor colors[9];
    syntax_line_colors(&sr, 0, colors, 9);

    /* The ')' at position 8 should be red */
    int found_red = 0;
    for (size_t i = 0; i < 9; i++) {
        if (colors[i] == TERM_COLOR_RED) found_red = 1;
    }
    ASSERT(found_red, "unmatched ')' should be colored red");

    syntax_result_free(&sr);
    buffer_destroy(&buf);
    PASS();
}

static void test_status_summary(void)
{
    TEST(syntax_status_summary);
    Buffer buf = buffer_create();
    line_append_str(&buf.lines[0], "hello", 5);

    SyntaxResult sr = syntax_check_buffer(&buf);
    char summary[32];
    syntax_status_summary(&sr, summary, sizeof(summary));
    ASSERT(strcmp(summary, "OK") == 0, "empty code should be OK");
    syntax_result_free(&sr);

    /* With error */
    line_append_str(&buf.lines[0], ")", 1);
    sr = syntax_check_buffer(&buf);
    syntax_status_summary(&sr, summary, sizeof(summary));
    ASSERT(strstr(summary, "ERR") != NULL, "should show ERR");
    syntax_result_free(&sr);

    buffer_destroy(&buf);
    PASS();
}

static void test_brackets_inside_quotes(void)
{
    TEST(brackets_inside_quotes_ignored);
    Buffer buf = buffer_create();
    line_append_str(&buf.lines[0], "puts(\"[({})]\");", 16);

    SyntaxResult sr = syntax_check_buffer(&buf);
    /* Brackets inside quotes should not be checked */
    ASSERT(sr.count == 0, "brackets inside strings should be ignored");
    syntax_result_free(&sr);
    buffer_destroy(&buf);
    PASS();
}

static void test_empty_buffer(void)
{
    TEST(syntax_empty_buffer);
    Buffer buf = buffer_create();

    SyntaxResult sr = syntax_check_buffer(&buf);
    ASSERT(sr.count == 0, "empty buffer should have 0 issues");
    syntax_result_free(&sr);
    buffer_destroy(&buf);
    PASS();
}

int main(void)
{
    printf("=== MINI Syntax Test Suite ===\n\n");

    printf("[Bracket Matching]\n");
    test_balanced_parens();
    test_unmatched_closing();
    test_unmatched_opening();
    test_nested_brackets();
    test_mixed_mismatch();
    test_multiline_brackets();

    printf("\n[Quote Detection]\n");
    test_unclosed_single_quote();
    test_unclosed_double_quote();
    test_escaped_quote();
    test_brackets_inside_quotes();

    printf("\n[Color/Status]\n");
    test_line_colors();
    test_status_summary();

    printf("\n[Edge Cases]\n");
    test_empty_buffer();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
