#include "../src/console_geometry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    unsigned char storage[sizeof(struct ConsoleGeometry)+8], before[sizeof storage];
    struct ConsoleGeometry got;
    memset(storage,0xa5,sizeof storage);
    assert(console_geometry_reply(storage,sizeof storage,1,80,24,640,384));
    memcpy(&got,storage,sizeof got);
    assert(got.cg_Size==24 && got.cg_Version==1);
    assert(got.cg_Columns==80 && got.cg_Rows==24);
    assert(got.cg_Width==640 && got.cg_Height==384);
    assert(storage[24]==0xa5 && storage[31]==0xa5);
    /* A fresh snapshot replaces the old dimensions after a resize. */
    assert(console_geometry_reply(storage,24,1,101,37,909,703));
    memcpy(&got,storage,sizeof got);
    assert(got.cg_Columns==101 && got.cg_Rows==37);
    assert(got.cg_Width==909 && got.cg_Height==703);
    memcpy(before,storage,sizeof storage);
    assert(!console_geometry_reply(storage,23,1,1,1,1,1));
    assert(!console_geometry_reply(storage,-1,1,1,1,1,1));
    assert(!console_geometry_reply(storage,24,0,1,1,1,1));
    assert(!console_geometry_reply(storage,24,2,1,1,1,1));
    assert(!console_geometry_reply(NULL,24,1,1,1,1,1));
    assert(!memcmp(storage,before,sizeof storage));
    /* Callers need not align the output block or pre-initialize its fields. */
    assert(console_geometry_reply(storage+1,24,1,7,3,56,48));
    memcpy(&got,storage+1,sizeof got);
    assert(got.cg_Size==24 && got.cg_Columns==7 && got.cg_Width==56);
    puts("console geometry tests passed");
    return 0;
}
