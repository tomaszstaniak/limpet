#include "../src/file_drop.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *quote(const char *path)
{
    static char out[XLINE_CAPACITY];
    return file_drop_quote(path, out, sizeof out) ? out : NULL;
}

static void line(XLineEditor *e, const char *text, size_t cursor)
{
    memset(e, 0, sizeof *e);
    assert(line_editor_insert(e, text, strlen(text)));
    e->cursor = e->anchor = cursor;
}

static int cuts(const char *line) { return file_drop_shell_cuts(line, strlen(line)); }

static XDropResult drop(XLineEditor *e, const char *a, const char *b)
{
    const char *paths[2] = { a, b };
    size_t failed;
    return file_drop_insert(e, paths, b ? 2 : 1, &failed);
}

int main(void)
{
    XLineEditor e;
    char big[XLINE_CAPACITY + 8];
    const char *many[XDROP_MAX_FILES + 1];
    size_t i, failed;

    /* AROS Shell arguments: plain paths as they are; the rest quoted, with
       " * $ ` escaped by * inside the quotes (ReadArgs, the C start-up and
       the Shell's $ and ` expansion). Never POSIX quoting. */
    assert(!strcmp(quote("RAM:notes.txt"), "RAM:notes.txt"));
    assert(!strcmp(quote("Work:"), "Work:"));
    assert(!strcmp(quote("Ram Disk:a b/c"), "\"Ram Disk:a b/c\""));
    assert(!strcmp(quote("RAM:q\"uote"), "\"RAM:q*\"uote\""));
    assert(!strcmp(quote("RAM:st*ar"), "\"RAM:st**ar\""));
    assert(!strcmp(quote("RAM:$HOME"), "\"RAM:*$HOME\""));
    assert(!strcmp(quote("RAM:`date`"), "\"RAM:*`date*`\""));
    assert(!strcmp(quote("RAM:a;b"), "\"RAM:a;b\""));
    assert(!strcmp(quote("RAM:>x"), "\"RAM:>x\""));
    assert(!strcmp(quote("RAM:k=v"), "\"RAM:k=v\""));
    assert(!strcmp(quote("RAM:#?(a|b)"), "\"RAM:#?(a|b)\""));
    assert(!strcmp(quote("RAM:back\\slash"), "RAM:back\\slash"));   /* not an escape on AROS */
    assert(!strcmp(quote("RAM:it's"), "RAM:it's"));
    assert(!strcmp(quote("RAM:*n"), "\"RAM:**n\""));                  /* never *N: ** then n */
    assert(!strcmp(quote("RAM:z\xc5\xbc\xc3\xb3\xc5\x82w"), "RAM:z\xc5\xbc\xc3\xb3\xc5\x82w"));
    /* refused: controls, invalid UTF-8 (a Latin-1 name), empty */
    assert(!quote("RAM:a\nb") && !quote("RAM:\x1b") && !quote("RAM:\x7f"));
    assert(!quote("RAM:caf\xe9") && !quote(""));
    assert(!quote("RAM:\xc2\x85"));                                    /* C1 NEL */
    {
        char small[8];
        assert(!file_drop_quote("RAM:abcd", small, sizeof small));     /* does not fit */
        assert(file_drop_quote("RAM:abc", small, sizeof small) == 7);
    }

    /* at the cursor, spaced from the text around it */
    line(&e, "", 0);
    assert(drop(&e, "RAM:a", NULL) == XDROP_OK && !strcmp(e.text, "RAM:a ") && e.cursor == 6);
    line(&e, "Type", 4);
    assert(drop(&e, "RAM:a", "RAM:b c") == XDROP_OK && !strcmp(e.text, "Type RAM:a \"RAM:b c\" "));
    line(&e, "Copy  TO RAM:", 5);                                     /* between two spaces */
    assert(drop(&e, "S:x", NULL) == XDROP_OK && !strcmp(e.text, "Copy S:x TO RAM:"));
    line(&e, "Copy TO RAM:", 4);                                      /* before a space */
    assert(drop(&e, "S:x", NULL) == XDROP_OK && !strcmp(e.text, "Copy S:x TO RAM:"));
    line(&e, "List X", 5);                                            /* inside a word */
    assert(drop(&e, "S:x", NULL) == XDROP_OK && !strcmp(e.text, "List S:x X") && e.cursor == 9);
    /* a selection stays: nothing typed is lost */
    line(&e, "Echo keep", 9);
    e.anchor = 5; e.selecting = 1;
    assert(drop(&e, "S:x", NULL) == XDROP_OK && !strcmp(e.text, "Echo keep S:x "));
    /* order is kept */
    line(&e, "", 0);
    assert(drop(&e, "RAM:2", "RAM:1") == XDROP_OK && !strcmp(e.text, "RAM:2 RAM:1 "));

    /* all or nothing */
    line(&e, "Echo", 4);
    e.anchor = 0; e.selecting = 1;
    assert(drop(&e, "RAM:ok", "RAM:caf\xe9") == XDROP_BAD_NAME);
    assert(!strcmp(e.text, "Echo") && e.cursor == 4 && e.anchor == 0 && e.selecting);
    {
        const char *paths[2] = { "RAM:ok", "RAM:\x01" };
        assert(file_drop_insert(&e, paths, 2, &failed) == XDROP_BAD_NAME && failed == 1);
    }
    memset(big, 'a', sizeof big); memcpy(big, "RAM:", 4); big[XLINE_CAPACITY - 10] = 0;
    line(&e, "Echo 1234567890", 15);
    assert(drop(&e, big, NULL) == XDROP_TOO_LONG && !strcmp(e.text, "Echo 1234567890"));
    big[XLINE_CAPACITY - 30] = 0;
    assert(drop(&e, big, NULL) == XDROP_OK && e.length < XLINE_CAPACITY);
    for (i = 0; i <= XDROP_MAX_FILES; i++) many[i] = "R:x";
    line(&e, "", 0);
    assert(file_drop_insert(&e, many, XDROP_MAX_FILES + 1, &failed) == XDROP_TOO_MANY && !e.length);
    assert(file_drop_insert(&e, many, XDROP_MAX_FILES, &failed) == XDROP_OK);

    /* The Shell's readLine counts the escaped *" as a quote: after a name
       with one quote, a quoted ; is taken for a comment and the line ends
       there. Seen on AROS One: argv stopped at "semi". Refused, all or
       nothing; each name alone is fine. */
    assert(!cuts("Echo \"a;b\""));
    assert(cuts("Echo \"q*\"d\" \"a;b\""));
    assert(!cuts("Echo \"q*\"*\"d\" \"a;b\""));                  /* two escaped quotes: even */
    assert(cuts("Echo \"q*\"d\" ; note"));                         /* a comment readLine would keep */
    assert(!cuts("Echo x ; \"q*\"d\" \"a;b\""));                 /* after a real comment */
    line(&e, "Echo", 4);
    assert(drop(&e, "RAM:q\"d", "RAM:a;b") == XDROP_SHELL_CUT && !strcmp(e.text, "Echo"));
    assert(drop(&e, "RAM:a;b", "RAM:q\"d") == XDROP_OK);                      /* the ; comes first */
    line(&e, "Echo \"x;y\"", 0);
    assert(drop(&e, "RAM:q\"d", NULL) == XDROP_SHELL_CUT);                    /* text already on the line */
    line(&e, "Echo", 4);
    assert(drop(&e, "RAM:q\"d", NULL) == XDROP_OK);

    /* not while Ctrl-R searches: the line shows the match, not the text */
    line(&e, "Echo", 4);
    line_editor_search_start(&e);
    assert(drop(&e, "RAM:a", NULL) == XDROP_SEARCHING && e.searching);
    line_editor_search_cancel(&e);
    assert(!strcmp(e.text, "Echo"));

    puts("ok file_drop");
    return 0;
}
