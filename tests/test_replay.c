#include "replay_case.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "replay_fixtures.h"

static XTermCell cells[80 * 160], history[256 * 160];
static uint32_t digest = UINT32_C(2166136261);
static unsigned runs;

/* Canonical byte order, independent of host ABI and structure padding. */
static void hash_number(unsigned value)
{
    unsigned i;
    for (i = 0; i < 4; ++i) {
        digest ^= (value >> (i * 8)) & 255;
        digest *= UINT32_C(16777619);
    }
}

static int check(const char *name, size_t split, unsigned step,
                 const XTermScreen *s, const ReplaySnapshot *expected)
{
    unsigned row, column, scalar;
#define FIELD(field) do { \
    if (s->field != expected->field) { \
        fprintf(stderr, "FAIL %s split=%lu step=%u: %s expected=%u actual=%u\n", \
                name, (unsigned long)split, step, #field, expected->field, s->field); \
        return 1; \
    } \
    hash_number(s->field); \
} while (0)
    FIELD(rows); FIELD(columns); FIELD(cursor_row); FIELD(cursor_column);
    FIELD(history_count); FIELD(view_offset); FIELD(wrap_pending);
#undef FIELD
    if (s->style.fg != expected->style.fg ||
        s->style.bg != expected->style.bg ||
        s->style.bold != expected->style.bold) {
        fprintf(stderr, "FAIL %s: current style mismatch\n", name);
        return 1;
    }
    hash_number(s->style.fg); hash_number(s->style.bg); hash_number(s->style.bold);
    for (row = 0; row < s->rows; ++row) {
        for (column = 0; column < s->columns; ++column) {
            XTermCell actual = screen_view_cell(s, row, column);
            XTermCell want = expected->cells[(size_t)row * s->columns + column];
            if (actual.ch != want.ch || actual.fg != want.fg ||
                actual.bg != want.bg || actual.bold != want.bold || actual.width != want.width ||
                actual.length != want.length || memcmp(actual.suffix, want.suffix, sizeof actual.suffix)) {
                fprintf(stderr, "FAIL %s split=%lu step=%u cell=(%u,%u): "
                        "expected=%u/%u/%u/%u/%u length=%u actual=%u/%u/%u/%u/%u length=%u\n",
                        name, (unsigned long)split, step, row, column,
                        want.ch, want.fg, want.bg, want.bold, want.width, want.length,
                        actual.ch, actual.fg, actual.bg, actual.bold, actual.width, actual.length);
                return 1;
            }
            hash_number(actual.ch); hash_number(actual.fg);
            hash_number(actual.bg); hash_number(actual.bold);
            hash_number(actual.width);
            hash_number(actual.length);
            for (scalar = 1; scalar < actual.length; ++scalar)
                hash_number(actual.suffix[scalar - 1]);
        }
    }
    return 0;
}

static int run_case(const ReplayCase *fixture, size_t split, int bytewise)
{
    XTermScreen s;
    size_t i;
    if (screen_init(&s, cells, sizeof cells / sizeof cells[0],
                    fixture->initial.rows, fixture->initial.columns) != 0)
        return 1;
    if (fixture->history_lines &&
        screen_attach_history(&s, history,
                              (size_t)fixture->history_lines * s.columns) != 0)
        return 1;
    if (bytewise) {
        for (i = 0; i < fixture->length; ++i)
            screen_write(&s, fixture->bytes + i, 1);
    } else {
        screen_write(&s, fixture->bytes, split);
        screen_write(&s, fixture->bytes + split, fixture->length - split);
    }
    if (check(fixture->name, split, 0, &s, &fixture->initial)) return 1;
    for (i = 0; i < fixture->action_count; ++i) {
        const ReplayAction *action = &fixture->actions[i];
        if (action->operation == 1) {
            if (screen_resize(&s, action->expected.rows, action->expected.columns) != 0)
                return 1;
        } else screen_scroll_view(&s, action->argument);
        if (check(fixture->name, split, (unsigned)i + 1, &s, &action->expected)) return 1;
    }
    ++runs;
    return 0;
}

int main(void)
{
    size_t i, split, deferred, count = sizeof replay_cases / sizeof replay_cases[0];
    for (i = 0; i < count; ++i) {
        const ReplayCase *fixture = &replay_cases[i];
        for (split = 0; split <= fixture->length; ++split)
            if (run_case(fixture, split, 0)) return 1;
        if (run_case(fixture, 0, 1)) return 1;
        printf("PASS %s\n", fixture->name);
    }
    /* Through a variable: with no deferred cases GCC's -Wtype-limits rejects
       `i < 0u`, and the fixture checker builds that case on purpose. */
    deferred = DEFERRED_COUNT;
    for (i = 0; i < deferred; ++i) printf("DEFERRED %s\n", deferred_cases[i]);
    printf("replay corpus: %lu passed, %u deferred, %u runs\n",
           (unsigned long)count, DEFERRED_COUNT, runs);
    printf("snapshot hash: %08lx\n", (unsigned long)digest);
    return 0;
}
