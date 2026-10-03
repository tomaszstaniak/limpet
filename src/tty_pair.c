#include "tty_pair.h"
#include <string.h>
#include <limits.h>
#define RECORD_END 512u
#define RECORD_EOF 256u
static int valid_end(TtyEnd e) { return e==TTY_MASTER || e==TTY_SLAVE; }
static TtyResult done(TtyStatus s,size_t n) { TtyResult r={s,n};return r; }
static size_t room(const TtyRing *q) { return TTY_QUEUE_CAPACITY-q->count; }
static void put(TtyRing *q,uint16_t c)
{ q->data[(q->head+q->count)%TTY_QUEUE_CAPACITY]=c;++q->count; }
static uint16_t take(TtyRing *q)
{
    uint16_t c=q->data[q->head];
    q->head=(q->head+1)%TTY_QUEUE_CAPACITY;--q->count;return c;
}
static void commit_line(TtyPair *p,int delimiter)
{
    size_t i;
    for(i=0;i<p->line_count;++i)
        put(&p->input,(uint16_t)(p->line[i] |
            ((!delimiter && i+1==p->line_count)?RECORD_END:0)));
    if(delimiter) put(&p->input,(uint16_t)((delimiter-1)|RECORD_END));
    else if(!p->line_count) put(&p->input,RECORD_EOF);
    p->line_count=0;p->line_ready=0;
}
static void publish_tail(TtyPair *p)
{
    if(p->line_count && (p->line_ready || !p->refs[TTY_MASTER]) &&
       room(&p->input)>=p->line_count) commit_line(p,0);
}
static size_t output_size(const TtyPair *p,unsigned char c)
{
    return c=='\n' && (p->config.output_flags&(TTY_OPOST|TTY_ONLCR))==
        (TTY_OPOST|TTY_ONLCR)?2:1;
}
static void output_byte(TtyPair *p,unsigned char c)
{
    if(output_size(p,c)==2) put(&p->output,'\r');
    put(&p->output,c);
}
void tty_pair_init(TtyPair *p)
{
    memset(p,0,sizeof *p);p->refs[0]=p->refs[1]=1;p->config.vmin=1;
    p->config.version=TTY_PAIR_VERSION;p->config.eof=4;
    p->geometry.version=TTY_PAIR_VERSION;p->geometry.rows=24;p->geometry.columns=80;
}
TtyStatus tty_pair_retain(TtyPair *p,TtyEnd e)
{
    if(!p || !valid_end(e)) return TTY_INVALID;
    if(!p->refs[e]) return TTY_CLOSED;
    if(p->refs[e]==UINT_MAX) return TTY_BUSY;
    ++p->refs[e];return TTY_OK;
}
TtyStatus tty_pair_release(TtyPair *p,TtyEnd e)
{
    if(!p || !valid_end(e)) return TTY_INVALID;
    if(!p->refs[e]) return TTY_CLOSED;
    if(! --p->refs[e]) p->interrupt_pending=0;
    return TTY_OK;
}
int tty_pair_take_interrupt(TtyPair *p)
{
    int pending;if(!p) return 0;
    pending=p->interrupt_pending;p->interrupt_pending=0;return pending;
}
TtyResult tty_pair_write(TtyPair *p,TtyEnd e,const void *data,size_t n)
{
    const unsigned char *b=data;size_t i;
    if(!p || !valid_end(e) || (!b && n)) return done(TTY_INVALID,0);
    if(!p->refs[e]) return done(TTY_CLOSED,0);
    if(!n) return done(TTY_OK,0);
    if(!p->refs[1-e]) return done(TTY_CLOSED,0);
    if(e==TTY_MASTER) {
        if(p->interrupt_pending) return done(TTY_AGAIN,0);
        publish_tail(p);
    }
    for(i=0;i<n;++i) {
        unsigned char c=b[i];
        int canon=(p->config.local_flags&TTY_CANON)!=0;
        int echo,is_eof,is_eol,erase,kill;
        if(e==TTY_SLAVE) {
            if(room(&p->output)<output_size(p,c)) break;
            output_byte(p,c);continue;
        }
        /* Translate the received byte once, before canonical/echo handling.
           An ignored CR needs no queue space and still advances the writer. */
        if(c=='\r') {
            if(p->config.input_flags&TTY_IGNCR) continue;
            if(p->config.input_flags&TTY_ICRNL) c='\n';
        } else if(c=='\n' && (p->config.input_flags&TTY_INLCR)) c='\r';
        if((p->config.local_flags&TTY_ISIG) && p->config.intr==(unsigned)c+1) {
            if(!(p->config.local_flags&TTY_NOFLSH))
                tty_pair_flush(p,TTY_FLUSH_INPUT|TTY_FLUSH_OUTPUT);
            p->interrupt_pending=1;
            return done(TTY_OK,i+1);
        }
        if(p->line_ready) break;
        is_eof=canon && !(p->config.local_flags&TTY_NOEOF) && c==p->config.eof;
        is_eol=canon && !is_eof && (c=='\n' || p->config.eol==(unsigned)c+1);
        erase=canon && !is_eof && !is_eol && p->config.erase==(unsigned)c+1;
        kill=canon && !is_eof && !is_eol && !erase && p->config.kill==(unsigned)c+1;
        echo=((p->config.local_flags&TTY_ECHO)!=0 ||
              (canon && c=='\n' && (p->config.local_flags&TTY_ECHONL))) && !is_eof;
        if(erase || kill) {
            unsigned char kill_echo[2]={c,'\n'};
            const unsigned char *feedback=&c;size_t length=echo?1:0,j,need=0;
            if(erase && echo && (p->config.local_flags&TTY_ECHOE)) {
                feedback=(const unsigned char *)"\b \b";length=p->line_count?3:0;
            } else if(kill && echo && (p->config.local_flags&TTY_ECHOK)) {
                feedback=kill_echo;length=2;
            }
            for(j=0;j<length;++j) need+=output_size(p,feedback[j]);
            if(room(&p->output)<need) break;
            if(kill) p->line_count=0;
            else if(p->line_count) --p->line_count;
            for(j=0;j<length;++j) output_byte(p,feedback[j]);
            continue;
        }
        /* Reserve the complete echo before consuming any input or delimiter. */
        if(echo && room(&p->output)<output_size(p,c)) break;
        if(canon) {
            if(is_eol || is_eof) {
                size_t need=p->line_count+(is_eol?1:0);
                if(!need) need=1;
                if(room(&p->input)<need) break;
                commit_line(p,is_eol?(int)c+1:0);
            } else {
                if(p->line_count==TTY_LINE_CAPACITY) break;
                p->line[p->line_count++]=c;
            }
        } else {
            if(!room(&p->input)) break;
            put(&p->input,c);++p->input_arrived;
        }
        if(echo) output_byte(p,c);
    }
    return done(i?TTY_OK:TTY_AGAIN,i);
}
TtyResult tty_pair_read(TtyPair *p,TtyEnd e,void *data,size_t n)
{
    unsigned char *b=data;size_t i;TtyRing *q;
    if(!p || !valid_end(e) || (!b && n)) return done(TTY_INVALID,0);
    if(!p->refs[e]) return done(TTY_CLOSED,0);
    if(!n) return done(TTY_OK,0);
    q=e==TTY_MASTER?&p->output:&p->input;
    if(e==TTY_MASTER && p->output_stopped && p->refs[TTY_SLAVE]) return done(TTY_AGAIN,0);
    /* Preserve a frozen partial line after earlier records, including when
       full-ring pressure deferred its publication during a mode switch. */
    if(e==TTY_SLAVE) publish_tail(p);
    for(i=0;i<n && q->count;) {
        /* A raw prefix can now precede an old/new canonical EOF. Report the
           bytes first and leave the EOF for the next nonempty read. */
        if(i && q->data[q->head]==RECORD_EOF) break;
        uint16_t c=take(q);
        if(c==RECORD_EOF) return done(TTY_EOF,0);
        b[i++]=(unsigned char)c;
        if(c&RECORD_END) break;
    }
    if(e==TTY_MASTER) p->output_retired+=i;
    else publish_tail(p);
    return done(i?TTY_OK:(p->refs[1-e]?TTY_AGAIN:TTY_EOF),i);
}
TtyStatus tty_pair_read_status(const TtyPair *p,TtyEnd e)
{
    const TtyRing *q;
    if(!p || !valid_end(e)) return TTY_INVALID;
    if(!p->refs[e]) return TTY_CLOSED;
    q=e==TTY_MASTER?&p->output:&p->input;
    if(e==TTY_MASTER && p->output_stopped && p->refs[TTY_SLAVE]) return TTY_AGAIN;
    if(q->count) return q->data[q->head]==RECORD_EOF?TTY_EOF:TTY_OK;
    if(e==TTY_SLAVE && p->line_ready && p->line_count) return TTY_OK;
    if(!p->refs[1-e])
        return e==TTY_SLAVE && p->line_count?TTY_OK:TTY_EOF;
    return TTY_AGAIN;
}
int tty_pair_readable(const TtyPair *p,TtyEnd e)
{
    TtyStatus status=tty_pair_read_status(p,e);
    return status==TTY_OK || status==TTY_EOF;
}
size_t tty_pair_input_extent(const TtyPair *p,int *boundary)
{
    size_t i,n=0;*boundary=0;
    for(i=0;i<p->input.count;++i) {
        uint16_t c=p->input.data[(p->input.head+i)%TTY_QUEUE_CAPACITY];
        if(c==RECORD_EOF) { *boundary=1;break; }
        ++n;if(c&RECORD_END) { *boundary=1;break; }
    }
    if(!n && !p->input.count && p->line_ready) { n=p->line_count;*boundary=1; }
    return n;
}
TtyStatus tty_config_validate(const TtyConfig *c)
{
    if(!c) return TTY_INVALID;
    if(c->version!=TTY_PAIR_VERSION || (c->input_flags&~(TTY_ICRNL|TTY_IGNCR|TTY_INLCR)) ||
       (c->output_flags&~(TTY_OPOST|TTY_ONLCR)) ||
       (c->local_flags&~TTY_LOCAL_MASK)) return TTY_UNSUPPORTED;
    if((c->eof=='\n' && !(c->local_flags&TTY_NOEOF)) ||
       c->erase>256 || c->kill>256 || c->eol>256 || c->intr>256) return TTY_INVALID;
    return TTY_OK;
}
static TtyStatus configure(TtyPair *p,const TtyConfig *c,int discard_input)
{
    TtyStatus status;
    if(!p) return TTY_INVALID;
    status=tty_config_validate(c);if(status!=TTY_OK) return status;
    if(!p->refs[0] || !p->refs[1]) return TTY_CLOSED;
    if(discard_input) {
        p->input.head=p->input.count=0;
        p->line_count=0;p->line_ready=0;
    } else if((p->config.local_flags&TTY_CANON) && !(c->local_flags&TTY_CANON) && p->line_count)
        p->line_ready=1;
    p->config=*c;++p->config_generation;publish_tail(p);return TTY_OK;
}
TtyStatus tty_pair_configure(TtyPair *p,const TtyConfig *c)
{ return configure(p,c,0); }
TtyStatus tty_pair_configure_discard_input(TtyPair *p,const TtyConfig *c)
{ return configure(p,c,1); }
TtyStatus tty_pair_flush(TtyPair *p,unsigned mask)
{
    if(!p || !mask || (mask&~(TTY_FLUSH_INPUT|TTY_FLUSH_OUTPUT))) return TTY_INVALID;
    if(!p->refs[TTY_SLAVE]) return TTY_CLOSED;
    if(mask&TTY_FLUSH_INPUT) {
        p->input.head=p->input.count=0;
        p->line_count=0;p->line_ready=0;
        ++p->config_generation;
    }
    if(mask&TTY_FLUSH_OUTPUT) {
        p->output_retired+=p->output.count;
        p->output.head=p->output.count=0;
    }
    return TTY_OK;
}
TtyStatus tty_pair_output_flow(TtyPair *p,int stopped)
{
    if(!p || (stopped!=0 && stopped!=1)) return TTY_INVALID;
    if(!p->refs[TTY_MASTER] || !p->refs[TTY_SLAVE]) return TTY_CLOSED;
    p->output_stopped=stopped;return TTY_OK;
}
TtyStatus tty_pair_get_config(const TtyPair *p,uint32_t v,TtyConfig *c)
{
    if(!p || !c) return TTY_INVALID;
    if(v!=TTY_PAIR_VERSION) return TTY_UNSUPPORTED;
    *c=p->config;return TTY_OK;
}
TtyStatus tty_pair_set_geometry(TtyPair *p,const TtyGeometry *g)
{
    if(!p || !g) return TTY_INVALID;
    if(g->version!=TTY_PAIR_VERSION) return TTY_UNSUPPORTED;
    if(!g->rows || !g->columns || g->rows>65535 || g->columns>65535 ||
       g->xpixel>65535 || g->ypixel>65535) return TTY_INVALID;
    if(!p->refs[0] || !p->refs[1]) return TTY_CLOSED;
    p->geometry=*g;return TTY_OK;
}
TtyStatus tty_pair_get_geometry(const TtyPair *p,uint32_t v,TtyGeometry *g)
{
    if(!p || !g) return TTY_INVALID;
    if(v!=TTY_PAIR_VERSION) return TTY_UNSUPPORTED;
    *g=p->geometry;return TTY_OK;
}
