/* Exercise actual CLI parsing without opening the real system clipboard. */
#define main clip_probe_main
#include "../tools/clip_probe.c"
#undef main
#include <assert.h>
static unsigned io_calls;
int aros_clip_read(unsigned u,unsigned char *b,size_t c,size_t *n)
{ (void)u;(void)b;(void)c; ++io_calls; *n=0; return 0; }
int aros_clip_write(unsigned u,const unsigned char *b,size_t n)
{ (void)u;(void)b;(void)n; ++io_calls; return 0; }
int main(void)
{
    const char *bad[]={"4294967296","","-1","256","1garbage","9999999999999999999999999999"};
    unsigned i;
    for(i=0;i<sizeof bad/sizeof *bad;++i) {
        char *argv[]={"clip-probe","write",(char *)bad[i],"ascii"};
        io_calls=0;
        assert(clip_probe_main(4,argv)==20);
        assert(io_calls==0);
    }
    { char *argv[]={"clip-probe","inspect","237"};
      assert(clip_probe_main(3,argv)==0); assert(io_calls==1); }
    puts("clip-probe argument tests passed (invalid units never access clipboard)");
    return 0;
}
