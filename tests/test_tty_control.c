#include "../src/tty_control.h"
#include "../src/tty_control_server.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static TtyPair pair;
static void canonical_editing(void)
{
    XtyControl c;unsigned char b[32];TtyResult r;
    tty_pair_init(&pair);memset(&c,0,sizeof c);c.version=1;c.operation=XTY_SET_CONFIG;
    c.values[2]=1|2|4|8;c.values[3]=4;
    c.values[4]=128; /* VERASE = DEL + 1; zero disables a character. */
    c.values[5]=22;  /* VKILL = Ctrl-U + 1. */
    c.values[6]=60;  /* VEOL = ';' + 1. */
    assert(tty_control_apply(&pair,TTY_SLAVE,&c,sizeof c)==TTY_OK);
    assert(tty_pair_write(&pair,TTY_MASTER,"ab\177c;",5).count==5);
    r=tty_pair_read(&pair,TTY_SLAVE,b,sizeof b);assert(r.count==3 && !memcmp(b,"ac;",3));
    r=tty_pair_read(&pair,TTY_MASTER,b,sizeof b);assert(r.count==7 && !memcmp(b,"ab\b \bc;",7));
    assert(tty_pair_write(&pair,TTY_MASTER,"bad\025ok\n",7).count==7);
    r=tty_pair_read(&pair,TTY_SLAVE,b,sizeof b);assert(r.count==3 && !memcmp(b,"ok\n",3));
    r=tty_pair_read(&pair,TTY_MASTER,b,sizeof b);assert(r.count==8 && !memcmp(b,"bad\025\nok\n",8));
    puts("TTY CANONICAL EDIT WIRE PASS: erase, kill, EOL, exact echo");
}
static XtyControl request(uint32_t op)
{ XtyControl c;memset(&c,0,sizeof c);c.version=1;c.operation=op;return c; }
static void rejected(XtyControl *c,TtyEnd end,size_t size,TtyStatus status)
{
    XtyControl before=*c;TtyPair saved=pair;
    assert(tty_control_apply(&pair,end,c,size)==status);
    assert(!memcmp(c,&before,sizeof before));assert(!memcmp(&pair,&saved,sizeof pair));
}
/* A query must not consume data or temporary EOF, and must not turn an
   uncommitted canonical line into readable input. Wire literals pin the ABI. */
static void read_state(TtyEnd end,uint32_t expected)
{
    XtyControl c=request(6);TtyPair saved=pair;unsigned i;
    memset(c.values,0xa5,sizeof c.values);
    assert(tty_control_apply(&pair,end,&c,sizeof c)==TTY_OK);
    assert(c.values[0]==expected);
    for(i=1;i<12;++i) assert(c.values[i]==0);
    assert(!memcmp(&pair,&saved,sizeof pair));
}
static void readiness(void)
{
    unsigned char b[8];XtyControl c;
    tty_pair_init(&pair);
    read_state(TTY_MASTER,0);read_state(TTY_SLAVE,0);
    assert(tty_pair_write(&pair,TTY_MASTER,"\0x",2).count==2);
    read_state(TTY_SLAVE,1);read_state(TTY_SLAVE,1);read_state(TTY_MASTER,0);
    assert(tty_pair_read(&pair,TTY_SLAVE,b,1).count==1 && b[0]==0);
    read_state(TTY_SLAVE,1);
    assert(tty_pair_read(&pair,TTY_SLAVE,b,8).count==1 && b[0]=='x');
    read_state(TTY_SLAVE,0);
    c=request(XTY_SET_CONFIG);c.values[2]=1;c.values[3]=4;
    assert(tty_control_apply(&pair,TTY_SLAVE,&c,sizeof c)==TTY_OK);
    assert(tty_pair_write(&pair,TTY_MASTER,"a",1).count==1);
    read_state(TTY_SLAVE,0);
    assert(tty_pair_write(&pair,TTY_MASTER,"\n\004",2).count==2);
    read_state(TTY_SLAVE,1);
    assert(tty_pair_read(&pair,TTY_SLAVE,b,8).count==2 && !memcmp(b,"a\n",2));
    read_state(TTY_SLAVE,2);read_state(TTY_SLAVE,2);
    assert(tty_pair_read(&pair,TTY_SLAVE,b,8).status==TTY_EOF);
    read_state(TTY_SLAVE,0);
    assert(tty_pair_write(&pair,TTY_MASTER,"tail",4).count==4);
    read_state(TTY_SLAVE,0);
    assert(tty_pair_release(&pair,TTY_MASTER)==TTY_OK);
    read_state(TTY_SLAVE,1);
    assert(tty_pair_read(&pair,TTY_SLAVE,b,8).count==4 && !memcmp(b,"tail",4));
    read_state(TTY_SLAVE,2);read_state(TTY_SLAVE,2);
    c=request(6);rejected(&c,TTY_MASTER,sizeof c,TTY_CLOSED);
    tty_pair_init(&pair);
    assert(tty_pair_write(&pair,TTY_SLAVE,"out",3).count==3);
    assert(tty_pair_release(&pair,TTY_SLAVE)==TTY_OK);
    read_state(TTY_MASTER,1);
    assert(tty_pair_read(&pair,TTY_MASTER,b,8).count==3 && !memcmp(b,"out",3));
    read_state(TTY_MASTER,2);
}
/* Wire flags must round-trip on either descriptor and alter actual bytes,
   not merely be accepted and cached in the response. */
static void input_flags(void)
{
    static const char *want[]={"\r\n","\n\n","\n","\n","\r\r","\n\r","\r","\r"};
    unsigned flags;unsigned char b[4];XtyControl c;
    for(flags=0;flags<8;++flags) {
        tty_pair_init(&pair);
        c=request(XTY_SET_CONFIG);c.values[0]=flags;c.values[3]=4;
        assert(tty_control_apply(&pair,TTY_SLAVE,&c,sizeof c)==TTY_OK);
        assert(c.values[0]==flags);
        c=request(XTY_GET_CONFIG);
        assert(tty_control_apply(&pair,TTY_MASTER,&c,sizeof c)==TTY_OK);
        assert(c.values[0]==flags);
        assert(tty_pair_write(&pair,TTY_MASTER,"\r\n",2).count==2);
        assert(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b).count==strlen(want[flags]));
        assert(!memcmp(b,want[flags],strlen(want[flags])));
    }
    puts("TTY INPUT FLAGS WIRE PASS: combinations=8 shared_snapshot=8");
}
static void flush_queue(unsigned mask)
{
    XtyControl c=request(7);unsigned i;c.values[0]=mask;
    assert(tty_control_apply(&pair,TTY_SLAVE,&c,sizeof c)==TTY_OK);
    assert(c.reserved[0]==XTY_PACKET_MAGIC);
    for(i=0;i<12;++i) assert(c.values[i]==0);
}
/* Catches conflating input/output queues, retained VEOF/partial lines,
   resetting configuration, and resurrecting buffered input after hangup. */
static void flushing(void)
{
    unsigned mask,i;unsigned char b[16];XtyControl c;TtyConfig config;
    for(mask=1;mask<=3;++mask) {
        tty_pair_init(&pair);config=pair.config;config.local_flags=TTY_CANON;
        assert(tty_pair_configure(&pair,&config)==TTY_OK);
        assert(tty_pair_write(&pair,TTY_MASTER,"old\n\004tail",9).count==9);
        assert(tty_pair_write(&pair,TTY_SLAVE,"output",6).count==6);
        flush_queue(mask);flush_queue(mask); /* Empty flush is idempotent. */
        assert(!memcmp(&pair.config,&config,sizeof config));
        if(mask&1) {
            read_state(TTY_SLAVE,0);
            assert(tty_pair_write(&pair,TTY_MASTER,"new\n",4).count==4);
            assert(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b).count==4);
            assert(!memcmp(b,"new\n",4));
        } else {
            assert(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b).count==4);
            assert(!memcmp(b,"old\n",4));
            assert(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b).status==TTY_EOF);
            assert(tty_pair_write(&pair,TTY_MASTER,"\n",1).count==1);
            assert(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b).count==5);
            assert(!memcmp(b,"tail\n",5));
        }
        if(mask&2) read_state(TTY_MASTER,0);
        else {
            assert(tty_pair_read(&pair,TTY_MASTER,b,sizeof b).count==6);
            assert(!memcmp(b,"output",6));
        }
    }
    tty_pair_init(&pair);config=pair.config;config.local_flags=TTY_CANON;
    assert(tty_pair_configure(&pair,&config)==TTY_OK);
    assert(tty_pair_write(&pair,TTY_MASTER,"tail",4).count==4);
    assert(tty_pair_release(&pair,TTY_MASTER)==TTY_OK);
    flush_queue(1);read_state(TTY_SLAVE,2);
    assert(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b).status==TTY_EOF);
    tty_pair_init(&pair);
    assert(tty_pair_write(&pair,TTY_MASTER,"keep",4).count==4);
    c=request(7);c.values[0]=1;rejected(&c,TTY_MASTER,64,TTY_UNSUPPORTED);
    c.values[0]=0;rejected(&c,TTY_SLAVE,64,TTY_INVALID);
    for(i=2;i<32;++i) {
        c.values[0]=1|(UINT32_C(1)<<i);rejected(&c,TTY_SLAVE,64,TTY_INVALID);
    }
    for(i=1;i<12;++i) {
        c=request(7);c.values[0]=1;c.values[i]=1;
        rejected(&c,TTY_SLAVE,64,TTY_INVALID);
    }
    assert(tty_pair_release(&pair,TTY_SLAVE)==TTY_OK);
    c=request(7);c.values[0]=3;rejected(&c,TTY_SLAVE,64,TTY_CLOSED);
    puts("TTY FLUSH PASS: directions=3 canonical/EOF, idempotence, hangup, atomic rejection");
}
/* Validation must precede discard. Success removes old records/EOF/tail,
   changes mode in the same operation, and preserves already queued output. */
static void configure_discard(void)
{
    unsigned i;XtyControl c;TtyConfig config;unsigned char b[16];
    tty_pair_init(&pair);config=pair.config;config.local_flags=TTY_CANON;
    assert(tty_pair_configure(&pair,&config)==TTY_OK);
    assert(tty_pair_write(&pair,TTY_MASTER,"old\n\004tail",9).count==9);
    assert(tty_pair_write(&pair,TTY_SLAVE,"output",6).count==6);
    c=request(8);c.values[0]=4;c.values[3]=4;
    rejected(&c,TTY_MASTER,64,TTY_UNSUPPORTED);
    for(i=4;i<12;++i) {
        c.values[i]=i<10?257:1;rejected(&c,TTY_SLAVE,64,TTY_INVALID);c.values[i]=0;
    }
    c.values[3]=256;rejected(&c,TTY_SLAVE,64,TTY_INVALID);
    c.values[3]='\n';rejected(&c,TTY_SLAVE,64,TTY_INVALID);c.values[3]=4;
    c.values[0]=8;rejected(&c,TTY_SLAVE,64,TTY_UNSUPPORTED);c.values[0]=4;
    c.values[1]=4;rejected(&c,TTY_SLAVE,64,TTY_UNSUPPORTED);c.values[1]=0;
    c.values[2]=256;rejected(&c,TTY_SLAVE,64,TTY_UNSUPPORTED);c.values[2]=0;
    /* An ordinary same-discipline SET must leave records and tail intact. */
    {
        XtyControl keep=request(XTY_SET_CONFIG);keep.values[2]=XTY_CANON;keep.values[3]=4;
        assert(tty_control_apply(&pair,TTY_SLAVE,&keep,sizeof keep)==TTY_OK);
        assert(pair.input.count==5 && pair.line_count==4);
    }
    assert(tty_control_apply(&pair,TTY_SLAVE,&c,sizeof c)==TTY_OK);
    assert(c.operation==8 && c.values[0]==4 && !c.values[1] && !c.values[2] && c.values[3]==4);
    read_state(TTY_SLAVE,0);
    assert(tty_pair_write(&pair,TTY_MASTER,"\n\004",2).count==2);
    assert(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b).count==2 && !memcmp(b,"\r\004",2));
    assert(tty_pair_read(&pair,TTY_MASTER,b,sizeof b).count==6 && !memcmp(b,"output",6));
    tty_pair_init(&pair);config=pair.config;config.local_flags=TTY_CANON;
    assert(tty_pair_configure(&pair,&config)==TTY_OK);
    assert(tty_pair_write(&pair,TTY_MASTER,"tail",4).count==4);
    assert(tty_pair_release(&pair,TTY_MASTER)==TTY_OK);
    c=request(8);c.values[3]=4;rejected(&c,TTY_SLAVE,64,TTY_CLOSED);
    assert(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b).count==4 && !memcmp(b,"tail",4));
    puts("TTY CONFIG DISCARD PASS: validation before discard, raw transition, output preservation, closed peer");
}
static void async_rejected(XtyControl *c,TtyEnd end,size_t size,TtyStatus status)
{
    XtyControl before=*c;TtyOperation op=TTY_REQUEST_READ;TtyConfig config,saved;
    memset(&config,0xa5,sizeof config);saved=config;
    assert(tty_control_prepare_async(end,c,size,&op,&config)==status);
    assert(!memcmp(c,&before,sizeof before) && !memcmp(&config,&saved,sizeof saved));
    assert(op==TTY_REQUEST_READ);
}
static void async_controls(void)
{
    unsigned op,i;XtyControl c,before;TtyOperation operation;TtyConfig config;
    unsigned char unaligned[65];
    for(op=9;op<=11;++op) {
        c=request(op);if(op!=9) { c.values[0]=7;c.values[1]=3;c.values[2]=3;c.values[3]=4; }
        before=c;memcpy(unaligned+1,&c,sizeof c);
        assert(tty_control_prepare_async(TTY_SLAVE,unaligned+1,64,&operation,&config)==TTY_OK);
        assert(operation==(op==9?TTY_REQUEST_DRAIN:op==10?TTY_REQUEST_DRAIN_CONFIG:TTY_REQUEST_DRAIN_CONFIG_DISCARD));
        if(op!=9) assert(config.input_flags==(TTY_ICRNL|TTY_IGNCR|TTY_INLCR) &&
            config.output_flags==(TTY_OPOST|TTY_ONLCR) && config.local_flags==(TTY_CANON|TTY_ECHO) && config.eof==4);
        assert(!memcmp(unaligned+1,&before,64));
        async_rejected(&c,TTY_MASTER,64,TTY_UNSUPPORTED);
        async_rejected(&c,TTY_SLAVE,63,TTY_INVALID);async_rejected(&c,TTY_SLAVE,65,TTY_INVALID);
        c.version=99;async_rejected(&c,TTY_SLAVE,64,TTY_UNSUPPORTED);c=before;
        for(i=0;i<2;++i) { c.reserved[i]=1;async_rejected(&c,TTY_SLAVE,64,TTY_INVALID);c=before; }
        for(i=op==9?0:10;i<12;++i) { c.values[i]=1;async_rejected(&c,TTY_SLAVE,64,TTY_INVALID);c=before; }
        if(op!=9) {
            c.values[3]=256;async_rejected(&c,TTY_SLAVE,64,TTY_INVALID);
            c.values[3]='\n';async_rejected(&c,TTY_SLAVE,64,TTY_INVALID);c=before;
            for(i=0;i<3;++i) { c.values[i]=UINT32_C(0x80000000);async_rejected(&c,TTY_SLAVE,64,TTY_UNSUPPORTED);c=before; }
        }
        tty_pair_init(&pair);rejected(&c,TTY_SLAVE,64,TTY_UNSUPPORTED);
    }
    puts("TTY ASYNC CONTROL PASS: decode, unaligned input, validation, unchanged rejection, no immediate completion");
}
int main(void)
{
    /* Signal configuration now round-trips; pair-only CAPS still omit delivery. */
    {
        XtyControl c;TtyConfig config;
        tty_pair_init(&pair);config=pair.config;config.local_flags=TTY_ISIG;config.intr=4;
        assert(tty_pair_configure(&pair,&config)==TTY_OK);
        memset(&c,0,sizeof c);c.version=1;c.operation=XTY_GET_CONFIG;
        assert(tty_control_apply(&pair,TTY_SLAVE,&c,sizeof c)==TTY_OK);
        assert(c.values[2]==64 && c.values[9]==4);
    }
    {
        XtyControl flow;unsigned char out[8];unsigned field;
        tty_pair_init(&pair);assert(tty_pair_write(&pair,TTY_SLAVE,"held",4).count==4);
        flow=request(12);rejected(&flow,TTY_SLAVE,64,TTY_INVALID);
        flow.values[0]=3;rejected(&flow,TTY_SLAVE,64,TTY_INVALID);
        flow.values[0]=1;rejected(&flow,TTY_MASTER,64,TTY_UNSUPPORTED);
        for(field=1;field<12;++field) {
            flow.values[field]=1;rejected(&flow,TTY_SLAVE,64,TTY_INVALID);
            flow.values[field]=0;
        }
        flow=request(12);flow.values[0]=1;
        assert(tty_control_apply(&pair,TTY_SLAVE,&flow,sizeof flow)==TTY_OK);
        assert(tty_pair_read(&pair,TTY_MASTER,out,sizeof out).status==TTY_AGAIN);
        assert(tty_pair_read_status(&pair,TTY_MASTER)==TTY_AGAIN);
        flow=request(12);flow.values[0]=2;
        assert(tty_control_apply(&pair,TTY_SLAVE,&flow,sizeof flow)==TTY_OK);
        assert(tty_pair_read(&pair,TTY_MASTER,out,sizeof out).count==4 && !memcmp(out,"held",4));
    }
    canonical_editing();
    XtyControl c;unsigned char bytes[65],b[4];unsigned i;
    async_controls();configure_discard();flushing();input_flags();readiness();
    tty_pair_init(&pair);
    c=request(XTY_CAPS);assert(tty_control_apply(&pair,TTY_SLAVE,&c,64)==TTY_OK);
    assert(c.values[0]==5247 && c.values[1]==7 && c.values[2]==3 && c.values[3]==63);
    assert(c.reserved[0]==XTY_PACKET_MAGIC && c.reserved[1]==0);
    c=request(XTY_CAPS);assert(tty_control_apply(&pair,TTY_MASTER,&c,64)==TTY_OK);
    assert(c.values[0]==29);
    c=request(XTY_SET_CONFIG);c.values[0]=1;c.values[1]=3;c.values[2]=3;c.values[3]=4;
    rejected(&c,TTY_MASTER,64,TTY_UNSUPPORTED);
    assert(tty_control_apply(&pair,TTY_SLAVE,&c,64)==TTY_OK);
    c=request(XTY_GET_CONFIG);memset(c.values,0xa5,sizeof c.values);
    memcpy(bytes+1,&c,64);
    assert(tty_control_apply(&pair,TTY_MASTER,bytes+1,64)==TTY_OK);
    memcpy(&c,bytes+1,64);
    assert(c.values[0]==1 && c.values[1]==3 && c.values[2]==3 && c.values[3]==4);
    for(i=4;i<12;++i) assert(c.values[i]==0);
    c=request(XTY_SET_GEOMETRY);c.values[0]=31;c.values[1]=109;c.values[2]=872;c.values[3]=527;
    assert(tty_control_apply(&pair,TTY_SLAVE,&c,64)==TTY_OK);
    c=request(XTY_GET_GEOMETRY);assert(tty_control_apply(&pair,TTY_MASTER,&c,64)==TTY_OK);
    assert(c.values[0]==31 && c.values[1]==109 && c.values[2]==872 && c.values[3]==527);
    c=request(XTY_GET_CONFIG);c.version=99;rejected(&c,TTY_SLAVE,64,TTY_UNSUPPORTED);
    c=request(99);rejected(&c,TTY_SLAVE,64,TTY_UNSUPPORTED);
    c=request(XTY_GET_CONFIG);c.reserved[1]=1;rejected(&c,TTY_SLAVE,64,TTY_INVALID);
    c=request(XTY_GET_CONFIG);rejected(&c,TTY_SLAVE,63,TTY_INVALID);
    rejected(&c,TTY_SLAVE,65,TTY_INVALID);rejected(&c,(TtyEnd)9,64,TTY_INVALID);
    assert(tty_control_apply(&pair,TTY_SLAVE,NULL,64)==TTY_INVALID);
    c=request(XTY_SET_CONFIG);c.values[3]=256;rejected(&c,TTY_SLAVE,64,TTY_INVALID);
    c.values[3]=4;c.values[0]=8;rejected(&c,TTY_SLAVE,64,TTY_UNSUPPORTED);
    c.values[0]=0;c.values[11]=1;rejected(&c,TTY_SLAVE,64,TTY_INVALID);
    c=request(XTY_SET_GEOMETRY);c.values[0]=1;c.values[1]=65536;
    rejected(&c,TTY_SLAVE,64,TTY_INVALID);
    /* Exercise every reserved SET slot and every unknown flag bit, rather
       than relying on one representative malformed word. */
    for(i=4;i<12;++i) {
        c=request(XTY_SET_CONFIG);c.values[3]=4;c.values[i]=i<10?257:1;
        rejected(&c,TTY_SLAVE,64,TTY_INVALID);
        c=request(XTY_SET_GEOMETRY);c.values[0]=24;c.values[1]=80;c.values[i]=1;
        rejected(&c,TTY_MASTER,64,TTY_INVALID);
    }
    for(i=0;i<32;++i) {
        uint32_t bit=UINT32_C(1)<<i;
        c=request(XTY_SET_CONFIG);c.values[3]=4;
        if(i>=3) { c.values[0]=bit;rejected(&c,TTY_SLAVE,64,TTY_UNSUPPORTED); }
        c.values[0]=0;
        if(i>=2) {
            c.values[1]=bit;rejected(&c,TTY_SLAVE,64,TTY_UNSUPPORTED);
            c.values[1]=0;
            if(i>=8) { c.values[2]=bit;rejected(&c,TTY_SLAVE,64,TTY_UNSUPPORTED); }
        }
    }
    c=request(XTY_SET_CONFIG);c.values[3]='\n';rejected(&c,TTY_SLAVE,64,TTY_INVALID);
    c=request(XTY_SET_GEOMETRY);c.values[1]=80;rejected(&c,TTY_SLAVE,64,TTY_INVALID);
    c.values[0]=24;c.values[1]=0;rejected(&c,TTY_SLAVE,64,TTY_INVALID);
    c.values[1]=80;c.values[2]=65536;rejected(&c,TTY_SLAVE,64,TTY_INVALID);
    c.values[2]=0;c.values[3]=65536;rejected(&c,TTY_SLAVE,64,TTY_INVALID);
    assert(tty_pair_write(&pair,TTY_MASTER,"a",1).count==1);
    c=request(XTY_SET_CONFIG);c.values[3]=4;
    assert(tty_control_apply(&pair,TTY_SLAVE,&c,sizeof c)==TTY_OK);
    assert(tty_pair_read(&pair,TTY_SLAVE,b,4).count==1 && b[0]=='a');
    assert(tty_pair_write(&pair,TTY_MASTER,"\004",1).count==1);
    assert(tty_pair_read(&pair,TTY_SLAVE,b,4).count==1 && b[0]==4);
    assert(tty_pair_release(&pair,TTY_SLAVE)==TTY_OK);
    c=request(XTY_GET_CONFIG);rejected(&c,TTY_SLAVE,64,TTY_CLOSED);
    puts("TTY CONTROL PASS: capabilities, config/geometry, unaligned block, strict validation, atomic rejection, preserved input/closed");
    return 0;
}
