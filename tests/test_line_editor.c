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
    return 0;
}
