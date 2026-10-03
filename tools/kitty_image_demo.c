/* Kitty graphics demo 1: one static image beside ordinary text.

   Talks to the terminal only through Input()/Output(): it asks whether
   Kitty graphics are supported (a=q), clears the screen, draws a generated
   192x128 RGB picture at row 4 with a=T and C=1 (cursor stays), writes
   text to its right with absolute positioning, and leaves the cursor below
   the picture. The image stays until the session ends or is deleted. */
#include <proto/dos.h>
#include <dos/dos.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "term_write.h"

#define W 192
#define H 128

static unsigned char rgb[W*H*3];
static char encoded[(W*H*3+2)/3*4+1];

static void put(const char *text) { write_text(text); }

static size_t base64(const unsigned char *data,size_t n,char *out)
{
    static const char t[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i;char *o=out;
    for(i=0;i<n;i+=3) {
        unsigned v=(unsigned)data[i]<<16|(i+1<n?(unsigned)data[i+1]<<8:0)|(i+2<n?data[i+2]:0);
        *o++=t[v>>18&63];*o++=t[v>>12&63];
        *o++=i+1<n?t[v>>6&63]:'=';*o++=i+2<n?t[v&63]:'=';
    }
    *o=0;return (size_t)(o-out);
}

/* Send one image in 4096-byte base64 chunks (m=1 ... m=0), one whole APC
   sequence per write. */
static void send_image(const char *first_keys,const char *data,size_t length)
{
    static char sequence[4096+256];char keys[160];size_t at=0;
    do {
        size_t n=length-at>4096?4096:length-at;
        snprintf(keys,sizeof keys,"%s%sm=%d",at?"":first_keys,at?"":",",at+n<length);
        write_all(sequence,kitty_sequence(sequence,keys,data+at,n));
        at+=n;
    } while(at<length);
}

/* Read one reply (ESC _ G ... ESC \\), for at most one second. Raw mode:
   in cooked mode it would wait in the Shell's line buffer. */
static int read_reply(char *answer,size_t size)
{
    size_t n=0;char c;
    while(n+1<size && WaitForChar(Input(),1000000) && Read(Input(),&c,1)==1) {
        answer[n++]=c;
        if(n>=2 && answer[n-2]==27 && c=='\\') break;
    }
    answer[n]=0;
    return strstr(answer,";OK")!=NULL;
}
static void printable(const char *in,char *out,size_t size)
{
    size_t i;
    for(i=0;in[i] && i+1<size;++i) out[i]=in[i]==27?'^':in[i];
    out[i]=0;
}

int main(void)
{
    char answer[80],shown[80],image_answer[80],image_shown[80];unsigned x,y;int ok;
    for(y=0;y<H;++y) for(x=0;x<W;++x) {
        unsigned char *p=rgb+(y*W+x)*3;
        double dx=x-W/2.0,dy=y-H/2.0,d=sqrt(dx*dx+dy*dy);
        p[0]=(unsigned char)(x*255/(W-1));
        p[1]=(unsigned char)(y*255/(H-1));
        p[2]=(unsigned char)(128+127*sin(d/6.0));
        if(((x/16)+(y/16))%2==0 && d>52) { p[0]/=2;p[1]/=2;p[2]/=2; }
        if(d<50 && d>44) { p[0]=p[1]=p[2]=255; }
    }
    SetMode(Input(),1);
    put("\033_Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA\033\\");
    ok=read_reply(answer,sizeof answer);
    printable(answer,shown,sizeof shown);
    /* A known layout: clear, then absolute rows (the terminal has no DECSC). */
    put("\033[2J\033[H\033[1mLimpet: Kitty graphics, static image\033[0m\r\n");
    put("Support query: ");put(ok?"supported":"no answer / not supported");
    put(" (reply ");put(shown);put(")\r\n");
    if(!ok) { SetMode(Input(),0);return RETURN_WARN; }
    put("\033[4;3H");
    send_image("a=T,f=24,s=192,v=128,i=1,C=1",encoded,base64(rgb,sizeof rgb,encoded));
    read_reply(image_answer,sizeof image_answer);
    SetMode(Input(),0);
    printable(image_answer,image_shown,sizeof image_shown);
    put("\033[4;30H\033[1mKitty image beside text\033[0m");
    put("\033[5;30H192 x 128 RGB, 24 bit");
    put("\033[6;30Hsent as base64 in 4 KiB chunks");
    put("\033[7;30Hdrawn by Limpet");
    put("\033[9;30H\033[36mcyan\033[0m \033[33myellow\033[0m \033[35mmagenta\033[0m text");
    put("\033[11;30Htransmit reply: ");put(image_shown[0]?image_shown:"none");
    put("\033[13;1H");
    return term_failed?RETURN_FAIL:RETURN_OK;
}
