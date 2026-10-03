#include "../src/tty_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static TtyService s;
static void expect(TtyResult r,TtyStatus status,size_t n)
{ assert(r.status==status && r.count==n); }
static TtyResult io(TtyHandle h,TtyOperation op,void *b,size_t n)
{ return tty_service_try_io(&s,h,op,b,n); }
static TtyRequest request(TtyOperation op,void *b,size_t n)
{
    TtyRequest r;memset(&r,0,sizeof r);
    r.handle=2;r.operation=op;r.buffer=b;r.length=n;return r;
}
static void no_retention(void)
{
    TtyRequest *r=(TtyRequest *)1;uint64_t token=1;
    assert(!tty_service_take(&s));
    assert(!tty_service_read_timer(&s,&r,&token) && !r && !token);
}
static void raw_timing(void)
{
    const unsigned char modes[4][2]={{0,0},{0,2},{3,0},{3,2}};
    unsigned i;char b[8];TtyConfig c;
    for(i=0;i<4;++i) {
        tty_service_init(&s);c=s.pair.config;c.vmin=modes[i][0];c.vtime=modes[i][1];
        assert(tty_pair_configure(&s.pair,&c)==TTY_OK);
        memset(b,'x',sizeof b);
        expect(io(2,TTY_REQUEST_READ,b,sizeof b),TTY_AGAIN,0);
        assert(!memcmp(b,"xxxxxxxx",8));no_retention();
        expect(io(1,TTY_REQUEST_WRITE,"A\0",2),TTY_OK,2);
        expect(io(2,TTY_REQUEST_READ,b,sizeof b),TTY_OK,2);
        assert(!memcmp(b,"A\0xxxxxx",8));no_retention();
    }
    puts("TTY TRY TIMING PASS: four modes, no timer, short binary input");
}
static void pressure(void)
{
    char fill[TTY_QUEUE_CAPACITY],out[TTY_QUEUE_CAPACITY];
    memset(fill,'q',sizeof fill);tty_service_init(&s);
    expect(io(2,TTY_REQUEST_WRITE,fill,sizeof fill-1),TTY_OK,sizeof fill-1);
    expect(io(2,TTY_REQUEST_WRITE,"yz",2),TTY_OK,1);
    expect(io(2,TTY_REQUEST_WRITE,"BAD",3),TTY_AGAIN,0);no_retention();
    expect(io(1,TTY_REQUEST_READ,out,sizeof out),TTY_OK,sizeof out);
    assert(!memcmp(out,fill,sizeof out-1) && out[sizeof out-1]=='y');
    expect(io(1,TTY_REQUEST_READ,out,1),TTY_AGAIN,0);
    expect(io(2,TTY_REQUEST_WRITE,"z",1),TTY_OK,1);
    expect(io(1,TTY_REQUEST_READ,out,1),TTY_OK,1);assert(out[0]=='z');
    no_retention();
    puts("TTY TRY PRESSURE PASS: short write, full ring, rejected bytes not retained");
}
static void fifo(void)
{
    char old[8],b[8];TtyConfig c;TtyRequest r;TtyHandle copy;
    tty_service_init(&s);c=s.pair.config;c.vmin=3;c.vtime=2;
    assert(tty_pair_configure(&s.pair,&c)==TTY_OK);
    assert(tty_service_duplicate(&s,2,&copy)==TTY_OK);
    r=request(TTY_REQUEST_READ,old,sizeof old);
    assert(tty_service_submit(&s,&r)==TTY_OK);
    expect(io(1,TTY_REQUEST_WRITE,"ab",2),TTY_OK,2);
    expect(io(copy,TTY_REQUEST_READ,b,sizeof b),TTY_AGAIN,0);
    expect(io(copy,TTY_REQUEST_READ,NULL,0),TTY_OK,0);
    expect(io(1,TTY_REQUEST_WRITE,"c",1),TTY_OK,1);
    /* Completed, uncollected requests no longer own the FIFO lane. */
    expect(io(1,TTY_REQUEST_WRITE,"d",1),TTY_OK,1);
    expect(io(copy,TTY_REQUEST_READ,b,sizeof b),TTY_OK,1);assert(b[0]=='d');
    assert(tty_service_take(&s)==&r);expect(r.result,TTY_OK,3);
    assert(!memcmp(old,"abc",3));
    expect(io(2,TTY_REQUEST_WRITE,"x",1),TTY_OK,1);
    r=request(TTY_REQUEST_DRAIN,NULL,0);assert(tty_service_submit(&s,&r)==TTY_OK);
    expect(io(copy,TTY_REQUEST_WRITE,"y",1),TTY_AGAIN,0);
    expect(io(copy,TTY_REQUEST_WRITE,NULL,0),TTY_OK,0);
    expect(io(1,TTY_REQUEST_READ,b,1),TTY_OK,1);assert(b[0]=='x');
    expect(io(copy,TTY_REQUEST_WRITE,"y",1),TTY_OK,1);
    assert(tty_service_take(&s)==&r);expect(r.result,TTY_OK,0);
    expect(io(1,TTY_REQUEST_READ,b,sizeof b),TTY_OK,1);assert(b[0]=='y');
    no_retention();
    puts("TTY TRY FIFO PASS: duplicate reader, VMIN head, drain barrier, zero I/O");
}
static void canonical_and_close(void)
{
    char b[8];TtyConfig c;
    tty_service_init(&s);c=s.pair.config;c.local_flags=TTY_CANON;
    assert(tty_pair_configure(&s.pair,&c)==TTY_OK);
    expect(io(1,TTY_REQUEST_WRITE,"a",1),TTY_OK,1);
    expect(io(2,TTY_REQUEST_READ,b,sizeof b),TTY_AGAIN,0);
    expect(io(1,TTY_REQUEST_WRITE,"\n",1),TTY_OK,1);
    expect(io(2,TTY_REQUEST_READ,b,sizeof b),TTY_OK,2);assert(!memcmp(b,"a\n",2));
    expect(io(1,TTY_REQUEST_WRITE,"\4",1),TTY_OK,1);
    expect(io(2,TTY_REQUEST_READ,b,sizeof b),TTY_EOF,0);
    expect(io(2,TTY_REQUEST_READ,b,sizeof b),TTY_AGAIN,0);
    expect(io(1,TTY_REQUEST_WRITE,"b\n",2),TTY_OK,2);
    expect(io(2,TTY_REQUEST_READ,b,sizeof b),TTY_OK,2);assert(!memcmp(b,"b\n",2));
    expect(io(2,TTY_REQUEST_WRITE,"out",3),TTY_OK,3);
    assert(tty_pair_output_flow(&s.pair,1)==TTY_OK);
    expect(io(1,TTY_REQUEST_READ,b,sizeof b),TTY_AGAIN,0);
    assert(tty_service_close(&s,2)==TTY_OK);
    expect(io(1,TTY_REQUEST_READ,b,sizeof b),TTY_OK,3);assert(!memcmp(b,"out",3));
    expect(io(1,TTY_REQUEST_READ,b,sizeof b),TTY_EOF,0);
    expect(io(1,TTY_REQUEST_WRITE,"x",1),TTY_CLOSED,0);
    expect(io(2,TTY_REQUEST_READ,NULL,0),TTY_CLOSED,0);
    assert(tty_service_close(&s,1)==TTY_OK && tty_service_retired(&s));
    puts("TTY TRY LIFETIME PASS: canonical boundary, temporary EOF, stopped output, hangup");
}
static void transformed_write_fifo(void)
{
    char fill[TTY_QUEUE_CAPACITY],out[TTY_QUEUE_CAPACITY];TtyConfig c;TtyRequest r;
    memset(fill,'q',sizeof fill);tty_service_init(&s);c=s.pair.config;
    c.output_flags=TTY_OPOST|TTY_ONLCR;
    assert(tty_pair_configure(&s.pair,&c)==TTY_OK);
    expect(io(2,TTY_REQUEST_WRITE,fill,sizeof fill-1),TTY_OK,sizeof fill-1);
    expect(io(2,TTY_REQUEST_WRITE,"\n",1),TTY_AGAIN,0);
    r=request(TTY_REQUEST_WRITE,"\n",1);assert(tty_service_submit(&s,&r)==TTY_OK);
    /* A byte could fit, but must not overtake the pending two-byte CRLF. */
    expect(io(2,TTY_REQUEST_WRITE,"x",1),TTY_AGAIN,0);
    expect(io(1,TTY_REQUEST_READ,out,1),TTY_OK,1);assert(out[0]=='q');
    assert(tty_service_take(&s)==&r);expect(r.result,TTY_OK,1);
    expect(io(1,TTY_REQUEST_READ,out,sizeof out),TTY_OK,sizeof out);
    assert(!memcmp(out,fill,sizeof out-2) && !memcmp(out+sizeof out-2,"\r\n",2));
    no_retention();
    puts("TTY TRY CRLF PASS: atomic expansion, pending writer ordering");
}
static void saturated_slots(void)
{
    TtyRequest requests[TTY_SERVICE_REQUESTS];unsigned i;char b='x';
    tty_service_init(&s);
    for(i=0;i<TTY_SERVICE_REQUESTS;++i) {
        requests[i]=request(i%2?TTY_REQUEST_WRITE:TTY_REQUEST_READ,NULL,0);
        requests[i].handle=i%4<2?1:2;
        assert(tty_service_submit(&s,&requests[i])==TTY_OK);
    }
    expect(io(1,TTY_REQUEST_WRITE,"a",1),TTY_OK,1);
    expect(io(2,TTY_REQUEST_READ,&b,1),TTY_OK,1);assert(b=='a');
    for(i=0;i<TTY_SERVICE_REQUESTS;++i) {
        assert(tty_service_take(&s)==&requests[i]);expect(requests[i].result,TTY_OK,0);
    }
    no_retention();
    puts("TTY TRY SLOTS PASS: completed-slot saturation does not prevent immediate I/O");
}
static void delayed_mode(void)
{
    char b[8];TtyConfig c;TtyRequest r;
    tty_service_init(&s);c=s.pair.config;c.local_flags=TTY_CANON;
    expect(io(2,TTY_REQUEST_WRITE,"x",1),TTY_OK,1);
    r=request(TTY_REQUEST_DRAIN_CONFIG,&c,sizeof c);
    assert(tty_service_submit(&s,&r)==TTY_OK);
    /* Simulate owner-side retirement before the next service call. */
    expect(tty_pair_read(&s.pair,TTY_MASTER,b,1),TTY_OK,1);
    expect(io(1,TTY_REQUEST_WRITE,"a",1),TTY_OK,1);
    assert(tty_service_take(&s)==&r);expect(r.result,TTY_OK,0);
    expect(io(2,TTY_REQUEST_READ,b,sizeof b),TTY_AGAIN,0);
    expect(io(1,TTY_REQUEST_WRITE,"\n",1),TTY_OK,1);
    expect(io(2,TTY_REQUEST_READ,b,sizeof b),TTY_OK,2);assert(!memcmp(b,"a\n",2));
    no_retention();
    puts("TTY TRY MODE PASS: older drain-config applies before immediate input");
}
static unsigned interrupts;
static void interrupt(void *context)
{ assert(context==&s && !s.pair.interrupt_pending);++interrupts; }
static void signals_and_errors(void)
{
    char b='x';TtyConfig c;
    tty_service_init(&s);c=s.pair.config;c.local_flags=TTY_ISIG|TTY_NOFLSH;c.intr=4;
    assert(tty_pair_configure(&s.pair,&c)==TTY_OK);
    expect(io(1,TTY_REQUEST_WRITE,"\3tail",5),TTY_OK,1);
    expect(io(2,TTY_REQUEST_WRITE,"out",3),TTY_AGAIN,0);
    expect(io(2,TTY_REQUEST_READ,NULL,0),TTY_OK,0);
    assert(s.pair.interrupt_pending);
    interrupts=0;s.interrupt=interrupt;s.interrupt_context=&s;
    expect(io(1,TTY_REQUEST_WRITE,"a",1),TTY_OK,1);assert(interrupts==1);
    expect(io(2,TTY_REQUEST_READ,&b,1),TTY_OK,1);assert(b=='a');
    expect(io(1,TTY_REQUEST_WRITE,"\3",1),TTY_OK,1);assert(interrupts==2);
    expect(io(0,TTY_REQUEST_READ,&b,1),TTY_CLOSED,0);
    expect(io(2,TTY_REQUEST_DRAIN,NULL,0),TTY_INVALID,0);
    expect(io(2,TTY_REQUEST_READ,NULL,1),TTY_INVALID,0);
    expect(tty_service_try_io(NULL,2,TTY_REQUEST_READ,&b,1),TTY_INVALID,0);
    no_retention();
    puts("TTY TRY SIGNAL PASS: ownerless event, dispatch before progress, validation");
}
int main(void)
{
    raw_timing();pressure();fifo();canonical_and_close();signals_and_errors();
    transformed_write_fifo();saturated_slots();delayed_mode();
    puts("TTY TRY IO PASS");return 0;
}
