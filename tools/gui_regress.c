/* GUI regression producer for the "no redraw while an image arrives" path
   (docs/release-checklist.md, R1). Writes a fixed stream to the terminal:
   text, cursor moves, an image transmission cut off by CAN, one cut off by a
   stray ESC, text after each, then a complete image beside text, and leaves
   the cursor at a known place. The same stream is shown with
   XTERM_APC_NODRAW=1 and =0; the screens must be identical, before and after
   a resize and an exposure. Terminal I/O only. */
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

static void put(const char *s) { Write(Output(),(APTR)s,(LONG)strlen(s)); }

/* A 64x32 RGB image as base64: a gradient with a white frame. */
static char image[64*32*4+8];
static size_t make_image(void)
{
    static const char t[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char rgb[64*32*3];unsigned x,y;size_t i,n=0;
    for(y=0;y<32;++y) for(x=0;x<64;++x) {
        unsigned char *p=rgb+(y*64+x)*3;int edge=x==0||y==0||x==63||y==31;
        p[0]=edge?255:(unsigned char)(x*4);p[1]=edge?255:(unsigned char)(y*8);p[2]=edge?255:160;
    }
    for(i=0;i<sizeof rgb;i+=3) {
        unsigned v=(unsigned)rgb[i]<<16|(unsigned)rgb[i+1]<<8|rgb[i+2];
        image[n++]=t[v>>18&63];image[n++]=t[v>>12&63];image[n++]=t[v>>6&63];image[n++]=t[v&63];
    }
    image[n]=0;return n;
}

int main(void)
{
    char line[96];unsigned i;size_t n=make_image();
    put("\033[2J\033[H");
    for(i=1;i<=8;++i) {
        snprintf(line,sizeof line,"line %02u  plain text, then colour: \033[3%um%s\033[0m\r\n",i,i%8,"colour");
        put(line);
    }
    /* Cursor moves and overwrites. */
    put("\033[3;20HOVERWRITE\033[10;1Hrow ten\033[1A\033[5Cup-and-right");
    /* An image cut off by CAN: nothing may appear, text after it must. */
    put("\033[12;1Hbefore CAN \033_Ga=T,f=24,s=64,v=32,i=7,m=1;");
    Write(Output(),image,(LONG)(n/3));
    put("\030after CAN");
    /* An image cut off by a stray ESC. */
    put("\033[13;1Hbefore ESC \033_Ga=T,f=24,s=64,v=32,i=8,m=1;");
    Write(Output(),image,(LONG)(n/2));
    put("\033Xignored\033\\after ESC");
    /* A complete image beside text; the cursor stays (C=1). */
    put("\033[15;1Himage:\033[15;10H\033_Ga=T,f=24,s=64,v=32,i=9,C=1,q=2;");
    Write(Output(),image,(LONG)n);
    put("\033\\\033[15;22Hbeside the image\033[17;22Hsecond row beside it");
    put("\033[19;1Hdone\r\n");
    return 0;
}
