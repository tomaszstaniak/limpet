#include "../src/screen_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XTermScreen screen;
static XTermCell cells[80];
static XHyperlinks links;
static unsigned replies;
static unsigned char response[128];
static size_t response_length;
static void reply(void *context,const unsigned char *bytes,size_t count)
{
    (void)context;
    assert(count<sizeof response-response_length);
    memcpy(response+response_length,bytes,count); response_length+=count;
    response[response_length]=0; ++replies;
}
static void text(const char *p)
{ screen_write(&screen,(const unsigned char *)p,strlen(p)); }
static void init(unsigned char introducer)
{
    assert(!screen_init(&screen,cells,80,4,20));
    hyperlinks_init(&links); screen_attach_links(&screen,&links);
    replies=0; response_length=0; response[0]=0; screen.reply=reply;
    text("A\033"); screen_write(&screen,&introducer,1);
}
static void unchanged(void)
{
    unsigned r,c;
    assert(screen.cursor_row==0 && screen.cursor_column==1);
    assert(screen_cell(&screen,0,0)=='A');
    for(r=0;r<4;++r) for(c=0;c<20;++c)
        if(r || c) assert(screen_cell(&screen,r,c)==' ');
    assert(!replies && links.next_token==1 && !screen.active_link);
    assert(screen.style.fg==7 && !screen.alternate_active && !screen.mouse_tracking);
}
/* CAN/SUB abort pending commands, not the printable byte that follows.
 * Catch a parser that merely marks CSI invalid or keeps consuming OSC.
 * Expectations are literal; every split also separates ESC and ST. */
static void cancelled_commands(void)
{
    static const char *prefixes[]={"\033", "\033[31", "\033[6",
        "\033]8;;https://example.org/", "\033]8;;https://example.org/\033",
        "\033]0;ignored", "\033Ppayload", "\033_payload\033"};
    unsigned p,c,runs=0;
    size_t split,n;
    unsigned char input[128];
    for(p=0;p<sizeof prefixes/sizeof prefixes[0];++p) for(c=0;c<2;++c) {
        n=strlen(prefixes[p]); memcpy(input,prefixes[p],n);
        input[n++]=(unsigned char)(c?26:24); input[n++]='B';
        for(split=0;split<=n;++split) {
            init(']'); text("0;initial\a");
            screen_write(&screen,input,split);
            screen_write(&screen,input+split,n-split);
            assert(screen_cell(&screen,0,1)=='B' && screen.cursor_column==2);
            assert(!replies && links.next_token==1 && !screen.active_link);
            assert(screen.style.fg==7);
            text("\033]8;;https://example.org/\033\\C");
            assert(screen_cell(&screen,0,2)=='C' && screen.active_link!=0);
            ++runs;
        }
    }
    /* Aborting an OSC 8 close must not close an already committed link. */
    for(c=0;c<2;++c) {
        unsigned token;
        unsigned char cancel=(unsigned char)(c?26:24);
        init(']'); text("8;;https://example.org/\a");
        token=screen.active_link; assert(token);
        text("\033]8;;"); screen_write(&screen,&cancel,1); text("B");
        assert(screen.active_link==token && links.next_token==2);
        assert(screen_cell(&screen,0,1)=='B');
    }
    printf("VT CANCEL PASS: split_cases=%u committed_link_cases=2\n",runs);
}
/* NUL padding and DEL must not terminate ESC or poison CSI parameters.
 * Literal checks catch both dropped commands and leaked command text. */
static void padded_commands(void)
{
    static const char *commands[]={"\033[31m", "\033[6n"};
    unsigned which,pad,runs=0;
    size_t pos,split,n;
    unsigned char input[32];
    for(which=0;which<2;++which) for(pad=0;pad<2;++pad) {
        n=strlen(commands[which]);
        for(pos=1;pos<n;++pos) {
            memcpy(input,commands[which],pos);
            input[pos]=(unsigned char)(pad?127:0);
            memcpy(input+pos+1,commands[which]+pos,n-pos);
            input[n+1]='B';
            for(split=0;split<=n+2;++split) {
                init(']'); text("0;initial\a");
                screen_write(&screen,input,split);
                screen_write(&screen,input+split,n+2-split);
                assert(screen_cell(&screen,0,1)=='B' && screen.cursor_column==2);
                assert(screen.style.fg==(which?7:1));
                assert(replies==(which?1u:0u));
                if(which) assert(!strcmp((const char *)response,"\033[1;2R"));
                ++runs;
            }
        }
    }
    printf("VT PADDING PASS: split_cases=%u\n",runs);
}
/* Oversized OSC must remain bounded and inert until terminated, including
 * when a resize happens in mid-string. Afterwards queries and links recover.
 * This also catches accidentally applying ESC/CSI padding rules to URL bytes. */
static void oversized_osc(void)
{
    static unsigned char payload[XHYPERLINK_LIMIT+64];
    static const size_t chunks[]={1,17,sizeof payload};
    unsigned field,term,delivery,runs=0;
    size_t offset,count;
    for(field=0;field<2;++field) for(term=0;term<2;++term)
        for(delivery=0;delivery<3;++delivery) {
            init(']'); text(field?"8;":"8;;https://example.org/");
            memset(payload,'a',sizeof payload);
            for(offset=0;offset<sizeof payload;offset+=count) {
                count=sizeof payload-offset;
                if(count>chunks[delivery]) count=chunks[delivery];
                screen_write(&screen,payload+offset,count);
            }
            if(field) text(";https://example.org/");
            text("\xe2\xff\033[6n");
            unchanged();
            assert(screen.osc_length<XHYPERLINK_LIMIT && !screen.utf8_remaining);
            assert(!screen_resize(&screen,3,18));
            text(term?"\033\\":"\a");
            assert(screen.active_link==XHYPERLINK_INVALID);
            text("\033[6nB");
            assert(replies==1 && !strcmp((const char *)response,"\033[1;2R"));
            assert(screen_cell(&screen,0,1)=='B' && screen.cursor_column==2);
            text("\033]8;;https://example.org/\aC\033]8;;\aD");
            assert(screen_cell(&screen,0,2)=='C' && screen_cell(&screen,0,3)=='D');
            assert(!screen.active_link && links.next_token==2);
            ++runs;
        }
    for(term=0;term<2;++term) {
        unsigned char invalid=(unsigned char)(term?127:0);
        init(']'); text("8;;https://example.org/");
        screen_write(&screen,&invalid,1); text("\aB");
        assert(screen.active_link==XHYPERLINK_INVALID && links.next_token==1);
        assert(screen_cell(&screen,0,1)=='B');
    }
    printf("VT OSC RECOVERY PASS: oversized_cases=%u invalid_url_controls=2\n",runs);
}
int main(void)
{
    const unsigned char kinds[]={'P','X','^','_'};
    unsigned k,i;
    cancelled_commands();
    padded_commands();
    oversized_osc();
    for(k=0;k<sizeof kinds;++k) {
        unsigned stable_state;
        unsigned char block[257];
        init(kinds[k]);
        text("hidden"); unchanged();
        text("\033"); unchanged(); text("\\B");
        assert(screen_cell(&screen,0,1)=='B' && screen.cursor_column==2);
        init(kinds[k]);
        text("before\aafter\033[2J\033[31m\033[18t\033[?1000h\033]8;;https://example.org/\a");
        unchanged();
        text("\033Xnested\033\033xpayload"); unchanged();
        text("\033\\B"); assert(screen_cell(&screen,0,1)=='B');
        init(kinds[k]); stable_state=screen.parser;
        memset(block,0xe2,sizeof block);
        for(i=0;i<4096;++i) screen_write(&screen,block,sizeof block);
        unchanged(); assert(screen.parser==stable_state && !screen.utf8_remaining);
        text("\033\\\xc3\xa9\xe7\x95\x8c");
        assert(screen_cell(&screen,0,1)==0xe9 && screen_cell(&screen,0,2)==0x754c);
        for(i=0;i<2;++i) {
            unsigned char cancel=(unsigned char)(i?26:24);
            init(kinds[k]); text("hidden\033");
            screen_write(&screen,&cancel,1); text("B");
            assert(screen_cell(&screen,0,1)=='B' && screen.cursor_column==2);
            init(kinds[k]); screen_write(&screen,&cancel,1); text("C");
            assert(screen_cell(&screen,0,1)=='C');
        }
    }
    /* Raw C1 recognition must not consume UTF-8 continuation bytes. */
    assert(!screen_init(&screen,cells,80,4,20));
    text("\xc3\x90\xc3\x98\xc3\x9e\xc3\x9f");
    assert(screen_cell(&screen,0,0)==0xd0 && screen_cell(&screen,0,1)==0xd8);
    assert(screen_cell(&screen,0,2)==0xde && screen_cell(&screen,0,3)==0xdf);
    /* OSC keeps BEL termination and registration. */
    hyperlinks_init(&links); screen_attach_links(&screen,&links);
    text("\033]8;;https://example.org/\aZ");
    assert(screen_cell(&screen,0,4)=='Z' && screen.active_link!=0);
    puts("control strings: PASS (DCS/SOS/PM/APC discard, ST, cancellation, UTF-8)");
    return 0;
}
