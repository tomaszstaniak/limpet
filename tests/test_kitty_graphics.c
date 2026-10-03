/* Host tests for the Kitty graphics subset (src/kitty_graphics.c) and its
   screen-model APC sink. */
#include "../src/kitty_graphics.h"
#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const XKittyLimits limits={64,64,64*64*4,3*64*64*4};
static const XKittyCursor cursor0={2,3,24,80,0,0,8,16};

static char *base64(const unsigned char *data,size_t n)
{
    static const char t[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char *out=malloc((n+2)/3*4+1),*o=out;size_t i;
    for(i=0;i<n;i+=3) {
        unsigned v=data[i]<<16|(i+1<n?data[i+1]<<8:0)|(i+2<n?data[i+2]:0);
        *o++=t[v>>18&63];*o++=t[v>>12&63];
        *o++=i+1<n?t[v>>6&63]:'=';*o++=i+2<n?t[v&63]:'=';
    }
    *o=0;return out;
}

/* One APC string (without ESC _ and ESC \), fed in pieces of `step` bytes. */
static int command(XKitty *k,const char *text,size_t step,XKittyResult *r)
{
    size_t n=strlen(text),i;
    xkitty_apc_begin(k);
    for(i=0;i<n;i+=step) xkitty_apc_data(k,(const unsigned char *)text+i,n-i<step?n-i:step);
    return xkitty_apc_end(k,&cursor0,r);
}
static int reply_is(const XKittyResult *r,const char *text)
{
    return r->reply_length==strlen(text) && !memcmp(r->reply,text,r->reply_length);
}

static unsigned char pixels[12]={1,2,3,4,5,6,7,8,9,10,11,12};

static void test_complete_and_chunked(void)
{
    XKitty k;XKittyResult r;char cmd[256];char *b=base64(pixels,12);size_t step;
    for(step=1;step<=strlen(b)+40;step+=7) {
        xkitty_init(&k,&limits);
        snprintf(cmd,sizeof cmd,"Ga=T,f=24,s=2,v=2,i=1;%s",b);
        assert(command(&k,cmd,step,&r)==1);
        assert(reply_is(&r,"\033_Gi=1;OK\033\\"));
        assert(k.images[0].id==1 && k.images[0].generation==1 && k.images[0].placed);
        assert(!memcmp(k.images[0].pixels,pixels,12));
        assert(r.changed && r.move_cursor && r.cursor_row==2 && r.cursor_column==4);
        assert(k.stats.memory_bytes==12 && k.stats.frames_received==1);
        xkitty_reset(&k);
        assert(k.stats.memory_bytes==0);
    }
    /* Three chunks; base64 split on 4-byte units as Kitty requires. */
    xkitty_init(&k,&limits);
    assert(command(&k,"Ga=T,f=24,s=2,v=2,i=7,m=1;AQID",3,&r)==1 && !r.reply_length && !r.changed);
    assert(k.transmitting && k.stats.frames_received==0);
    assert(command(&k,"Gm=1;BAUGBwgJ",2,&r)==1 && !r.reply_length);
    assert(command(&k,"Gm=0;CgsM",1,&r)==1 && reply_is(&r,"\033_Gi=7;OK\033\\"));
    assert(k.images[0].id==7 && !memcmp(k.images[0].pixels,pixels,12) && !k.transmitting);
    xkitty_reset(&k);free(b);
}

static void test_bad_lengths(void)
{
    XKitty k;XKittyResult r;
    xkitty_init(&k,&limits);
    /* 9 bytes for a 12-byte image. */
    assert(command(&k,"Ga=t,f=24,s=2,v=2,i=1;AQIDBAUGBwgJ",5,&r)==1);
    assert(reply_is(&r,"\033_Gi=1;EINVAL:data size does not match dimensions\033\\"));
    assert(!k.images[0].id && k.stats.memory_bytes==0 && k.stats.transmissions_dropped==1);
    /* 15 bytes for a 12-byte image. */
    assert(command(&k,"Ga=t,f=24,s=2,v=2,i=1;AQIDBAUGBwgJCgsMDQ4P",5,&r)==1);
    assert(reply_is(&r,"\033_Gi=1;EINVAL:malformed or oversized data\033\\"));
    /* Not base64. */
    assert(command(&k,"Ga=t,f=24,s=2,v=2,i=1;AQID!AUGBwgJCgsM",5,&r)==1);
    assert(reply_is(&r,"\033_Gi=1;EINVAL:malformed or oversized data\033\\"));
    assert(!k.images[0].id && k.stats.memory_bytes==0);
    xkitty_reset(&k);
}

static void test_interruptions(void)
{
    XKitty k;XKittyResult r;
    xkitty_init(&k,&limits);
    assert(command(&k,"Ga=T,f=24,s=2,v=2,i=1;AQIDBAUGBwgJCgsM",64,&r)==1);
    /* A second frame for id 1 is cut short: the first stays intact. */
    xkitty_apc_begin(&k);
    xkitty_apc_data(&k,(const unsigned char *)"Ga=T,f=24,s=2,v=2,i=1,m=1;AAAA",30);
    xkitty_apc_abort(&k);
    assert(!k.transmitting && k.images[0].generation==1 && k.stats.memory_bytes==12);
    /* An open chunked transmission interrupted by an unrelated command. */
    assert(command(&k,"Ga=t,f=24,s=2,v=2,i=2,m=1;AQID",64,&r)==1 && k.transmitting);
    assert(k.stats.memory_bytes==24);
    assert(command(&k,"Ga=d,d=a,i=1",64,&r)==1);
    assert(!k.transmitting && k.stats.memory_bytes==12 && k.stats.transmissions_dropped==2);
    /* The last chunk alone is not an image. */
    assert(command(&k,"Gm=0;CgsM",64,&r)==1 && !r.changed);
    xkitty_reset(&k);
}

static void test_unsupported_and_replies(void)
{
    XKitty k;XKittyResult r;
    xkitty_init(&k,&limits);
    assert(command(&k,"Ga=T,f=100,s=2,v=2,i=3;AAAA",64,&r) && reply_is(&r,"\033_Gi=3;ENOTSUPPORTED:format\033\\"));
    assert(command(&k,"Ga=T,t=f,f=24,s=2,v=2,i=3;AAAA",64,&r) && reply_is(&r,"\033_Gi=3;ENOTSUPPORTED:transmission medium\033\\"));
    assert(command(&k,"Ga=T,o=z,f=24,s=2,v=2,i=3;AAAA",64,&r) && reply_is(&r,"\033_Gi=3;ENOTSUPPORTED:key\033\\"));
    assert(command(&k,"Ga=f,i=3",64,&r) && reply_is(&r,"\033_Gi=3;ENOTSUPPORTED:action\033\\"));
    assert(command(&k,"Ga=T,x=1,f=24,s=2,v=2,i=3;AAAA",64,&r) && reply_is(&r,"\033_Gi=3;ENOTSUPPORTED:key\033\\"));
    assert(command(&k,"Ga=d,d=p,i=3",64,&r) && reply_is(&r,"\033_Gi=3;ENOTSUPPORTED:delete target\033\\"));
    assert(command(&k,"Ga=p,i=9",64,&r) && reply_is(&r,"\033_Gi=9;ENOENT:no such image\033\\"));
    /* The standard support query. */
    assert(command(&k,"Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA",64,&r) && reply_is(&r,"\033_Gi=31;OK\033\\"));
    /* Quiet modes and anonymous commands. */
    assert(command(&k,"Ga=q,i=31,s=1,v=1,f=24,q=1;AAAA",64,&r) && !r.reply_length);
    assert(command(&k,"Ga=T,f=100,s=2,v=2,i=3,q=1;AAAA",64,&r) && r.reply_length);
    assert(command(&k,"Ga=T,f=100,s=2,v=2,i=3,q=2;AAAA",64,&r) && !r.reply_length);
    assert(command(&k,"Ga=T,f=24,s=2,v=2;AQIDBAUGBwgJCgsM",64,&r) && !r.reply_length && !k.images[0].id);
    assert(command(&k,"Ga=T,f=24,,s=2;AAAA",64,&r) && !r.reply_length);
    /* Not Kitty at all. */
    assert(!command(&k,"Xsomething",64,&r) && !r.reply_length);
    /* An echoed reply (cooked tty) is ignored: no reply, no error, and an
       open transmission survives it. */
    {
        unsigned long errors=k.stats.errors,commands=k.stats.commands;
        assert(command(&k,"Gi=7;OK",64,&r) && !r.reply_length);
        assert(command(&k,"Gi=7;EINVAL:data size does not match dimensions",5,&r) && !r.reply_length);
        assert(command(&k,"Gi=7,p=2;OK",64,&r) && !r.reply_length);
        assert(k.stats.errors==errors && k.stats.commands==commands);
        assert(command(&k,"Ga=t,f=24,s=2,v=2,i=8,m=1;AQIDBAUG",64,&r) && !r.reply_length);
        assert(command(&k,"Gi=8;OK",64,&r) && !r.reply_length && k.transmitting);
        assert(command(&k,"Gm=0;BwgJCgsM",64,&r) && reply_is(&r,"\033_Gi=8;OK\033\\"));
    }
    assert(k.stats.memory_bytes==12);
    assert(command(&k,"Ga=d,d=I,i=8",64,&r));
    assert(k.stats.memory_bytes==0);
    xkitty_reset(&k);
}

static void test_limits(void)
{
    XKitty k;XKittyResult r;char cmd[128];unsigned i;
    xkitty_init(&k,&limits);
    assert(command(&k,"Ga=t,f=24,s=65,v=1,i=1;AAAA",64,&r) && reply_is(&r,"\033_Gi=1;EINVAL:dimensions exceed limit\033\\"));
    assert(command(&k,"Ga=t,f=24,i=1;AAAA",64,&r) && reply_is(&r,"\033_Gi=1;EINVAL:width and height required\033\\"));
    /* Total memory: three 64x64 RGBA images fit, the fourth does not. */
    for(i=1;i<=4;++i) {
        char *b;unsigned char *px=calloc(64*64*4,1);
        b=base64(px,64*64*4);
        snprintf(cmd,sizeof cmd,"Ga=t,f=32,s=64,v=64,i=%u,m=1;",i);
        assert(command(&k,cmd,64,&r)==1);
        if(i<4) {
            xkitty_apc_begin(&k);
            xkitty_apc_data(&k,(const unsigned char *)"Gm=0;",5);
            xkitty_apc_data(&k,(const unsigned char *)b,strlen(b));
            assert(xkitty_apc_end(&k,&cursor0,&r) && r.reply_length);
        } else assert(reply_is(&r,"\033_Gi=4;ENOSPC:image memory limit\033\\"));
        free(b);free(px);
    }
    assert(k.stats.memory_bytes==3*64*64*4 && k.stats.memory_peak<=limits.max_total_bytes);
    xkitty_reset(&k);
    assert(k.stats.memory_bytes==0);
}

static void test_replace_and_delete(void)
{
    XKitty k;XKittyResult r;XKittyCursor c=cursor0;long first;unsigned count;
    xkitty_init(&k,&limits);
    assert(command(&k,"Ga=T,f=24,s=2,v=2,i=1;AQIDBAUGBwgJCgsM",64,&r) && r.changed);
    assert(command(&k,"Ga=t,f=24,s=2,v=2,i=1;DAsKCQgHBgUEAwIB",64,&r) && r.changed);
    assert(k.images[0].generation==2 && k.images[0].placed && k.images[0].pixels[0]==12);
    assert(k.stats.memory_bytes==12);
    assert(xkitty_placement_rows(&k.images[0],&c,&first,&count) && first==2 && count==1);
    c.scroll_count=5;
    assert(xkitty_placement_rows(&k.images[0],&c,&first,&count) && first==-3);
    c.alternate=1;assert(!xkitty_placement_rows(&k.images[0],&c,&first,&count));
    assert(command(&k,"Ga=d,d=a",64,&r) && r.changed && !k.images[0].placed && k.images[0].id==1);
    assert(command(&k,"Ga=p,i=1,C=1",64,&r) && r.changed && !r.move_cursor && k.images[0].placed);
    assert(command(&k,"Ga=d,d=I,i=1",64,&r) && !k.images[0].id && k.stats.memory_bytes==0);
    xkitty_reset(&k);
}

/* Ids are 1..2^32-1. A larger number must not wrap onto an existing image,
   placement ids are refused rather than ignored, and serials stay unique
   when a slot is freed and reused (generation restarts there). */
static void test_ids_and_serials(void)
{
    XKitty k;XKittyResult r;uint64_t serial;
    xkitty_init(&k,&limits);
    assert(command(&k,"Ga=T,f=24,s=2,v=2,i=1;AQIDBAUGBwgJCgsM",64,&r));
    assert(command(&k,"Ga=d,d=I,i=4294967297",64,&r) && !r.reply_length);
    assert(command(&k,"Ga=d,d=I,i=99999999999999999999999",3,&r) && !r.reply_length);
    assert(k.images[0].id==1 && k.images[0].placed && k.stats.memory_bytes==12);
    assert(command(&k,"Ga=p,i=1,p=2",64,&r) && reply_is(&r,"\033_Gi=1;ENOTSUPPORTED:key\033\\"));
    assert(command(&k,"Ga=p,i=1,p=0",64,&r) && reply_is(&r,"\033_Gi=1;OK\033\\"));
    assert(command(&k,"Ga=t,f=24,s=2,v=2,i=4294967295,q=1;AQIDBAUGBwgJCgsM",64,&r) && !r.reply_length);
    assert(k.images[1].id==4294967295u && k.images[1].serial==2);
    assert(command(&k,"Ga=p,i=4294967296",64,&r) && !r.reply_length);
    serial=k.images[0].serial;assert(serial==1 && k.images[0].generation==1);
    assert(command(&k,"Ga=d,d=I,i=1",64,&r) && !k.images[0].id);
    assert(command(&k,"Ga=T,f=24,s=2,v=2,i=5;AQIDBAUGBwgJCgsM",64,&r) && k.images[0].id==5);
    assert(k.images[0].generation==1 && k.images[0].serial==3 && k.images[0].serial!=serial);
    xkitty_reset(&k);
}

/* The model hands APC to the sink and never to the grid. */
static XKitty sink_kitty;static unsigned sink_ends,sink_aborts;
static XKittyResult sink_result;
static void sink(void *context,int event,const unsigned char *bytes,size_t n)
{
    (void)context;
    if(event==XSCREEN_APC_BEGIN) xkitty_apc_begin(&sink_kitty);
    else if(event==XSCREEN_APC_DATA) xkitty_apc_data(&sink_kitty,bytes,n);
    else if(event==XSCREEN_APC_END) { ++sink_ends;xkitty_apc_end(&sink_kitty,&cursor0,&sink_result); }
    else { ++sink_aborts;xkitty_apc_abort(&sink_kitty); }
}
static void test_model(void)
{
    static XTermCell cells[24*80];
    XTermScreen s;size_t i,step;
    const char *stream="ab\033_Ga=T,f=24,s=2,v=2,i=1;AQIDBAUGBwgJCgsM\033\\cd\033_Xignored\033\\e"
                       "\033_Ga=T,f=24,s=2,v=2,i=2;AQID\033Xjunk\033\\f\033_Ga=T,i=3,f=24,s=2,v=2;AQ\030g";
    for(step=1;step<=strlen(stream);step+=5) {
        screen_init(&s,cells,24*80,24,80);
        xkitty_init(&sink_kitty,&limits);sink_ends=sink_aborts=0;
        s.apc=sink;s.apc_context=NULL;
        for(i=0;i<strlen(stream);i+=step)
            screen_write(&s,(const unsigned char *)stream+i,strlen(stream)-i<step?strlen(stream)-i:step);
        assert(screen_cell(&s,0,0)=='a' && screen_cell(&s,0,1)=='b' && screen_cell(&s,0,2)=='c' &&
               screen_cell(&s,0,3)=='d' && screen_cell(&s,0,4)=='e' && screen_cell(&s,0,5)=='f' &&
               screen_cell(&s,0,6)=='g' && screen_cell(&s,0,7)==' ');
        assert(sink_ends==2 && sink_aborts==2);
        assert(sink_kitty.images[0].id==1 && sink_kitty.stats.frames_received==1 && !sink_kitty.transmitting);
        assert(sink_kitty.stats.memory_bytes==12);
        xkitty_reset(&sink_kitty);
    }
    /* Without a sink the string is still discarded, as before. */
    screen_init(&s,cells,24*80,24,80);
    screen_write(&s,(const unsigned char *)"x\033_Ga=T;AAAA\033\\y",16);
    assert(screen_cell(&s,0,0)=='x' && screen_cell(&s,0,1)=='y');
    /* Full-screen scrolls are counted for image anchoring. */
    screen_init(&s,cells,24*80,24,80);
    for(i=0;i<30;++i) screen_write(&s,(const unsigned char *)"\n",1);
    assert(s.scroll_count==7);
    screen_write(&s,(const unsigned char *)"\033[2S",4);
    assert(s.scroll_count==9);
    screen_write(&s,(const unsigned char *)"\033[5;10r\033[3S",11);
    assert(s.scroll_count==9);
}

/* A stream of Kitty strings leaves visible_bytes alone; anything that can
   change the display moves it, including an ESC sequence and discarded
   strings' neighbours. */
static void test_visible_bytes(void)
{
    static XTermCell cells[24*80];XTermScreen s;unsigned long v;size_t i;
    const char *images="\033_Ga=T,f=24,s=2,v=2,i=1,m=1;AQIDBAUG\033\\\033_Gm=0;BwgJCgsM\033\\";
    screen_init(&s,cells,24*80,24,80);
    xkitty_init(&sink_kitty,&limits);s.apc=sink;s.apc_context=NULL;
    v=s.visible_bytes;
    for(i=0;i<strlen(images);++i) screen_write(&s,(const unsigned char *)images+i,1);
    assert(s.visible_bytes==v && sink_kitty.stats.frames_received==1);
    screen_write(&s,(const unsigned char *)"\033_Xjunk\033\\",9);
    assert(s.visible_bytes==v);                       /* non-Kitty APC, also discarded */
    screen_write(&s,(const unsigned char *)"\033[H",3);assert(s.visible_bytes>v);
    v=s.visible_bytes;screen_write(&s,(const unsigned char *)"a",1);assert(s.visible_bytes==v+1);
    v=s.visible_bytes;screen_write(&s,(const unsigned char *)"\303\251",2);assert(s.visible_bytes==v+2);
    v=s.visible_bytes;screen_write(&s,(const unsigned char *)"\033Pdcs\033\\",7);assert(s.visible_bytes==v+1);
    /* Shrinking the height drops rows from the top to keep the cursor row;
       an image placed there must move up with the text. */
    {
        XKittyCursor c;long first;unsigned count;
        screen_init(&s,cells,24*80,24,80);s.apc=sink;
        screen_write(&s,(const unsigned char *)"\033[15;1H",7);
        c.row=s.cursor_row;c.column=0;c.rows=24;c.columns=80;c.alternate=0;
        c.scroll_count=s.scroll_count;c.cell_width=8;c.cell_height=16;
        {
            const char *t="Ga=T,f=24,s=2,v=2,i=4;AQIDBAUGBwgJCgsM";
            xkitty_apc_begin(&sink_kitty);
            xkitty_apc_data(&sink_kitty,(const unsigned char *)t,strlen(t));
            assert(xkitty_apc_end(&sink_kitty,&c,&sink_result));
        }
        screen_write(&s,(const unsigned char *)"\033[21;1H",7);
        v=s.scroll_count;
        assert(!screen_resize(&s,10,80) && s.scroll_count==v+11);
        c.rows=10;c.scroll_count=s.scroll_count;
        {
            unsigned i;
            for(i=0;i<XKITTY_MAX_IMAGES && sink_kitty.images[i].id!=4;++i) {}
            assert(i<XKITTY_MAX_IMAGES);
            assert(xkitty_placement_rows(&sink_kitty.images[i],&c,&first,&count) && first==14-11);
        }
    }
    xkitty_reset(&sink_kitty);
}

int main(void)
{
    test_complete_and_chunked();
    test_bad_lengths();
    test_interruptions();
    test_unsupported_and_replies();
    test_limits();
    test_replace_and_delete();
    test_ids_and_serials();
    test_model();
    test_visible_bytes();
    puts("KITTY GRAPHICS HOST PASS: chunks, lengths, interruptions, replies, limits, replace/delete, ids/serials, model sink, visible bytes");
    return 0;
}
