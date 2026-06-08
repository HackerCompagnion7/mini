/*
 * MINI - test_core.c
 * Automated tests for buffer, file, and search modules.
 * No terminal required — pure logic testing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "buffer.h"
#include "file.h"
#include "search.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %-50s", #name); } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ---- Buffer Tests ---- */

static void test_buffer_create(void)
{
    TEST(buffer_create);
    Buffer buf = buffer_create();
    ASSERT(buf.lines != NULL, "lines should not be NULL");
    ASSERT(buf.count == 1, "should start with 1 empty line");
    ASSERT(buf.lines[0].data != NULL, "first line data should exist");
    ASSERT(buf.lines[0].len == 0, "first line should be empty");
    buffer_destroy(&buf);
    PASS();
}

static void test_buffer_insert_line(void)
{
    TEST(buffer_insert_line);
    Buffer buf = buffer_create();

    int r = buffer_insert_line(&buf, 1);
    ASSERT(r == 0, "insert should succeed");
    ASSERT(buf.count == 2, "should have 2 lines");

    buffer_insert_line(&buf, 1);
    ASSERT(buf.count == 3, "should have 3 lines");

    buffer_destroy(&buf);
    PASS();
}

static void test_buffer_delete_line(void)
{
    TEST(buffer_delete_line);
    Buffer buf = buffer_create();
    buffer_insert_line(&buf, 1);
    buffer_insert_line(&buf, 2);

    ASSERT(buf.count == 3, "should have 3 lines before delete");

    int r = buffer_delete_line(&buf, 1);
    ASSERT(r == 0, "delete should succeed");
    ASSERT(buf.count == 2, "should have 2 lines after delete");

    buffer_destroy(&buf);
    PASS();
}

static void test_line_insert_char(void)
{
    TEST(line_insert_char);
    Buffer buf = buffer_create();
    Line *ln = buffer_get_line(&buf, 0);

    line_insert_char(ln, 0, "H", 1);
    line_insert_char(ln, 1, "i", 1);
    line_insert_char(ln, 2, "!", 1);

    ASSERT(ln->len == 3, "line length should be 3");
    ASSERT(strcmp(ln->data, "Hi!") == 0, "line content should be 'Hi!'");

    buffer_destroy(&buf);
    PASS();
}

static void test_line_delete_char(void)
{
    TEST(line_delete_char_range);
    Buffer buf = buffer_create();
    Line *ln = buffer_get_line(&buf, 0);

    line_insert_char(ln, 0, "A", 1);
    line_insert_char(ln, 1, "B", 1);
    line_insert_char(ln, 2, "C", 1);

    line_delete_char_range(ln, 1, 1); /* delete 'B' */

    ASSERT(ln->len == 2, "line length should be 2");
    ASSERT(strcmp(ln->data, "AC") == 0, "line content should be 'AC'");

    buffer_destroy(&buf);
    PASS();
}

static void test_line_append_str(void)
{
    TEST(line_append_str);
    Buffer buf = buffer_create();
    Line *ln = buffer_get_line(&buf, 0);

    line_append_str(ln, "Hello", 5);
    line_append_str(ln, " World", 6);

    ASSERT(ln->len == 11, "line length should be 11");
    ASSERT(strcmp(ln->data, "Hello World") == 0, "line content should be 'Hello World'");

    buffer_destroy(&buf);
    PASS();
}

/* ---- UTF-8 Tests ---- */

static void test_utf8_char_len(void)
{
    TEST(utf8_char_len);
    ASSERT(utf8_char_len('A') == 1, "ASCII should be 1 byte");
    ASSERT(utf8_char_len(0xC3) == 2, "2-byte UTF-8 leader");
    ASSERT(utf8_char_len(0xE2) == 3, "3-byte UTF-8 leader");
    ASSERT(utf8_char_len(0xF0) == 4, "4-byte UTF-8 leader");
    PASS();
}

static void test_utf8_insert_french(void)
{
    TEST(utf8_insert_french_accent);
    Buffer buf = buffer_create();
    Line *ln = buffer_get_line(&buf, 0);

    /* Insert 'é' (0xC3 0xA9) */
    line_insert_char(ln, 0, "\xC3\xA9", 2);

    ASSERT(ln->len == 2, "line byte length should be 2");
    ASSERT(utf8_char_count(ln->data, ln->len) == 1, "should be 1 character");

    /* Insert 'à' after it */
    line_insert_char(ln, 2, "\xC3\xA0", 2);

    ASSERT(ln->len == 4, "line byte length should be 4");
    ASSERT(utf8_char_count(ln->data, ln->len) == 2, "should be 2 characters");

    buffer_destroy(&buf);
    PASS();
}

static void test_utf8_prev_next(void)
{
    TEST(utf8_prev_next_char_pos);
    const char *text = "a\xC3\xA9""b"; /* a, é, b */
    size_t len = 4;

    /* Position 0: 'a' */
    ASSERT(utf8_next_char_pos(text, 0, len) == 1, "next from 'a' should be 1");
    /* Position 1: start of 'é' */
    ASSERT(utf8_next_char_pos(text, 1, len) == 3, "next from 'é' should be 3");
    /* Position 3: 'b' */
    ASSERT(utf8_next_char_pos(text, 3, len) == 4, "next from 'b' should be 4");

    /* Backwards */
    ASSERT(utf8_prev_char_pos(text, 1) == 0, "prev from pos 1 should be 0");
    ASSERT(utf8_prev_char_pos(text, 3) == 1, "prev from pos 3 should be 1 (start of é)");
    ASSERT(utf8_prev_char_pos(text, 4) == 3, "prev from pos 4 should be 3");

    PASS();
}

/* ---- File I/O Tests ---- */

static void test_file_save_load(void)
{
    TEST(file_save_and_load);
    Buffer buf = buffer_create();

    /* Create content */
    Line *ln = buffer_get_line(&buf, 0);
    line_append_str(ln, "First line", 10);
    buffer_insert_line(&buf, 1);
    line_append_str(&buf.lines[1], "Second line", 11);
    buffer_insert_line(&buf, 2);
    line_append_str(&buf.lines[2], "Third line", 10);

    buf.filepath = strdup("/tmp/mini_test_file.txt");

    int r = file_save(&buf);
    ASSERT(r == 0, "save should succeed");

    /* Load into new buffer */
    Buffer buf2 = buffer_create();
    r = file_load(&buf2, "/tmp/mini_test_file.txt");
    ASSERT(r == 0, "load should succeed");
    ASSERT(buf2.count == 3, "should have 3 lines");
    ASSERT(strcmp(buf2.lines[0].data, "First line") == 0, "first line matches");
    ASSERT(strcmp(buf2.lines[1].data, "Second line") == 0, "second line matches");
    ASSERT(strcmp(buf2.lines[2].data, "Third line") == 0, "third line matches");
    ASSERT(buf2.modified == false, "loaded file should not be modified");

    buffer_destroy(&buf);
    buffer_destroy(&buf2);
    remove("/tmp/mini_test_file.txt");
    PASS();
}

static void test_file_utf8(void)
{
    TEST(file_save_load_utf8);
    Buffer buf = buffer_create();
    Line *ln = buffer_get_line(&buf, 0);
    /* French: "Café résumé" */
    line_append_str(ln, "Caf\xC3\xA9 r\xC3\xA9sum\xC3\xA9", 13);

    buf.filepath = strdup("/tmp/mini_test_utf8.txt");
    file_save(&buf);

    Buffer buf2 = buffer_create();
    file_load(&buf2, "/tmp/mini_test_utf8.txt");
    ASSERT(buf2.count == 1, "should have 1 line");
    ASSERT(buf2.lines[0].len == 13, "byte length should be 13");
    ASSERT(memcmp(buf2.lines[0].data, "Caf\xC3\xA9 r\xC3\xA9sum\xC3\xA9", 13) == 0,
           "UTF-8 content should round-trip correctly");

    buffer_destroy(&buf);
    buffer_destroy(&buf2);
    remove("/tmp/mini_test_utf8.txt");
    PASS();
}

static void test_file_empty(void)
{
    TEST(file_empty_file);
    /* Create empty file */
    FILE *f = fopen("/tmp/mini_test_empty.txt", "w");
    fclose(f);

    Buffer buf = buffer_create();
    int r = file_load(&buf, "/tmp/mini_test_empty.txt");
    ASSERT(r == 0, "load empty file should succeed");
    ASSERT(buf.count == 1, "should have at least 1 line");
    ASSERT(buf.lines[0].len == 0, "line should be empty");

    buffer_destroy(&buf);
    remove("/tmp/mini_test_empty.txt");
    PASS();
}

static void test_file_nonexistent(void)
{
    TEST(file_nonexistent);
    Buffer buf = buffer_create();
    int r = file_load(&buf, "/tmp/mini_nonexistent_12345.txt");
    ASSERT(r == -1, "loading nonexistent file should fail");

    buffer_destroy(&buf);
    PASS();
}

static void test_file_crlf(void)
{
    TEST(file_crlf_handling);
    /* Create file with CRLF line endings */
    FILE *f = fopen("/tmp/mini_test_crlf.txt", "wb");
    fprintf(f, "Line1\r\nLine2\r\n");
    fclose(f);

    Buffer buf = buffer_create();
    file_load(&buf, "/tmp/mini_test_crlf.txt");
    ASSERT(buf.count == 2, "should have 2 lines");
    ASSERT(strcmp(buf.lines[0].data, "Line1") == 0, "first line without CR");
    ASSERT(strcmp(buf.lines[1].data, "Line2") == 0, "second line without CR");

    buffer_destroy(&buf);
    remove("/tmp/mini_test_crlf.txt");
    PASS();
}

/* ---- Search Tests ---- */

static void test_search_forward(void)
{
    TEST(search_forward);
    Buffer buf = buffer_create();
    line_append_str(&buf.lines[0], "Hello World", 11);
    buffer_insert_line(&buf, 1);
    line_append_str(&buf.lines[1], "Goodbye World", 13);

    SearchMatch match;
    int found = search_find(&buf, "World", 0, 0, SEARCH_FORWARD, &match);
    ASSERT(found == 1, "should find 'World'");
    ASSERT(match.line == 0, "should be on line 0");
    ASSERT(match.byte_pos == 6, "should be at byte position 6");

    buffer_destroy(&buf);
    PASS();
}

static void test_search_multiline(void)
{
    TEST(search_multiline);
    Buffer buf = buffer_create();
    line_append_str(&buf.lines[0], "First", 5);
    buffer_insert_line(&buf, 1);
    line_append_str(&buf.lines[1], "Second", 6);
    buffer_insert_line(&buf, 2);
    line_append_str(&buf.lines[2], "Third", 5);

    SearchMatch match;
    int found = search_find(&buf, "Third", 0, 0, SEARCH_FORWARD, &match);
    ASSERT(found == 1, "should find 'Third'");
    ASSERT(match.line == 2, "should be on line 2");

    buffer_destroy(&buf);
    PASS();
}

static void test_search_not_found(void)
{
    TEST(search_not_found);
    Buffer buf = buffer_create();
    line_append_str(&buf.lines[0], "Hello", 5);

    SearchMatch match;
    int found = search_find(&buf, "xyz", 0, 0, SEARCH_FORWARD, &match);
    ASSERT(found == 0, "should not find 'xyz'");

    buffer_destroy(&buf);
    PASS();
}

static void test_search_utf8(void)
{
    TEST(search_utf8_pattern);
    Buffer buf = buffer_create();
    /* "Café résumé" */
    line_append_str(&buf.lines[0], "Caf\xC3\xA9 r\xC3\xA9sum\xC3\xA9", 13);

    SearchMatch match;
    /* Search for 'é' */
    int found = search_find(&buf, "\xC3\xA9", 0, 0, SEARCH_FORWARD, &match);
    ASSERT(found == 1, "should find UTF-8 pattern");
    ASSERT(match.byte_pos == 3, "first é at byte 3");

    buffer_destroy(&buf);
    PASS();
}

static void test_search_wrap(void)
{
    TEST(search_find_next_wrap);
    Buffer buf = buffer_create();
    line_append_str(&buf.lines[0], "aaa bbb aaa", 11);

    SearchMatch match;
    int found = search_find(&buf, "aaa", 0, 0, SEARCH_FORWARD, &match);
    ASSERT(found == 1, "should find first 'aaa'");
    ASSERT(match.byte_pos == 0, "first at pos 0");

    /* Find next after first */
    found = search_find_next(&buf, "aaa", match.line, match.byte_pos,
                             SEARCH_FORWARD, 1, &match);
    ASSERT(found == 1, "should find second 'aaa'");
    ASSERT(match.byte_pos == 8, "second at pos 8");

    buffer_destroy(&buf);
    PASS();
}

/* ---- Stress Test ---- */

static void test_large_file(void)
{
    TEST(large_file_1000_lines);
    Buffer buf = buffer_create();

    /* Create 1000 lines */
    for (int i = 0; i < 1000; i++) {
        if (i > 0) buffer_insert_line(&buf, (size_t)i);
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "Line %d: some content here", i);
        line_append_str(&buf.lines[i], tmp, strlen(tmp));
    }

    ASSERT(buf.count == 1000, "should have 1000 lines");

    /* Save and reload */
    buf.filepath = strdup("/tmp/mini_test_large.txt");
    file_save(&buf);

    Buffer buf2 = buffer_create();
    file_load(&buf2, "/tmp/mini_test_large.txt");
    ASSERT(buf2.count == 1000, "reloaded should have 1000 lines");
    ASSERT(strcmp(buf2.lines[999].data, "Line 999: some content here") == 0,
           "last line should match");

    buffer_destroy(&buf);
    buffer_destroy(&buf2);
    remove("/tmp/mini_test_large.txt");
    PASS();
}

/* ---- Main ---- */

int main(void)
{
    printf("=== MINI Test Suite ===\n\n");

    printf("[Buffer Tests]\n");
    test_buffer_create();
    test_buffer_insert_line();
    test_buffer_delete_line();
    test_line_insert_char();
    test_line_delete_char();
    test_line_append_str();

    printf("\n[UTF-8 Tests]\n");
    test_utf8_char_len();
    test_utf8_insert_french();
    test_utf8_prev_next();

    printf("\n[File I/O Tests]\n");
    test_file_save_load();
    test_file_utf8();
    test_file_empty();
    test_file_nonexistent();
    test_file_crlf();

    printf("\n[Search Tests]\n");
    test_search_forward();
    test_search_multiline();
    test_search_not_found();
    test_search_utf8();
    test_search_wrap();

    printf("\n[Stress Tests]\n");
    test_large_file();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
