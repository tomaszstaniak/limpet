#include "../src/aros_clipboard.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char previous[AROS_CLIP_LIMIT],after[AROS_CLIP_LIMIT];
int main(void)
{
    /* Isolated unit, never the user's system unit 0. Restore even on mismatch. */
    static const unsigned char clip[]={ 'F','O','R','M',0,0,0,16,'F','T','X','T',
                                       'C','H','R','S',0,0,0,3,'A','B','C',0 };
    size_t before=0,n=0; int result;
    assert(aros_clip_read(237,previous,sizeof previous,&before)==0);
    if(aros_clip_write(237,clip,sizeof clip)) { result=-1; goto restore; }
    result=aros_clip_read(237,after,sizeof after,&n);
    if(result || n!=sizeof clip || memcmp(after,clip,sizeof clip)) {
        result=-1; goto restore;
    }
    result=aros_clip_read(237,after,2,&n)==-2?0:-1;
restore:
    if(aros_clip_write(237,previous,before)) {
        puts("clipboard restore FAILED"); return 20;
    }
    if(result) { puts("clipboard roundtrip FAILED (original restored)"); return 20; }
    puts("native clipboard tests passed (unit 237, exact IFF bytes, bounded read, restore)");
    return 0;
}
