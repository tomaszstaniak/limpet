/* Detects stream corruption/reordering and false visibility completion. */
#include "../tools/flood_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned calls;
static int record(void *ctx,unsigned kind,uint64_t seq,uint64_t tick)
{ (void)ctx; (void)kind; (void)tick; assert(seq); ++calls; return 1; }
static void input_tests(void)
{
    unsigned char b[32]; unsigned split;
    static const unsigned char known[16]={0xf1,0x0d,1,0,1,0,0,0,0,0,0,0,0x79,0x57,0x36,0x15};
    flood_input(b,FLOOD_KEY,1); assert(!memcmp(b,known,16));
    flood_input(b+16,FLOOD_STOP,2);
    for(split=0;split<=32;++split) {
        FloodParser p={0}; calls=0;
        assert(flood_input_feed(&p,b,split,record,NULL));
        assert(flood_input_feed(&p,b+split,32-split,record,NULL));
        assert(calls==2 && p.inputs==2 && p.stopped && flood_finish(&p));
    }
    { FloodParser p={0}; assert(flood_input_feed(&p,b,15,record,NULL)); assert(!flood_finish(&p)); }
    { FloodParser p={0}; b[3]=1; assert(!flood_input_feed(&p,b,16,record,NULL)); b[3]=0; }
    { FloodParser p={0}; b[12]^=1; assert(!flood_input_feed(&p,b,16,record,NULL)); b[12]^=1; }
    { FloodParser p={0}; assert(!flood_input_feed(&p,b+16,16,record,NULL)); }
    { FloodParser p={0}; assert(flood_input_feed(&p,b,16,record,NULL)); assert(!flood_input_feed(&p,b,16,record,NULL)); }
}
static void output_tests(void)
{
    unsigned char b[8192]; size_t n=0,split;
    n+=flood_frame(b+n,sizeof b-n,'F',1,0);
    n+=flood_frame(b+n,sizeof b-n,'A',1,123);
    n+=flood_frame(b+n,sizeof b-n,'T',1,0);
    assert(n && !memcmp(b,"\033]777;F;0000000000000001;0000000000000000\007",42));
    for(split=0;split<=n;++split) {
        FloodParser p={0}; calls=0;
        assert(flood_output_feed(&p,b,split,record,NULL));
        assert(flood_output_feed(&p,b+split,n-split,record,NULL));
        assert(calls==3 && p.frames==1 && p.acks==1 && p.stopped && flood_finish(&p));
    }
    { FloodParser p={0}; b[45]^=1; assert(!flood_output_feed(&p,b,n,record,NULL)); b[45]^=1; }
    { FloodParser p={0}; assert(flood_output_feed(&p,b,n-1,record,NULL)); assert(!flood_finish(&p)); }
    { FloodParser p={0}; size_t m=flood_frame(b,sizeof b,'A',2,123); assert(!flood_output_feed(&p,b,m,record,NULL)); }
}
static void visibility_tests(void)
{
    FloodSample samples[3]; uint64_t done=0; unsigned i;
    uint64_t values[]={40,10,20,20};
    memset(samples,0,sizeof samples);
    for(i=0;i<3;++i) {
        samples[i].due=1; samples[i].offer=2; samples[i].dispatch=3;
        samples[i].admit=4; samples[i].peer=5; samples[i].parsed=6;
    }
    assert(!flood_complete(samples,3,&done,3,7,8,0) && done==0);
    assert(!flood_complete(samples,3,&done,4,7,8,1) && done==0);
    assert(!flood_complete(samples,3,&done,3,5,8,1) && done==0);
    assert(flood_complete(samples,3,&done,3,7,8,1) && done==3);
    for(i=0;i<3;++i) assert(samples[i].readback==8 && samples[i].uploaded==7);
    assert(!flood_complete(samples,3,&done,3,9,10,1));
    assert(!flood_complete(samples,3,&done,2,9,10,1));
    assert(flood_quantile(values,4,50)==20);
    assert(flood_quantile(values,4,95)==40);
    assert(flood_quantile(values,4,99)==40);
    assert(flood_quantile(values,0,50)==0);
    assert(flood_quantile(values,1,50)==10);
}
int main(void)
{ input_tests(); output_tests(); visibility_tests(); puts("flood protocol PASS"); return 0; }
