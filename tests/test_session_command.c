#include "../src/session_command.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    char out[128];
    assert(!session_quote_path("RAM:a b/session-peer",out,sizeof out));
    assert(!strcmp(out,"\"RAM:a b/session-peer\""));
    assert(!session_quote_path("RAM:a*\"b/session-peer",out,sizeof out));
    assert(!strcmp(out,"\"RAM:a***\"b/session-peer\""));
    assert(session_quote_path("RAM:$name/session-peer",out,sizeof out)==-1);
    assert(session_quote_path("RAM:`Echo injected`/session-peer",out,sizeof out)==-1);
    assert(session_quote_path("RAM:<$$>/session-peer",out,sizeof out)==-1);
    assert(session_quote_path("RAM:a\nEcho injected",out,sizeof out)==-1);
    assert(session_quote_path("RAM:peer",out,4)==-1);
    assert(session_quote_path("",out,sizeof out)==-1);
    puts("session command tests passed (literal paths, bounds, substitution rejection)");
    return 0;
}
