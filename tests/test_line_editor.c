#include "../src/line_editor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XLineEditor editor, saved;
static void insert(const char *text)
{ assert(line_editor_insert(&editor, text, strlen(text))); }
static void expect(const char *text, size_t cursor)
{
    assert(!strcmp(editor.text, text));
    assert(editor.length == strlen(text));
    assert(editor.cursor == cursor);
}
static void commit(const char *expected)
{
    char out[XLINE_CAPACITY + 1];
    size_t n = strlen(expected);
    memset(out, '?', sizeof out);
    assert(line_editor_commit(&editor, out, n) == n);
    assert(!memcmp(out, expected, n) && out[n] == '?');
    expect("", 0);
}
static void typed(const char *text)
{ assert(line_editor_search_insert(&editor, text, strlen(text))); }
static const char *label(size_t *cursor)
{
    static char out[XLINE_CAPACITY];
    assert(line_editor_search_label(&editor, out, sizeof out, cursor));
    return out;
}

/* Ctrl-R: reverse search through the command history. */
static void test_search(void)
{
    size_t c;
    char out[XLINE_CAPACITY];
    memset(&editor, 0, sizeof editor);
    insert("Dir RAM:"); commit("Dir RAM:\n");
    insert("Echo \xc5\xbc\xc3\xb3\xc5\x82w"); commit("Echo \xc5\xbc\xc3\xb3\xc5\x82w\n");
    insert("List SYS:C"); commit("List SYS:C\n");
    insert("Echo hello"); commit("Echo hello\n");
    insert("echo HELLO"); commit("echo HELLO\n");
    insert("Echo hello"); commit("Echo hello\n");

    /* Start: the typed line is kept and shown until the query matches. */
    insert("Copy a b"); line_editor_action(&editor, XLINE_LEFT);
    assert(!line_editor_search_start(&editor) && editor.searching);
    expect("Copy a b", 7);
    assert(!strcmp(label(&c), "(reverse-i-search)`': Copy a b"));
    /* Typing narrows; the newest match first, case ignored for ASCII. */
    typed("ech"); expect("Echo hello", 10);
    assert(!strcmp(label(&c), "(reverse-i-search)`ech': Echo hello") && c == 25);
    typed("o h");
    expect("Echo hello", 10);
    /* Ctrl-R again: older, and an identical older text is skipped. */
    assert(line_editor_search_start(&editor)); expect("echo HELLO", 10);
    assert(line_editor_search_start(&editor)); expect("Echo hello", 10);
    assert(editor.match == 3);
    /* No older one: failed, the match shown stays. */
    assert(!line_editor_search_start(&editor) && editor.search_failed);
    expect("Echo hello", 10);
    assert(!strncmp(label(&c), "(failed reverse-i-search)`echo h': Echo hello", 46));
    /* Backspace widens the query and starts again from the newest. */
    assert(line_editor_search_backspace(&editor) && !editor.search_failed);
    expect("Echo hello", 10); assert(editor.match == 1);
    /* Esc: the line, cursor and selection from before come back. */
    line_editor_search_cancel(&editor);
    assert(!editor.searching); expect("Copy a b", 7);

    /* Unicode query; Enter keeps the match for editing, nothing is run. */
    assert(!line_editor_search_start(&editor));
    typed("\xc5\xbc\xc3\xb3");
    expect("Echo \xc5\xbc\xc3\xb3\xc5\x82w", 12);
    line_editor_search_accept(&editor);
    assert(!editor.searching && !editor.browsing);
    expect("Echo \xc5\xbc\xc3\xb3\xc5\x82w", 12);
    insert("!"); expect("Echo \xc5\xbc\xc3\xb3\xc5\x82w!", 13);
    /* Backspace over a two-byte query character removes it whole. */
    assert(!line_editor_search_start(&editor));
    typed("\xc5\x82");
    assert(line_editor_search_backspace(&editor) && !editor.query_length);
    expect("Echo \xc5\xbc\xc3\xb3\xc5\x82w!", 13);      /* the line from before */
    assert(!line_editor_search_backspace(&editor));
    /* Nothing found from the start: the typed line stays and is kept. */
    typed("zzz");
    assert(editor.search_failed); expect("Echo \xc5\xbc\xc3\xb3\xc5\x82w!", 13);
    line_editor_search_accept(&editor);
    expect("Echo \xc5\xbc\xc3\xb3\xc5\x82w!", 13);
    /* Invalid query input changes nothing. */
    assert(!line_editor_search_start(&editor));
    assert(!line_editor_search_insert(&editor, "a\033", 2));
    assert(!line_editor_search_insert(&editor, "\xc5", 1));
    assert(!editor.query_length);
    memset(out, 'x', XLINE_QUERY_MAX);
    assert(!line_editor_search_insert(&editor, out, XLINE_QUERY_MAX));
    assert(line_editor_search_insert(&editor, out, XLINE_QUERY_MAX - 1));
    assert(!line_editor_search_insert(&editor, "x", 1));
    line_editor_search_cancel(&editor);
    /* Not searching: the search calls do nothing. */
    assert(!line_editor_search_insert(&editor, "a", 1) && !line_editor_search_backspace(&editor));
    assert(!line_editor_search_label(&editor, out, sizeof out, &c));
    /* Started while browsing: Esc goes back to the browsed entry. */
    line_editor_action(&editor, XLINE_CLEAR);
    line_editor_action(&editor, XLINE_PREVIOUS); line_editor_action(&editor, XLINE_PREVIOUS);
    expect("echo HELLO", 10); assert(editor.browsing == 2);
    assert(!line_editor_search_start(&editor)); typed("dir");
    expect("Dir RAM:", 8);
    line_editor_search_cancel(&editor);
    expect("echo HELLO", 10); assert(editor.browsing == 2);
    /* After an accepted match Up starts again from the newest entry. */
    assert(!line_editor_search_start(&editor)); typed("dir"); line_editor_search_accept(&editor);
    assert(line_editor_action(&editor, XLINE_PREVIOUS)); expect("Echo hello", 10);
    /* Clear and commit end a search. */
    assert(!line_editor_search_start(&editor)); line_editor_action(&editor, XLINE_CLEAR);
    assert(!editor.searching);
    /* Only the 32 kept entries are searched; the oldest went. */
    memset(&editor, 0, sizeof editor);
    {
        unsigned i; char line[32];
        for (i = 0; i < XLINE_HISTORY + 4; ++i) {
            snprintf(line, sizeof line, "cmd%02u", i); insert(line);
            snprintf(line, sizeof line, "cmd%02u\n", i); commit(line);
        }
        assert(!line_editor_search_start(&editor)); typed("cmd03");
        assert(editor.search_failed);
        line_editor_search_cancel(&editor);
        assert(!line_editor_search_start(&editor)); typed("cmd04");
        assert(!editor.search_failed); expect("cmd04", 5);
        line_editor_search_cancel(&editor);
    }
    /* A long match is cut at a scalar boundary in the label. */
    {
        char small[40];
        memset(&editor, 0, sizeof editor);
        insert("Echo \xc5\xbc\xc3\xb3\xc5\x82w \xc5\xbc\xc3\xb3\xc5\x82w \xc5\xbc\xc3\xb3\xc5\x82w"); commit("Echo \xc5\xbc\xc3\xb3\xc5\x82w \xc5\xbc\xc3\xb3\xc5\x82w \xc5\xbc\xc3\xb3\xc5\x82w\n");
        assert(!line_editor_search_start(&editor)); typed("E");
        assert(line_editor_search_label(&editor, small, sizeof small, &c) < sizeof small);
        assert(((unsigned char)small[strlen(small) - 1] & 0xc0) != 0xc0);
        line_editor_search_cancel(&editor);
    }
    puts("line editor search PASS: start keeps the line, narrowing, case, repeat with duplicates skipped, "
         "failure visible and harmless, backspace, Esc restores, Enter edits without running, Unicode, "
         "history limit, label bounds");
}

int main(void)
{
    size_t i;
    char full[XLINE_CAPACITY], clusters[128], out[4];
    insert("Eho");
    assert(line_editor_action(&editor, XLINE_HOME));
    assert(line_editor_action(&editor, XLINE_RIGHT));
    insert("c"); expect("Echo", 2);
    assert(line_editor_action(&editor, XLINE_DELETE)); expect("Eco", 2);
    assert(line_editor_action(&editor, XLINE_BACKSPACE)); expect("Eo", 1);
    assert(line_editor_action(&editor, XLINE_END)); expect("Eo", 2);
    assert(!line_editor_action(&editor, XLINE_RIGHT));
    assert(!line_editor_action(&editor, XLINE_DELETE));
    commit("Eo\n");

    /* Each sample must move/delete as one cluster, not bytes or scalars. */
    { const char *samples[] = {"é", "e\xcc\x81", "中", "😀", "👨‍👩‍👧‍👦",
                               "🇵🇱", "👍🏽", "❤️", "1️⃣"};
      for (i=0; i<sizeof samples/sizeof samples[0]; ++i) {
          insert("A"); insert(samples[i]); insert("B");
          assert(line_editor_action(&editor, XLINE_LEFT));
          assert(line_editor_action(&editor, XLINE_BACKSPACE)); expect("AB", 1);
          line_editor_action(&editor, XLINE_CLEAR);
          insert(samples[i]); insert("B"); line_editor_action(&editor, XLINE_HOME);
          assert(line_editor_action(&editor, XLINE_DELETE)); expect("B", 0);
          line_editor_action(&editor, XLINE_CLEAR);
      }
    }
    /* Insertion/deletion can join clusters on either side of the edit. */
    insert("e"); insert("\xcc\x81");
    assert(line_editor_action(&editor, XLINE_LEFT)); expect("e\xcc\x81", 0);
    line_editor_action(&editor, XLINE_DELETE); expect("", 0);
    insert("👨👩"); line_editor_action(&editor, XLINE_LEFT);
    insert("‍"); expect("👨‍👩", strlen("👨‍👩"));
    line_editor_action(&editor, XLINE_BACKSPACE); expect("", 0);
    insert("🇵x🇱"); line_editor_action(&editor, XLINE_HOME);
    line_editor_action(&editor, XLINE_RIGHT); line_editor_action(&editor, XLINE_DELETE);
    expect("🇵🇱", strlen("🇵🇱"));
    line_editor_action(&editor, XLINE_CLEAR);

    insert("keep"); saved=editor;
    { const char *bad[] = {"\033[A", "\r", "\n", "\t", "\x7f", "\xc2\x9b",
                          "\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82"};
      for (i=0; i<sizeof bad/sizeof bad[0]; ++i) {
          assert(!line_editor_insert(&editor,bad[i],strlen(bad[i])));
          assert(!memcmp(&editor,&saved,sizeof editor));
      }
    }
    assert(!line_editor_insert(&editor,"\0",1));
    assert(!line_editor_insert(&editor,NULL,1));
    assert(line_editor_insert(&editor,NULL,0));
    assert(!line_editor_insert(&editor,"x",(size_t)-1));
    assert(!memcmp(&editor,&saved,sizeof editor));
    memset(out,'?',sizeof out);
    assert(!line_editor_commit(&editor,out,sizeof out));
    assert(!memcmp(out,"????",4) && !memcmp(&editor,&saved,sizeof editor));
    assert(!line_editor_commit(&editor,NULL,100));
    line_editor_action(&editor,XLINE_CLEAR);
    memset(full,'a',sizeof full); full[sizeof full-1]=0;
    insert(full); saved=editor;
    assert(!line_editor_insert(&editor,"b",1));
    assert(!memcmp(&editor,&saved,sizeof editor));
    line_editor_action(&editor,XLINE_CLEAR);
    clusters[0]='e';
    for(i=0;i<15;++i) { clusters[1+2*i]=(char)0xcc; clusters[2+2*i]=(char)0x81; }
    clusters[31]=0; insert(clusters); saved=editor;
    assert(!line_editor_insert(&editor,"\xcc\x81",2));
    assert(!memcmp(&editor,&saved,sizeof editor));

    memset(&editor,0,sizeof editor);
    insert("one"); commit("one\n"); insert("two"); commit("two\n");
    insert("two"); commit("two\n"); assert(editor.count==2);
    commit("\n"); assert(editor.count==2);
    insert("draft"); line_editor_action(&editor,XLINE_LEFT);
    assert(line_editor_action(&editor,XLINE_PREVIOUS)); expect("two",3);
    assert(line_editor_action(&editor,XLINE_PREVIOUS)); expect("one",3);
    assert(!line_editor_action(&editor,XLINE_PREVIOUS));
    insert("!"); expect("one!",4);
    line_editor_action(&editor,XLINE_NEXT); expect("two",3);
    line_editor_action(&editor,XLINE_NEXT); expect("draft",4);
    assert(!line_editor_action(&editor,XLINE_NEXT));
    line_editor_action(&editor,XLINE_PREVIOUS);
    line_editor_action(&editor,XLINE_PREVIOUS); expect("one",3);
    line_editor_action(&editor,XLINE_CLEAR); expect("",0);
    assert(!line_editor_action(&editor,XLINE_NEXT));
    /* Ring eviction: retain the newest 32, not the first 32. */
    memset(&editor,0,sizeof editor);
    for(i=0;i<40;++i) {
        char command[16], expected[20];
        snprintf(command,sizeof command,"cmd%02lu",(unsigned long)i);
        snprintf(expected,sizeof expected,"%s\n",command);
        insert(command); commit(expected);
    }
    for(i=0;i<32;++i) assert(line_editor_action(&editor,XLINE_PREVIOUS));
    expect("cmd08",5); assert(!line_editor_action(&editor,XLINE_PREVIOUS));
    for(i=0;i<32;++i) assert(line_editor_action(&editor,XLINE_NEXT));
    expect("",0);
    insert("abc"); line_editor_action(&editor,XLINE_HOME);
    assert(line_editor_insert(&editor,editor.text,3)); expect("abcabc",3);
    line_editor_action(&editor,XLINE_CLEAR);
    insert("A👨‍👩‍👧‍👦中B");
    assert(line_editor_action(&editor,XLINE_SELECT_LEFT));
    assert(editor.selecting && editor.anchor==editor.length);
    assert(line_editor_action(&editor,XLINE_SELECT_LEFT));
    insert("X"); expect("A👨‍👩‍👧‍👦X",strlen("A👨‍👩‍👧‍👦X"));
    assert(!editor.selecting);
    line_editor_action(&editor,XLINE_SELECT_HOME);
    saved=editor;
    assert(!line_editor_insert(&editor,"\xff",1));
    assert(!memcmp(&saved,&editor,sizeof editor));
    line_editor_action(&editor,XLINE_RIGHT);
    expect("A👨‍👩‍👧‍👦X",strlen("A👨‍👩‍👧‍👦X"));
    assert(!editor.selecting);
    line_editor_action(&editor,XLINE_HOME);
    line_editor_action(&editor,XLINE_SELECT_END);
    assert(line_editor_action(&editor,XLINE_BACKSPACE)); expect("",0);
    insert("abc"); line_editor_action(&editor,XLINE_HOME);
    line_editor_action(&editor,XLINE_SELECT_RIGHT);
    assert(editor.anchor==0 && editor.cursor==1);
    line_editor_action(&editor,XLINE_DELETE); expect("bc",0);
    line_editor_action(&editor,XLINE_SELECT_RIGHT);
    line_editor_action(&editor,XLINE_PREVIOUS);
    line_editor_action(&editor,XLINE_NEXT); expect("bc",1);
    assert(editor.selecting && editor.anchor==0);
    puts("line editor PASS: insertion, grapheme editing, atomic rejection, history, draft, bounds");
    { /* A configured history length: the caller's ring, oldest dropped. */
      static char ring3[3][XLINE_CAPACITY], line[8];
      static XLineEditor e; char run[16]; unsigned k;
      line_editor_attach_history(&e,ring3,3);
      for(k=1;k<=5;++k) {
          snprintf(line,sizeof line,"c%u",k);
          assert(line_editor_insert(&e,line,strlen(line)));
          assert(line_editor_commit(&e,run,sizeof run)==3);
      }
      assert(e.count==3);
      assert(line_editor_action(&e,XLINE_PREVIOUS) && !strcmp(e.text,"c5"));
      assert(line_editor_action(&e,XLINE_PREVIOUS) && !strcmp(e.text,"c4"));
      assert(line_editor_action(&e,XLINE_PREVIOUS) && !strcmp(e.text,"c3"));
      assert(!line_editor_action(&e,XLINE_PREVIOUS) && !strcmp(e.text,"c3"));
      line_editor_action(&e,XLINE_NEXT); line_editor_action(&e,XLINE_NEXT); line_editor_action(&e,XLINE_NEXT);
      line_editor_search_start(&e);
      assert(line_editor_search_insert(&e,"c4",2) && !strcmp(e.text,"c4"));
      assert(!line_editor_search_start(&e));      /* c2 and c1 are gone */
      line_editor_search_cancel(&e);
      /* One entry. */
      line_editor_attach_history(&e,ring3,1);
      assert(!e.count);
      assert(line_editor_insert(&e,"x",1) && line_editor_commit(&e,run,sizeof run));
      assert(line_editor_insert(&e,"y",1) && line_editor_commit(&e,run,sizeof run));
      assert(e.count==1 && line_editor_action(&e,XLINE_PREVIOUS) && !strcmp(e.text,"y"));
      assert(!line_editor_action(&e,XLINE_PREVIOUS));
      puts("line editor PASS: attached history of 3 and of 1 entries"); }
    test_search();
    return 0;
}
