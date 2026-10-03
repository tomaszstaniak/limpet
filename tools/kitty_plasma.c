/* Kitty graphics demo 2: a 320x200 plasma animated in place.

   Usage: kitty_plasma [SECONDS [RESULTFILE [sequence|frame]]]
   (default 60 seconds, one Write() per APC sequence)

   Talks to the terminal only through Input()/Output(). Every frame is a
   complete 320x200 RGB image sent as base64 in 4 KiB chunks, the Kitty
   limit per chunk. Each chunk goes out as one whole APC sequence in one
   Write() ("sequence"), or the frame's 63 sequences in one Write()
   ("frame"); the byte stream is the same either way. The first
   frame is a=T (transmit and display), later ones a=t under the same id,
   which replaces the pixels while the placement stays. q=2 suppresses
   replies so nothing comes back on the input. Writes block when the
   terminal is behind, so the producer runs at the terminal's pace.

   q or Ctrl-C ends it early. On exit the image is deleted (d=I), the cursor
   shown again and a summary printed; it counts frames *sent*, which is not
   the number the terminal drew (the terminal keeps its own counters). */
#include <proto/dos.h>
#include <dos/dos.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "term_write.h"

#define W 320
#define H 200
#define CHUNK 4096

static unsigned char frame[W*H*3];
static char encoded[(W*H*3+2)/3*4];
static char stream[(W*H*3+2)/3*4+((W*H*3+2)/3*4/CHUNK+1)*96];
static unsigned char wave_x[1024],wave_y[1024],wave_d[2048];
static unsigned char palette[256][3];
static unsigned short dist[H][W];
static int whole_frame;

static size_t base64(const unsigned char *data,size_t n,char *out)
{
    static const char t[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i;char *o=out;
    for(i=0;i+2<n;i+=3) {
        unsigned v=(unsigned)data[i]<<16|(unsigned)data[i+1]<<8|data[i+2];
        *o++=t[v>>18&63];*o++=t[v>>12&63];*o++=t[v>>6&63];*o++=t[v&63];
    }
    return (size_t)(o-out); /* W*H*3 is a multiple of 3: no padding */
}

static void render(unsigned t)
{
    unsigned x,y;unsigned char *p=frame;
    for(y=0;y<H;++y) {
        unsigned wy=wave_y[(y*2+t*3)&1023];
        for(x=0;x<W;++x) {
            unsigned v=wave_x[(x+t*2)&1023]+wy+wave_d[(dist[y][x]+t*4)&2047]+wave_x[(x+y+t)&1023];
            const unsigned char *c=palette[(v+t)&255];
            *p++=c[0];*p++=c[1];*p++=c[2];
        }
    }
}

static void send_frame(unsigned index)
{
    char keys[80];size_t length=base64(frame,sizeof frame,encoded),at=0,used=0;
    do {
        size_t n=length-at>CHUNK?CHUNK:length-at;
        if(!at)
            snprintf(keys,sizeof keys,"a=%c,f=24,s=%d,v=%d,i=1,C=1,q=2,m=%d",
                     index?'t':'T',W,H,at+n<length);
        else snprintf(keys,sizeof keys,"m=%d",at+n<length);
        used+=kitty_sequence(stream+used,keys,encoded+at,n);
        if(!whole_frame) { write_all(stream,used);used=0; }
        at+=n;
    } while(at<length);
    if(used) write_all(stream,used);
}

static long elapsed_ms(const struct DateStamp *a,const struct DateStamp *b)
{
    return (b->ds_Days-a->ds_Days)*86400000L+(b->ds_Minute-a->ds_Minute)*60000L+
           (b->ds_Tick-a->ds_Tick)*1000L/TICKS_PER_SECOND;
}

int main(int argc,char **argv)
{
    long seconds=argc>1?atol(argv[1]):60,ms=0;
    unsigned frames=0,i,x,y;int stop=0;
    struct DateStamp start,now;char line[200];
    if(seconds<=0) seconds=60;
    whole_frame=argc>3 && !strcmp(argv[3],"frame");
    for(i=0;i<1024;++i) {
        wave_x[i]=(unsigned char)(32+31*sin(i*6.2831853/256.0));
        wave_y[i]=(unsigned char)(32+31*sin(i*6.2831853/341.0));
    }
    for(i=0;i<2048;++i) wave_d[i]=(unsigned char)(32+31*sin(i*6.2831853/128.0));
    for(y=0;y<H;++y) for(x=0;x<W;++x) {
        double dx=x-W/2.0,dy=y-H/2.0;
        dist[y][x]=(unsigned short)(sqrt(dx*dx+dy*dy)*2.0);
    }
    for(i=0;i<256;++i) {
        palette[i][0]=(unsigned char)(128+127*sin(i*6.2831853/256.0));
        palette[i][1]=(unsigned char)(128+127*sin(i*6.2831853/256.0+2.094));
        palette[i][2]=(unsigned char)(128+127*sin(i*6.2831853/256.0+4.189));
    }
    SetMode(Input(),1);
    write_text("\033[2J\033[H\033[?25l\033[1mLimpet: Kitty plasma 320x200\033[0m  q or Ctrl-C ends");
    DateStamp(&start);
    while(!stop && !term_failed) {
        char c;
        write_text("\033[3;3H");
        render(frames);send_frame(frames);++frames;
        if(frames%10==0) {
            DateStamp(&now);ms=elapsed_ms(&start,&now);
            snprintf(line,sizeof line,"\033[2;1Hsent %u frames in %ld.%01ld s",frames,ms/1000,(ms%1000)/100);
            write_text(line);
        }
        while(WaitForChar(Input(),0) && Read(Input(),&c,1)==1) if(c=='q' || c=='Q' || c==3) stop=1;
        if(CheckSignal(SIGBREAKF_CTRL_C)) stop=1;
        DateStamp(&now);ms=elapsed_ms(&start,&now);
        if(ms>=seconds*1000L) stop=1;
    }
    write_text("\033_Ga=d,d=I,i=1,q=2\033\\");
    write_text("\033[?25h\033[16;1H");
    SetMode(Input(),0);
    snprintf(line,sizeof line,"plasma sent_frames=%u seconds=%ld.%03ld sent_fps=%ld.%02ld bytes=%llu writes=%lu mode=%s%s\n",
             frames,ms/1000,ms%1000,ms?frames*1000L/ms:0,ms?(frames*100000L/ms)%100:0,term_bytes,term_writes,
             whole_frame?"frame":"sequence",term_failed?" output_failed":"");
    write_text("\r\n");write_text(line);write_text("\r");
    if(argc>2) {
        BPTR file=Open((CONST_STRPTR)argv[2],MODE_READWRITE);
        if(file) { Seek(file,0,OFFSET_END);Write(file,line,(LONG)strlen(line));Close(file); }
    }
    return RETURN_OK;
}
