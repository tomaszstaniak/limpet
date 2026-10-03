#include "session_test_options.h"
#include <assert.h>
#include <stdio.h>
static void check(int argc,const char **args,int good,unsigned count,int strict)
{
    SessionTestOptions options={77,77};
    int result=session_test_options(argc,args,&options);
    assert(result==good);
    if(good) { assert(options.repeat==count); assert(options.require_break==strict); }
    else { assert(options.repeat==77); assert(options.require_break==77); }
}
#define CHECK(good,count,strict,...) do { const char *args[]={__VA_ARGS__}; \
    check((int)(sizeof args/sizeof args[0]),args,good,count,strict); } while(0)
int main(void)
{
    CHECK(1,2,1,"suite","--repeat","2","--require-break");
    CHECK(1,1,0,"suite");
    CHECK(1,1,1,"suite","--require-break");
    CHECK(1,1000,1,"suite","--require-break","--repeat","1000");
    CHECK(1,1,0,"suite","--repeat","1");
    CHECK(1,9,0,"suite","--repeat","009");
    CHECK(0,0,0,"suite","--repeat");
    CHECK(0,0,0,"suite","--repeat","");
    CHECK(0,0,0,"suite","--repeat","0");
    CHECK(0,0,0,"suite","--repeat","1001");
    CHECK(0,0,0,"suite","--repeat","4294967297");
    CHECK(0,0,0,"suite","--repeat","99999999999999999999999999999999999");
    CHECK(0,0,0,"suite","--repeat","-1");
    CHECK(0,0,0,"suite","--repeat","+1");
    CHECK(0,0,0,"suite","--repeat"," 1");
    CHECK(0,0,0,"suite","--repeat","1 ");
    CHECK(0,0,0,"suite","--repeat","1x");
    CHECK(0,0,0,"suite","--repeat","1.0");
    CHECK(0,0,0,"suite","--repeat","1","--repeat","2");
    CHECK(0,0,0,"suite","--require-break","--require-break");
    CHECK(0,0,0,"suite","--repeat","--require-break");
    CHECK(0,0,0,"suite","--require-break","junk");
    CHECK(0,0,0,"suite","--repeat=2");
    CHECK(0,0,0,"suite","2");
    CHECK(0,0,0,"suite","--unknown");
    CHECK(0,0,0,"suite","--repeat","2","--require-break","trailing");
    puts("session test options: PASS"); return 0;
}
