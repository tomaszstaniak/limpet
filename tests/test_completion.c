#include "../src/completion.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XLineEditor e;
static XCompletion c;
static void input(const char *s)
{ memset(&e,0,sizeof e); assert(line_editor_insert(&e,s,strlen(s))); }
int main(void)
{
    /* Wrong token/CWD split would complete the command instead of its path. */
    input("Type RAM:fo");
    assert(completion_prepare(&c,&e));
    assert(!strcmp(c.directory,"RAM:") && !strcmp(c.prefix,"fo") && !c.command);
    assert(completion_add(&c,"folder",1));
    assert(completion_add(&c,"food.txt",0));
    assert(!completion_add(&c,"other",0));
    assert(completion_apply(&c,&e,0));
    assert(!strcmp(e.text,"Type RAM:folder/"));
    assert(completion_current(&c,&e));
    assert(completion_apply(&c,&e,0));
    assert(!strcmp(e.text,"Type RAM:food.txt "));
    assert(completion_apply(&c,&e,1));
    assert(!strcmp(e.text,"Type RAM:folder/"));
    assert(line_editor_insert(&e,"x",1));
    assert(!completion_current(&c,&e));
    /* Quoting must protect spaces, quotes and Amiga '*' escapes. */
    input("Type \"RAM:My F");
    assert(completion_prepare(&c,&e));
    assert(!strcmp(c.directory,"RAM:") && !strcmp(c.prefix,"My F"));
    assert(completion_add(&c,"My File",0));
    assert(completion_apply(&c,&e,0));
    assert(!strcmp(e.text,"Type \"RAM:My File\" "));
    input("Type RAM:a"); assert(completion_prepare(&c,&e));
    assert(completion_add(&c,"a*\"b",0));
    assert(completion_apply(&c,&e,0));
    assert(!strcmp(e.text,"Type \"RAM:a***\"b\" "));
    input("Type f"); assert(completion_prepare(&c,&e));
    assert(completion_add(&c,"foo=bar",0)); assert(completion_apply(&c,&e,0));
    assert(!strcmp(e.text,"Type \"foo=bar\" "));
    /* Suffix arguments are preserved; mid-token and selection do not guess. */
    input("Type RAM:fo TO RAM:out"); e.cursor=11;
    assert(completion_prepare(&c,&e)); assert(completion_add(&c,"food",0));
    assert(completion_apply(&c,&e,0));
    assert(!strcmp(e.text,"Type RAM:food TO RAM:out"));
    input("Echo test"); e.cursor=7; assert(!completion_prepare(&c,&e));
    e.cursor=e.length; line_editor_action(&e,XLINE_SELECT_HOME);
    assert(!completion_prepare(&c,&e));
    /* Directory completion remains extendable inside quotes. */
    input("CD RAM:My"); assert(completion_prepare(&c,&e));
    assert(completion_add(&c,"My Dir",1)); assert(completion_apply(&c,&e,0));
    assert(!strcmp(e.text,"CD \"RAM:My Dir/\""));
    assert(e.cursor==e.length-1);
    assert(completion_prepare(&c,&e));
    assert(!strcmp(c.directory,"RAM:My Dir/") && !strcmp(c.prefix,""));
    /* Command detection, case-insensitive duplicates, and device suffix. */
    input("  ec"); assert(completion_prepare(&c,&e) && c.command);
    assert(completion_add(&c,"Echo",0)); assert(!completion_add(&c,"ECHO",0));
    assert(completion_apply(&c,&e,0)); assert(!strcmp(e.text,"  Echo "));
    input("CD ra"); assert(completion_prepare(&c,&e));
    assert(completion_add(&c,"RAM",2)); assert(completion_apply(&c,&e,0));
    assert(!strcmp(e.text,"CD RAM:"));
    /* No injection, malformed UTF-8, controls, or half-scalar insertion. */
    input("Type "); assert(completion_prepare(&c,&e));
    assert(!completion_add(&c,"evil\nEcho BAD",0));
    assert(!completion_add(&c,"$variable",0));
    assert(!completion_add(&c,"`command`",0));
    assert(!completion_add(&c,"\xff",0));
    assert(completion_add(&c,"日本😀",0)); assert(completion_apply(&c,&e,0));
    assert(!strcmp(e.text,"Type 日本😀 "));
    input("Type $SYS"); assert(!completion_prepare(&c,&e));
    input("Copy file TO=RAM:fo"); assert(completion_prepare(&c,&e));
    assert(!strcmp(c.directory,"RAM:") && !strcmp(c.prefix,"fo") && !c.command);
    assert(completion_add(&c,"folder",1)); assert(completion_apply(&c,&e,0));
    assert(!strcmp(e.text,"Copy file TO=RAM:folder/"));
    /* Full candidate set is explicit; overflow must not corrupt the line. */
    input("Type "); assert(completion_prepare(&c,&e));
    for(unsigned i=0;i<XCOMP_MAX;i++) { char s[32]; snprintf(s,sizeof s,"f%03u",i); assert(completion_add(&c,s,0)); }
    assert(!completion_add(&c,"overflow",0) && c.limited);
    /* A replacement that grows past the line limit leaves input/cursor intact. */
    { char full[XLINE_CAPACITY]; size_t cursor;
      memset(full,'x',sizeof full-1); full[sizeof full-1]=0;
      memcpy(full,"Type f ",7); input(full); e.cursor=6; cursor=e.cursor;
      assert(completion_prepare(&c,&e)); assert(completion_add(&c,"filename",0));
      assert(!completion_apply(&c,&e,0));
      assert(e.cursor==cursor && !strcmp(e.text,full) && !e.selecting); }
    puts("completion: PASS"); return 0;
}
