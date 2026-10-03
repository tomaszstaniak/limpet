#include "flood_protocol.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
void flood_input(unsigned char *p,unsigned k,uint64_t s)
{
    unsigned i;
    p[0]=0xf1;p[1]=0x0d;p[2]=(unsigned char)k;p[3]=0;
    for(i=0;i<8;++i) p[4+i]=(unsigned char)(s>>(8*i));
    for(i=0;i<4;++i) p[12+i]=(unsigned char)((0x12345678u>>(8*i))+i+(s>>(8*i)));
}
int flood_input_feed(FloodParser *p,const unsigned char *b,size_t n,FloodRecord f,void *c)
{
    size_t i; unsigned j,k; uint64_t seq; unsigned char expected[16];
    if(p->error) return 0;
    for(i=0;i<n;++i) {
        if(p->stopped) { p->error=1;return 0; }
        p->bytes[p->length++]=b[i];++p->bytes_seen;
        if(p->length!=16) continue;
        k=p->bytes[2];seq=0;
        for(j=0;j<8;++j) seq|=(uint64_t)p->bytes[4+j]<<(8*j);
        flood_input(expected,k,p->inputs+1);
        if(k<FLOOD_KEY || k>FLOOD_STOP || memcmp(p->bytes,expected,16) ||
           (f && !f(c,k,seq,0))) { p->error=1;return 0; }
        ++p->inputs;p->length=0;
        if(k==FLOOD_STOP) p->stopped=1;
    }
    return 1;
}
size_t flood_frame(unsigned char *b,size_t n,unsigned k,uint64_t s,uint64_t t)
{
    char body[FLOOD_FRAME_MAX]; int written; size_t used; unsigned i,bit;
    if(k!='F' && k!='A' && k!='T') return 0;
    written=snprintf(body,sizeof body,"\033]777;%c;%016llx;%016llx\007",k,
                     (unsigned long long)s,(unsigned long long)t);
    if(written!=FLOOD_HEADER_SIZE) return 0;
    used=(size_t)written;
    if(k=='F') {
        written=snprintf(body+used,sizeof body-used,
            "\033[%u;1H\033[3%umframe %016llx ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789 é 中\033[K",
            2+(unsigned)(s%23),(unsigned)(s%7)+1,(unsigned long long)s);
        if(written<0 || (size_t)written>=sizeof body-used) return 0;
        used+=(size_t)written;
    } else if(k=='A') {
        memcpy(body+used,"\033[?25l\033[1;1H",12);used+=12;
        /* Prefix 0xa5 and 64 big-endian bits. Explicit RGB avoids theme assumptions. */
        for(i=0;i<72;++i) {
            const char *colour;
            bit=i<8?(0xa5u>>(7-i))&1u:(unsigned)((s>>(71-i))&1u);
            colour=bit?"\033[48;2;255;255;255m ":"\033[48;2;0;0;0m ";
            written=snprintf(body+used,sizeof body-used,"%s",colour);
            if(written<0 || (size_t)written>=sizeof body-used) return 0;
            used+=(size_t)written;
        }
        memcpy(body+used,"\033[0m",4);used+=4;
    }
    if(used>n) return 0;
    memcpy(b,body,used);return used;
}
static int hex16(const unsigned char *b,uint64_t *v)
{
    unsigned i,d;*v=0;
    for(i=0;i<16;++i) {
        if(b[i]>='0' && b[i]<='9') d=b[i]-'0';
        else if(b[i]>='a' && b[i]<='f') d=b[i]-'a'+10;
        else return 0;
        *v=(*v<<4)|d;
    }
    return 1;
}
int flood_output_feed(FloodParser *p,const unsigned char *b,size_t n,FloodRecord f,void *c)
{
    size_t i; uint64_t seq,tick; unsigned k;
    if(p->error) return 0;
    for(i=0;i<n;++i) {
        if(p->stopped || p->length==sizeof p->bytes) goto bad;
        p->bytes[p->length++]=b[i];++p->bytes_seen;
        if(p->length<FLOOD_HEADER_SIZE) continue;
        if(p->length==FLOOD_HEADER_SIZE) {
            if(!hex16(p->bytes+8,&seq) || !hex16(p->bytes+25,&tick)) goto bad;
            p->need=flood_frame(p->expected,sizeof p->expected,p->bytes[6],seq,tick);
            if(!p->need || memcmp(p->bytes,p->expected,FLOOD_HEADER_SIZE)) goto bad;
        }
        if(p->length>p->need || p->bytes[p->length-1]!=p->expected[p->length-1]) goto bad;
        if(p->length!=p->need) continue;
        if(!hex16(p->bytes+8,&seq) || !hex16(p->bytes+25,&tick)) goto bad;
        k=p->bytes[6];
        if(k=='F') { if(seq!=p->frames+1 || tick) goto bad; ++p->frames; }
        if(k=='A') { if(seq!=p->acks+1 || !tick) goto bad; ++p->acks; }
        if(k=='T') { if(seq!=p->acks || tick) goto bad; p->stopped=1; }
        if(f && !f(c,k,seq,tick)) goto bad;
        p->length=0;
    }
    return 1;
bad:
    p->error=1;return 0;
}
int flood_finish(const FloodParser *p) { return !p->error && !p->length && p->stopped; }
int flood_complete(FloodSample *s,size_t n,uint64_t *d,uint64_t q,uint64_t u,uint64_t r,int ok)
{
    uint64_t i;
    if(!ok || n>FLOOD_SAMPLES || q>n || q<=*d || r<u) return 0;
    for(i=*d;i<q;++i) {
        FloodSample *a=&s[i];
        if(!a->offer || !a->parsed || a->due>a->offer || a->offer>a->dispatch ||
           a->dispatch>a->admit || a->admit>a->peer || a->peer>a->parsed ||
           a->parsed>u || a->uploaded || a->readback) return 0;
    }
    for(i=*d;i<q;++i) { s[i].uploaded=u;s[i].readback=r; }
    *d=q;return 1;
}
static int compare(const void *a,const void *b)
{ uint64_t x=*(const uint64_t *)a,y=*(const uint64_t *)b;return (x>y)-(x<y); }
uint64_t flood_quantile(uint64_t *v,size_t n,unsigned p)
{
    if(!n || n>FLOOD_SAMPLES || !p || p>100) return 0;
    qsort(v,n,sizeof *v,compare);return v[(n*p+99)/100-1];
}
