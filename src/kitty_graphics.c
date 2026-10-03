#include "kitty_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { KSTATE_IDLE, KSTATE_CONTROL, KSTATE_PAYLOAD, KSTATE_IGNORE };

/* Parsed control data of one APC string. Values are kept as text so that
   single-letter values (a=T) and numbers share one representation. */
typedef struct {
    int present[128];
    long long value[128]; /* ids reach 2^32-1: wider than a 32-bit long */
    char letter[128];
} Keys;

static void memory_update(XKitty *k)
{
    size_t total=0;unsigned i;
    for(i=0;i<XKITTY_MAX_IMAGES;++i) total+=k->images[i].bytes;
    if(k->transmitting) total+=k->tx_bytes;
    k->stats.memory_bytes=total;
    if(total>k->stats.memory_peak) k->stats.memory_peak=total;
}

static void drop_transmission(XKitty *k,int counted)
{
    if(!k->transmitting) return;
    free(k->tx_pixels);
    k->tx_pixels=NULL;k->transmitting=0;k->tx_bytes=k->tx_filled=0;
    if(counted) ++k->stats.transmissions_dropped;
    memory_update(k);
}

void xkitty_init(XKitty *k,const XKittyLimits *limits)
{
    memset(k,0,sizeof *k);
    k->limits=*limits;
}

void xkitty_reset(XKitty *k)
{
    unsigned i;
    drop_transmission(k,1);
    for(i=0;i<XKITTY_MAX_IMAGES;++i) free(k->images[i].pixels);
    memset(k->images,0,sizeof k->images);
    k->state=KSTATE_IDLE;
    memory_update(k);
}

static XKittyImage *find_image(XKitty *k,uint32_t id)
{
    unsigned i;
    if(!id) return NULL;
    for(i=0;i<XKITTY_MAX_IMAGES;++i) if(k->images[i].id==id) return &k->images[i];
    return NULL;
}

static int parse_keys(const char *text,size_t length,Keys *keys)
{
    size_t i=0;
    memset(keys,0,sizeof *keys);
    while(i<length) {
        unsigned char key=(unsigned char)text[i];
        long long value=0;int negative=0,digits=0;
        if(key>=128 || i+1>=length || text[i+1]!='=') return 0;
        i+=2;
        if(i<length && (text[i]=='-' || (text[i]>='0' && text[i]<='9'))) {
            if(text[i]=='-') { negative=1;++i; }
            /* Check after every digit: a wrapped id would name another image. */
            while(i<length && text[i]>='0' && text[i]<='9') {
                value=value*10+(text[i]-'0');++i;++digits;
                if(value>4294967295LL) return 0;
            }
            if(!digits) return 0;
            keys->value[key]=negative?-value:value;
        } else if(i<length && text[i]!=',') {
            keys->letter[key]=text[i];++i;
        } else return 0;
        keys->present[key]=1;
        if(i<length) { if(text[i]!=',') return 0; ++i; }
    }
    return 1;
}

/* Keys of the protocol this subset does not implement. Any use of one is
   ENOTSUPPORTED rather than silently ignored. */
static const char *unsupported_key(const Keys *keys)
{
    static const char unsupported[]="IpxywhcrXYzUSOPQHVo";
    const char *c;
    for(c=unsupported;*c;++c)
        if(keys->present[(unsigned char)*c] && (keys->value[(unsigned char)*c] || keys->letter[(unsigned char)*c]))
            return "ENOTSUPPORTED:key";
    return NULL;
}

static const char *check_format(XKitty *k,const Keys *keys,unsigned *width,unsigned *height,unsigned *channels)
{
    long f=keys->present['f']?keys->value['f']:32;
    char t=keys->present['t']?keys->letter['t']:'d';
    if(keys->present['o']) return "ENOTSUPPORTED:compression";
    if(t!='d') return "ENOTSUPPORTED:transmission medium";
    if(f!=24 && f!=32) return "ENOTSUPPORTED:format";
    if(!keys->present['s'] || !keys->present['v'] || keys->value['s']<=0 || keys->value['v']<=0)
        return "EINVAL:width and height required";
    if((unsigned long)keys->value['s']>k->limits.max_width || (unsigned long)keys->value['v']>k->limits.max_height)
        return "EINVAL:dimensions exceed limit";
    *width=(unsigned)keys->value['s'];*height=(unsigned)keys->value['v'];*channels=f==24?3:4;
    if((size_t)*width*(size_t)*height*(size_t)*channels>k->limits.max_image_bytes)
        return "EINVAL:image exceeds size limit";
    return NULL;
}

/* The control data has been read (payload follows, if any). Decide what
   this string is and open a transmission if needed. */
static void begin_command(XKitty *k)
{
    Keys keys;const char *error=NULL;size_t i;int only_chunk_keys=1;
    int reply_shape=1;
    k->cmd_error=NULL;k->cmd_continuation=k->cmd_echo=0;k->cmd_more=0;k->cmd_quiet=0;k->cmd_id=0;k->cmd_action='t';
    if(!parse_keys(k->control,k->control_length,&keys)) {
        /* Unparseable: no id is known, so there is nobody to answer. */
        drop_transmission(k,1);
        k->cmd_error="EINVAL:control data";return;
    }
    for(i=0;i<128;++i) if(keys.present[i] && i!='m' && i!='q') only_chunk_keys=0;
    k->cmd_more=keys.present['m'] && keys.value['m']==1;
    k->cmd_quiet=keys.present['q']?(unsigned)keys.value['q']:0;
    if(k->transmitting && only_chunk_keys) {
        k->cmd_continuation=1;k->cmd_id=k->tx_id;k->cmd_action='t';
        if(keys.present['q']) k->tx_quiet=k->cmd_quiet;
        return;
    }
    for(i=0;i<128;++i) if(keys.present[i] && i!='i' && i!='I' && i!='p') reply_shape=0;
    if(reply_shape && keys.present['i']) { k->cmd_echo=1;return; }
    /* Anything else while a transmission is open interrupts it. */
    drop_transmission(k,1);
    k->cmd_action=keys.present['a']?keys.letter['a']:'t';
    k->cmd_id=keys.present['i'] && keys.value['i']>0?(uint32_t)keys.value['i']:0;
    /* Deleting everything needs no id; every other command names an image. */
    if(!k->cmd_id && !(k->cmd_action=='d' && (!keys.present['d'] ||
                       keys.letter['d']=='a' || keys.letter['d']=='A'))) {
        k->cmd_error="EINVAL:image id required";return;
    }
    if((error=unsupported_key(&keys))) { k->cmd_error=error;return; }
    if(k->cmd_action=='t' || k->cmd_action=='T' || k->cmd_action=='q') {
        unsigned width,height,channels;size_t bytes;
        if((error=check_format(k,&keys,&width,&height,&channels))) { k->cmd_error=error;return; }
        if(k->cmd_action=='q') return;
        bytes=(size_t)width*height*channels;
        if(!find_image(k,k->cmd_id)) {
            for(i=0;i<XKITTY_MAX_IMAGES && k->images[i].id;++i) {}
            if(i==XKITTY_MAX_IMAGES) { k->cmd_error="ENOSPC:too many images";return; }
        }
        if(k->stats.memory_bytes+bytes>k->limits.max_total_bytes) { k->cmd_error="ENOSPC:image memory limit";return; }
        if(!(k->tx_pixels=malloc(bytes))) { k->cmd_error="ENOMEM:out of memory";return; }
        k->transmitting=1;k->tx_id=k->cmd_id;k->tx_width=width;k->tx_height=height;
        k->tx_channels=channels;k->tx_bytes=bytes;k->tx_filled=0;
        k->tx_quad=k->tx_quad_length=k->tx_padding=k->tx_error=0;
        k->tx_quiet=k->cmd_quiet;k->tx_display=k->cmd_action=='T';
        k->tx_cursor=keys.present['C']?(unsigned)keys.value['C']:0;
        memory_update(k);
        return;
    }
    if(k->cmd_action=='p') {
        if(!find_image(k,k->cmd_id)) k->cmd_error="ENOENT:no such image";
        k->cmd_cursor=keys.present['C']?(unsigned)keys.value['C']:0;
        return;
    }
    if(k->cmd_action=='d') {
        k->cmd_delete=keys.present['d']?keys.letter['d']:'a';
        if(k->cmd_delete!='a' && k->cmd_delete!='A' && k->cmd_delete!='i' && k->cmd_delete!='I')
            k->cmd_error="ENOTSUPPORTED:delete target";
        return;
    }
    k->cmd_error="ENOTSUPPORTED:action";
}

static int base64_value(unsigned char c)
{
    if(c>='A' && c<='Z') return c-'A';
    if(c>='a' && c<='z') return c-'a'+26;
    if(c>='0' && c<='9') return c-'0'+52;
    if(c=='+') return 62;
    if(c=='/') return 63;
    return -1;
}

static void payload(XKitty *k,const unsigned char *bytes,size_t length)
{
    size_t i;
    k->stats.payload_bytes+=length;
    if(k->cmd_echo || k->cmd_error || !k->transmitting || k->tx_error) return;
    for(i=0;i<length;++i) {
        int v;
        if(bytes[i]=='=') { ++k->tx_padding; v=0; }
        else if(k->tx_padding || (v=base64_value(bytes[i]))<0) { k->tx_error=1;return; }
        k->tx_quad=(k->tx_quad<<6)|(unsigned)v;
        if(++k->tx_quad_length==4) {
            unsigned n=3-k->tx_padding,j;
            if(k->tx_padding>2 || k->tx_filled+n>k->tx_bytes) { k->tx_error=1;return; }
            for(j=0;j<n;++j) k->tx_pixels[k->tx_filled++]=(unsigned char)(k->tx_quad>>(16-8*j));
            k->tx_quad=k->tx_quad_length=0;
            /* Padding ends a base64 unit; a later chunk starts afresh. */
            k->tx_padding=0;
        }
    }
}

void xkitty_apc_begin(XKitty *k)
{
    k->state=KSTATE_CONTROL;k->is_kitty=-1;k->control_length=0;
    k->cmd_error=NULL;k->cmd_id=0;k->cmd_quiet=0;
}

void xkitty_apc_data(XKitty *k,const unsigned char *bytes,size_t length)
{
    size_t i=0;
    if(k->state==KSTATE_CONTROL) {
        for(;i<length;++i) {
            if(k->is_kitty<0) {
                /* Other APC strings are not ours: ignore them entirely. */
                k->is_kitty=bytes[i]=='G';
                if(!k->is_kitty) { k->state=KSTATE_IGNORE;return; }
                continue;
            }
            if(bytes[i]==';') {
                begin_command(k);k->state=KSTATE_PAYLOAD;++i;break;
            }
            if(k->control_length>=XKITTY_CONTROL_MAX) {
                drop_transmission(k,1);
                k->cmd_error="EINVAL:control data too long";k->state=KSTATE_IGNORE;return;
            }
            k->control[k->control_length++]=(char)bytes[i];
        }
    }
    if(k->state==KSTATE_PAYLOAD && i<length) payload(k,bytes+i,length-i);
}

void xkitty_apc_abort(XKitty *k)
{
    /* A string cut short by CAN/SUB or a stray escape: its data is gone,
       so an open transmission cannot complete. */
    if(k->is_kitty>0) drop_transmission(k,1);
    k->state=KSTATE_IDLE;k->is_kitty=-1;k->control_length=0;
}

static void reply(XKittyResult *r,uint32_t id,unsigned quiet,const char *error)
{
    int n;
    if(!id || (!error && quiet>=1) || (error && quiet>=2)) return;
    n=snprintf((char *)r->reply,sizeof r->reply,"\033_Gi=%lu;%s\033\\",(unsigned long)id,error?error:"OK");
    r->reply_length=n>0 && (size_t)n<sizeof r->reply?(size_t)n:0;
}

static void place(XKitty *k,XKittyImage *image,const XKittyCursor *c,unsigned cursor_policy,XKittyResult *r)
{
    unsigned rows=(image->height+c->cell_height-1)/c->cell_height;
    unsigned columns=(image->width+c->cell_width-1)/c->cell_width;
    (void)k;
    image->placed=1;image->placed_row=(long)c->row+(long)c->scroll_count;
    image->placed_column=c->column;image->placed_alternate=c->alternate;
    r->changed=1;
    if(!cursor_policy) {
        unsigned row=c->row+(rows?rows-1:0),column=c->column+columns;
        r->move_cursor=1;
        r->cursor_row=row<c->rows?row:c->rows-1;
        r->cursor_column=column<c->columns?column:c->columns-1;
    }
}

int xkitty_apc_end(XKitty *k,const XKittyCursor *c,XKittyResult *r)
{
    int kitty;
    memset(r,0,sizeof *r);
    kitty=k->is_kitty>0;
    /* No ';': a command without payload (a=p, a=d, a continuation). */
    if(kitty && k->state==KSTATE_CONTROL) begin_command(k);
    k->state=KSTATE_IDLE;k->is_kitty=-1;k->control_length=0;
    if(!kitty) return 0;
    if(k->cmd_echo) { k->cmd_echo=0;return 1; }
    ++k->stats.commands;
    if(k->cmd_error) {
        ++k->stats.errors;
        if(k->transmitting && (k->cmd_continuation || k->tx_id==k->cmd_id)) drop_transmission(k,1);
        reply(r,k->cmd_id,k->cmd_quiet,k->cmd_error);
        return 1;
    }
    if(k->cmd_continuation || ((k->cmd_action=='t' || k->cmd_action=='T') && k->transmitting)) {
        XKittyImage *image;unsigned i;
        if(k->tx_error) {
            uint32_t id=k->tx_id;unsigned quiet=k->tx_quiet;
            drop_transmission(k,1);++k->stats.errors;
            reply(r,id,quiet,"EINVAL:malformed or oversized data");return 1;
        }
        if(k->cmd_more) return 1;
        if(k->tx_quad_length || k->tx_filled!=k->tx_bytes) {
            uint32_t id=k->tx_id;unsigned quiet=k->tx_quiet;
            drop_transmission(k,1);++k->stats.errors;
            reply(r,id,quiet,"EINVAL:data size does not match dimensions");return 1;
        }
        /* Complete: replace the image under this id. */
        image=find_image(k,k->tx_id);
        if(!image) {
            for(i=0;i<XKITTY_MAX_IMAGES && k->images[i].id;++i) {}
            image=&k->images[i]; /* begin_command reserved room */
            memset(image,0,sizeof *image);image->id=k->tx_id;
        }
        free(image->pixels);
        image->pixels=k->tx_pixels;image->bytes=k->tx_bytes;
        image->width=k->tx_width;image->height=k->tx_height;image->channels=k->tx_channels;
        ++image->generation;image->serial=++k->commits;++k->stats.frames_received;
        k->tx_pixels=NULL;k->transmitting=0;k->tx_bytes=k->tx_filled=0;
        memory_update(k);
        if(image->placed) r->changed=1;
        if(k->tx_display) place(k,image,c,k->tx_cursor,r);
        reply(r,image->id,k->tx_quiet,NULL);
        return 1;
    }
    switch(k->cmd_action) {
    case 'q':
        reply(r,k->cmd_id,k->cmd_quiet,NULL);
        return 1;
    case 'p':
        place(k,find_image(k,k->cmd_id),c,k->cmd_cursor,r);
        reply(r,k->cmd_id,k->cmd_quiet,NULL);
        return 1;
    case 'd': {
        unsigned i;char d=k->cmd_delete;
        for(i=0;i<XKITTY_MAX_IMAGES;++i) {
            XKittyImage *image=&k->images[i];
            if(!image->id || ((d=='i' || d=='I') && image->id!=k->cmd_id)) continue;
            if(image->placed) r->changed=1;
            image->placed=0;
            if(d=='A' || d=='I') { free(image->pixels);memset(image,0,sizeof *image); }
        }
        memory_update(k);
        reply(r,k->cmd_id,k->cmd_quiet,NULL);
        return 1;
    }
    }
    return 1;
}

int xkitty_placement_rows(const XKittyImage *image,const XKittyCursor *c,long *first,unsigned *count)
{
    if(!image->id || !image->placed || image->placed_alternate!=c->alternate || !c->cell_height) return 0;
    *first=image->placed_row-(long)c->scroll_count;
    *count=(image->height+c->cell_height-1)/c->cell_height;
    return 1;
}
