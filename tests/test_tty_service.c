#include "../src/tty_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static TtyService service;
static TtyRequest request(TtyHandle h, TtyOperation op, void *b, size_t n)
{
    TtyRequest r;
    memset(&r, 0, sizeof r);
    r.handle=h;r.operation=op;r.buffer=b;r.length=n;
    return r;
}
static void completion(TtyRequest *r, TtyStatus status, size_t count, int cancelled)
{
    assert(tty_service_take(&service)==r);
    assert(r->result.status==status && r->result.count==count);
    assert(r->cancelled==cancelled);
}

/* A pending read cannot block its producer; full writes retain their buffer. */
static void wake_and_pressure(void)
{
    unsigned char b[TTY_QUEUE_CAPACITY], fill[TTY_QUEUE_CAPACITY];
    TtyRequest read,write,blocked,drain;
    memset(fill,'Q',sizeof fill);tty_service_init(&service);
    read=request(2,TTY_REQUEST_READ,b,sizeof b);
    write=request(1,TTY_REQUEST_WRITE,fill,3);
    assert(tty_service_submit(&service,&read)==TTY_OK);
    assert(!tty_service_take(&service));
    assert(tty_service_submit(&service,&write)==TTY_OK);
    /* Collection uses submission order among currently completed requests. */
    completion(&read,TTY_OK,3,0);assert(!memcmp(b,"QQQ",3));
    completion(&write,TTY_OK,3,0);
    write=request(2,TTY_REQUEST_WRITE,fill,sizeof fill);
    assert(tty_service_submit(&service,&write)==TTY_OK);
    completion(&write,TTY_OK,sizeof fill,0);
    blocked=request(2,TTY_REQUEST_WRITE,fill,3);
    assert(tty_service_submit(&service,&blocked)==TTY_OK);
    assert(!tty_service_take(&service));
    /* A slave write is not reported short: one byte of room queues one
       byte and keeps the request pending until the rest fits. */
    drain=request(1,TTY_REQUEST_READ,b,1);
    assert(tty_service_submit(&service,&drain)==TTY_OK);
    completion(&drain,TTY_OK,1,0);assert(!tty_service_take(&service));
    drain=request(1,TTY_REQUEST_READ,b,2);
    assert(tty_service_submit(&service,&drain)==TTY_OK);
    completion(&blocked,TTY_OK,3,0);completion(&drain,TTY_OK,2,0);
    /* Cancelled after a partial transfer: the queued prefix is reported. */
    {
        static unsigned char big[2*TTY_QUEUE_CAPACITY],sink[TTY_QUEUE_CAPACITY];
        TtyRequest partial=request(2,TTY_REQUEST_WRITE,big,sizeof big);
        memset(big,'P',sizeof big);
        drain=request(1,TTY_REQUEST_READ,sink,sizeof sink);
        assert(tty_service_submit(&service,&drain)==TTY_OK);
        completion(&drain,TTY_OK,sizeof sink,0);       /* queue empty now */
        assert(tty_service_submit(&service,&partial)==TTY_OK);
        assert(!tty_service_take(&service));            /* 1024 queued, rest waits */
        drain=request(1,TTY_REQUEST_READ,sink,512);
        assert(tty_service_submit(&service,&drain)==TTY_OK);
        completion(&drain,TTY_OK,512,0);
        assert(!tty_service_take(&service));            /* 1536 queued now */
        assert(tty_service_cancel(&service,&partial)==TTY_OK);
        completion(&partial,TTY_OK,TTY_QUEUE_CAPACITY+512,1);
        /* The sent prefix is not rolled back: the queue is still full, so
           the two-byte write below blocks. */
    }
    blocked=request(2,TTY_REQUEST_WRITE,fill+1,2);
    assert(tty_service_submit(&service,&blocked)==TTY_OK);
    assert(tty_service_cancel(&service,&blocked)==TTY_OK);
    assert(tty_service_cancel(&service,&blocked)==TTY_BUSY);
    completion(&blocked,TTY_CLOSED,0,1);
    assert(tty_service_close(&service,1)==TTY_OK);
    assert(tty_service_close(&service,2)==TTY_OK);
    assert(tty_service_retired(&service));
}

/* Cancel/close must complete once; stale IDs must not reach reused slots. */
static void handles_and_cancel(void)
{
    unsigned char b;
    TtyHandle duplicate=0,next=0;
    TtyRequest first,second,write;
    tty_service_init(&service);
    assert(tty_service_duplicate(&service,2,&duplicate)==TTY_OK);
    first=request(2,TTY_REQUEST_READ,&b,1);
    second=request(duplicate,TTY_REQUEST_READ,&b,1);
    assert(tty_service_submit(&service,&first)==TTY_OK);
    assert(tty_service_submit(&service,&second)==TTY_OK);
    assert(tty_service_submit(&service,&first)==TTY_BUSY);
    assert(tty_service_close(&service,2)==TTY_OK);
    completion(&first,TTY_CLOSED,0,1);
    assert(!tty_service_take(&service));
    assert(tty_service_duplicate(&service,duplicate,&next)==TTY_OK);
    assert(next!=2 && next!=duplicate);
    assert(tty_service_submit(&service,&first)==TTY_CLOSED);
    write=request(1,TTY_REQUEST_WRITE,&b,0);
    assert(tty_service_submit(&service,&write)==TTY_OK);
    completion(&write,TTY_OK,0,0);
    assert(tty_service_close(&service,1)==TTY_OK);
    completion(&second,TTY_EOF,0,0);
    assert(tty_service_close(&service,duplicate)==TTY_OK);
    assert(tty_service_close(&service,next)==TTY_OK);
    assert(tty_service_retired(&service));
}

/* Completed but uncollected packets still own slots; never overwrite them. */
static void slots_and_fifo(void)
{
    TtyRequest reads[TTY_SERVICE_REQUESTS],extra,write;
    unsigned char b[TTY_SERVICE_REQUESTS],data[]="ab";
    size_t i;
    tty_service_init(&service);
    for(i=0;i<TTY_SERVICE_REQUESTS;++i) {
        reads[i]=request(i%4<2?1:2,(TtyOperation)(i%2),&b[i],0);
        assert(tty_service_submit(&service,&reads[i])==TTY_OK);
    }
    extra=request(2,TTY_REQUEST_READ,b,1);
    assert(tty_service_submit(&service,&extra)==TTY_BUSY);
    for(i=0;i<TTY_SERVICE_REQUESTS;++i) completion(&reads[i],TTY_OK,0,0);
    reads[0]=request(2,TTY_REQUEST_READ,&b[0],1);
    reads[1]=request(2,TTY_REQUEST_READ,&b[1],1);
    assert(tty_service_submit(&service,&reads[0])==TTY_OK);
    assert(tty_service_submit(&service,&reads[1])==TTY_OK);
    write=request(1,TTY_REQUEST_WRITE,data,2);
    assert(tty_service_submit(&service,&write)==TTY_OK);
    completion(&reads[0],TTY_OK,1,0);completion(&reads[1],TTY_OK,1,0);
    completion(&write,TTY_OK,2,0);assert(b[0]=='a' && b[1]=='b');
    assert(tty_service_close(&service,1)==TTY_OK);
    assert(tty_service_close(&service,2)==TTY_OK);
    assert(tty_service_retired(&service));
}
/* Queued EOF is one completion, not destruction of the service or lease. */
static void eof_and_retirement(void)
{
    unsigned cycle;
    for(cycle=0;cycle<1000;++cycle) {
        TtyConfig config;
        unsigned char b[8],input[]={4,'x','\n'};
        TtyRequest first,second,write,pending;
        tty_service_init(&service);
        assert(tty_pair_get_config(&service.pair,TTY_PAIR_VERSION,&config)==TTY_OK);
        config.local_flags=TTY_CANON;
        assert(tty_pair_configure(&service.pair,&config)==TTY_OK);
        first=request(2,TTY_REQUEST_READ,b,sizeof b);
        second=request(2,TTY_REQUEST_READ,b,sizeof b);
        write=request(1,TTY_REQUEST_WRITE,input,sizeof input);
        assert(tty_service_submit(&service,&first)==TTY_OK);
        assert(tty_service_submit(&service,&second)==TTY_OK);
        assert(tty_service_submit(&service,&write)==TTY_OK);
        completion(&first,TTY_EOF,0,0);
        completion(&second,TTY_OK,2,0);assert(!memcmp(b,"x\n",2));
        completion(&write,TTY_OK,3,0);
        pending=request(2,TTY_REQUEST_READ,b,sizeof b);
        assert(tty_service_submit(&service,&pending)==TTY_OK);
        assert(tty_service_close(&service,2)==TTY_OK);
        assert(tty_service_close(&service,1)==TTY_OK);
        assert(!tty_service_retired(&service));
        completion(&pending,TTY_CLOSED,0,1);
        assert(!tty_service_take(&service));
        assert(tty_service_cancel(&service,&pending)==TTY_INVALID);
        assert(tty_service_retired(&service));
    }
    puts("TTY SERVICE LIFETIME PASS: cycles=1000 temporary EOF, cancellation, final collection");
}

/* Saturated consumers must leave admission space for the producer that wakes them. */
static void lane_admission(void)
{
    TtyRequest reads[8],extra,write;
    unsigned char b[8],input[]="abcdefgh";
    size_t i;
    tty_service_init(&service);
    for(i=0;i<8;++i) {
        reads[i]=request(2,TTY_REQUEST_READ,&b[i],1);
        assert(tty_service_submit(&service,&reads[i])==TTY_OK);
    }
    extra=request(2,TTY_REQUEST_READ,b,1);
    assert(tty_service_submit(&service,&extra)==TTY_BUSY);
    write=request(1,TTY_REQUEST_WRITE,input,8);
    assert(tty_service_submit(&service,&write)==TTY_OK);
    for(i=0;i<8;++i) completion(&reads[i],TTY_OK,1,0);
    completion(&write,TTY_OK,8,0);
    assert(!memcmp(b,input,8));
    assert(tty_service_close(&service,1)==TTY_OK);
    assert(tty_service_close(&service,2)==TTY_OK);
    assert(tty_service_retired(&service));
}
/* Per-request cancellation must not run a later write halfway through shutdown. */
static void atomic_cancel_all(void)
{
    unsigned char fill[TTY_QUEUE_CAPACITY],b[TTY_QUEUE_CAPACITY];
    char newline='\n',x='x';TtyConfig c;TtyRequest first,second;
    tty_service_init(&service);memset(fill,'Q',sizeof fill);
    assert(tty_pair_get_config(&service.pair,1,&c)==TTY_OK);
    c.output_flags=TTY_OPOST|TTY_ONLCR;
    assert(tty_pair_configure(&service.pair,&c)==TTY_OK);
    assert(tty_pair_write(&service.pair,TTY_SLAVE,fill,sizeof fill-1).count==sizeof fill-1);
    first=request(2,TTY_REQUEST_WRITE,&newline,1);
    second=request(2,TTY_REQUEST_WRITE,&x,1);
    assert(tty_service_submit(&service,&first)==TTY_OK);
    assert(tty_service_submit(&service,&second)==TTY_OK);
    assert(!tty_service_take(&service));
    assert(tty_service_cancel_all(&service)==2);
    completion(&first,TTY_CLOSED,0,1);completion(&second,TTY_CLOSED,0,1);
    assert(tty_pair_read(&service.pair,TTY_MASTER,b,sizeof b).count==sizeof fill-1);
    assert(!memcmp(b,fill,sizeof fill-1));
    assert(tty_service_close(&service,1)==TTY_OK);
    assert(tty_service_close(&service,2)==TTY_OK);
    assert(tty_service_retired(&service));
}
/* Zero-byte I/O must not wait behind a blocked operation in the same lane.
   It transfers nothing and must not consume input or bypass a nonempty FIFO. */
static void zero_length_behind_blocked(void)
{
    unsigned lane;unsigned char fill[TTY_QUEUE_CAPACITY],byte=0;
    memset(fill,'Z',sizeof fill);
    for(lane=0;lane<4;++lane) {
        TtyEnd end=(TtyEnd)(lane/2);
        TtyOperation op=(TtyOperation)(lane%2);
        TtyHandle h=end==TTY_MASTER?1:2;
        TtyRequest blocked,zero;
        tty_service_init(&service);
        if(op==TTY_REQUEST_WRITE)
            assert(tty_pair_write(&service.pair,end,fill,sizeof fill).count==sizeof fill);
        blocked=request(h,op,&byte,1);
        zero=request(h,op,NULL,0);
        assert(tty_service_submit(&service,&blocked)==TTY_OK);
        assert(!tty_service_take(&service));
        assert(tty_service_submit(&service,&zero)==TTY_OK);
        completion(&zero,TTY_OK,0,0);
        assert(!tty_service_take(&service));
        assert(blocked.result.status==TTY_AGAIN);
        if(op==TTY_REQUEST_WRITE) {
            unsigned char b[TTY_QUEUE_CAPACITY];
            assert(tty_pair_read(&service.pair,(TtyEnd)(1-end),b,sizeof b).count==sizeof b);
            assert(!memcmp(b,fill,sizeof b));
        }
        /* Closing the exact owner cancels the original request, not the
           already collected zero-byte request. */
        assert(tty_service_close(&service,h)==TTY_OK);
        completion(&blocked,TTY_CLOSED,0,1);
        assert(tty_service_close(&service,h==1?2:1)==TTY_OK);
        assert(tty_service_retired(&service));
    }
    puts("TTY ZERO IO PASS: four blocked lanes, immediate empty completion, unchanged payload, close");
}
/* An empty write may pass a blocked CRLF, but must not let a later byte
   use the one remaining slot and overtake that CRLF. */
static void zero_length_preserves_fifo(void)
{
    unsigned char fill[TTY_QUEUE_CAPACITY-1],out[TTY_QUEUE_CAPACITY+2];
    char newline='\n',last='x';TtyConfig config;
    TtyRequest first,zero,later,read;size_t i;
    tty_service_init(&service);memset(fill,'Q',sizeof fill);
    assert(tty_pair_get_config(&service.pair,1,&config)==TTY_OK);
    config.output_flags=TTY_OPOST|TTY_ONLCR;
    assert(tty_pair_configure(&service.pair,&config)==TTY_OK);
    assert(tty_pair_write(&service.pair,TTY_SLAVE,fill,sizeof fill).count==sizeof fill);
    first=request(2,TTY_REQUEST_WRITE,&newline,1);
    zero=request(2,TTY_REQUEST_WRITE,NULL,0);
    later=request(2,TTY_REQUEST_WRITE,&last,1);
    assert(tty_service_submit(&service,&first)==TTY_OK);
    assert(tty_service_submit(&service,&zero)==TTY_OK);
    completion(&zero,TTY_OK,0,0);
    assert(tty_service_submit(&service,&later)==TTY_OK);
    assert(!tty_service_take(&service));
    read=request(1,TTY_REQUEST_READ,out,1);
    assert(tty_service_submit(&service,&read)==TTY_OK);
    completion(&first,TTY_OK,1,0);completion(&read,TTY_OK,1,0);
    assert(!tty_service_take(&service));
    read=request(1,TTY_REQUEST_READ,out+1,2);
    assert(tty_service_submit(&service,&read)==TTY_OK);
    completion(&later,TTY_OK,1,0);completion(&read,TTY_OK,2,0);
    read=request(1,TTY_REQUEST_READ,out+3,sizeof out-3);
    assert(tty_service_submit(&service,&read)==TTY_OK);
    completion(&read,TTY_OK,sizeof out-3,0);
    for(i=0;i<sizeof fill;++i) assert(out[i]=='Q');
    assert(!memcmp(out+sizeof fill,"\r\nx",3));
    assert(!tty_service_take(&service));
    assert(tty_service_close(&service,1)==TTY_OK);
    assert(tty_service_close(&service,2)==TTY_OK);
    assert(tty_service_retired(&service));
    puts("TTY ZERO FIFO PASS: blocked CRLF, empty write, later byte, exact ordered output");
}
/* Binary duplex traffic through the request owner, not direct pair reads.
   Missing wakeups, offset errors after short writes, cross-stream delivery or
   double completion must fail before the bounded event budget expires. */
static unsigned char stream_byte(unsigned stream,size_t offset)
{ return (unsigned char)(offset*37u+(offset>>8)*19u+stream*113u); }
static uint32_t schedule_next(uint32_t *state)
{
    *state^=*state<<13;*state^=*state>>17;*state^=*state<<5;
    return *state;
}
static void duplex_pressure(void)
{
    enum { BYTES=131071, ROUNDS=8, BUDGET=200000 };
    static unsigned char buffers[4][1537];
    unsigned round;size_t total_short=0,total_events=0;
    for(round=0;round<ROUNDS;++round) {
        TtyRequest requests[4];int live[4]={0,0,0,0};
        size_t offsets[4]={0,0,0,0},events,short_writes=0;
        uint32_t random=UINT32_C(0x91e10da5)+round;
        tty_service_init(&service);
        for(events=0;events<BUDGET;++events) {
            uint32_t draw=schedule_next(&random);
            unsigned lane=(draw>>16)&3u;
            /* Deliberately stop submitting readers for most of each window.
               Completed requests can also remain uncollected below. */
            if(!live[lane] && offsets[lane]<BYTES &&
               (lane<2 || events%128>=96)) {
                size_t i,n=1+(draw%(lane<2?1537u:257u));
                if(n>BYTES-offsets[lane]) n=BYTES-offsets[lane];
                if(lane<2) for(i=0;i<n;++i)
                    buffers[lane][i]=stream_byte(lane,offsets[lane]+i);
                else memset(buffers[lane],0xa5,sizeof buffers[lane]);
                requests[lane]=request(lane==0 || lane==3?1:2,
                    lane<2?TTY_REQUEST_WRITE:TTY_REQUEST_READ,buffers[lane],n);
                assert(tty_service_submit(&service,&requests[lane])==TTY_OK);
                live[lane]=1;
            }
            if((draw&3u)==0) {
                TtyRequest *r=tty_service_take(&service);
                if(r) {
                    size_t i;unsigned found;
                    for(found=0;found<4 && r!=&requests[found];++found) {}
                    assert(found<4 && live[found]);
                    assert(!r->cancelled && r->result.status==TTY_OK);
                    assert(r->result.count>0 && r->result.count<=r->length);
                    if(found<2) short_writes+=r->result.count<r->length;
                    else for(i=0;i<r->result.count;++i)
                        assert(buffers[found][i]==stream_byte(found-2,offsets[found]+i));
                    offsets[found]+=r->result.count;live[found]=0;
                }
            }
            if(offsets[0]==BYTES && offsets[1]==BYTES &&
               offsets[2]==BYTES && offsets[3]==BYTES) break;
        }
        assert(events<BUDGET && short_writes>0);
        for(unsigned lane=0;lane<4;++lane) assert(!live[lane]);
        assert(!tty_service_take(&service));
        assert(tty_service_close(&service,1)==TTY_OK);
        assert(tty_service_close(&service,2)==TTY_OK);
        assert(tty_service_retired(&service));
        total_short+=short_writes;total_events+=events+1;
    }
    printf("TTY DUPLEX PRESSURE PASS: schedules=%u bytes_per_stream=%lu short_writes=%lu events=%lu\n",
           ROUNDS,(unsigned long)BYTES*ROUNDS,(unsigned long)total_short,(unsigned long)total_events);
}
/* Flush only accepted bytes: pending writes may resume, pending readers keep
   their ownership, and completed read buffers must never be retracted. */
static void flush_pressure(void)
{
    unsigned direction;unsigned char full[TTY_QUEUE_CAPACITY],b[8];
    TtyRequest fill,write,read;TtyPair saved;
    memset(full,'f',sizeof full);
    for(direction=0;direction<2;++direction) {
        TtyHandle writer=direction?2:1,reader=direction?1:2;
        tty_service_init(&service);
        fill=request(writer,TTY_REQUEST_WRITE,full,sizeof full);
        assert(tty_service_submit(&service,&fill)==TTY_OK);
        completion(&fill,TTY_OK,sizeof full,0);
        write=request(writer,TTY_REQUEST_WRITE,(void *)"new",3);
        assert(tty_service_submit(&service,&write)==TTY_OK);
        assert(!tty_service_take(&service));
        saved=service.pair;
        assert(tty_pair_flush(&service.pair,0)==TTY_INVALID);
        assert(!memcmp(&service.pair,&saved,sizeof saved));
        assert(tty_pair_flush(&service.pair,direction?TTY_FLUSH_OUTPUT:TTY_FLUSH_INPUT)==TTY_OK);
        tty_service_pump(&service);completion(&write,TTY_OK,3,0);
        read=request(reader,TTY_REQUEST_READ,b,sizeof b);
        assert(tty_service_submit(&service,&read)==TTY_OK);
        /* A flush cannot undo bytes already copied into an uncollected read. */
        assert(tty_pair_flush(&service.pair,3)==TTY_OK);
        completion(&read,TTY_OK,3,0);assert(!memcmp(b,"new",3));
        read=request(reader,TTY_REQUEST_READ,b,sizeof b);
        assert(tty_service_submit(&service,&read)==TTY_OK);
        assert(tty_pair_flush(&service.pair,3)==TTY_OK);
        tty_service_pump(&service);assert(!tty_service_take(&service));
        write=request(writer,TTY_REQUEST_WRITE,(void *)"next",4);
        assert(tty_service_submit(&service,&write)==TTY_OK);
        completion(&read,TTY_OK,4,0);assert(!memcmp(b,"next",4));
        completion(&write,TTY_OK,4,0);
        assert(tty_service_close(&service,1)==TTY_OK);
        assert(tty_service_close(&service,2)==TTY_OK);
        assert(tty_service_retired(&service));
    }
    puts("TTY FLUSH PRESSURE PASS: directions=2 resumed writes, retained reads, completed buffers, retirement");
}
/* A blocked writer must use the new configuration only after old input is
   discarded; invalid config cannot first clear old records or partial input. */
static void configure_discard_pressure(void)
{
    unsigned char full[TTY_QUEUE_CAPACITY],b[TTY_QUEUE_CAPACITY];
    TtyConfig config;TtyPair saved;TtyRequest blocked,read;
    tty_service_init(&service);config=service.pair.config;config.local_flags=TTY_CANON|TTY_ECHO;
    assert(tty_pair_configure(&service.pair,&config)==TTY_OK);
    assert(tty_pair_write(&service.pair,TTY_MASTER,"old\n\004tail",9).count==9);
    assert(tty_pair_read(&service.pair,TTY_MASTER,b,sizeof b).count==8);
    memset(full,'f',sizeof full);
    assert(tty_pair_write(&service.pair,TTY_SLAVE,full,sizeof full).count==sizeof full);
    blocked=request(1,TTY_REQUEST_WRITE,(void *)"\n\004",2);
    assert(tty_service_submit(&service,&blocked)==TTY_OK);assert(!tty_service_take(&service));
    saved=service.pair;config.version=99;
    assert(tty_pair_configure_discard_input(&service.pair,&config)==TTY_UNSUPPORTED);
    assert(!memcmp(&service.pair,&saved,sizeof saved));
    config.version=1;config.local_flags=0;config.input_flags=TTY_INLCR;
    assert(tty_pair_configure_discard_input(&service.pair,&config)==TTY_OK);
    tty_service_pump(&service);completion(&blocked,TTY_OK,2,0);
    read=request(2,TTY_REQUEST_READ,b,sizeof b);
    assert(tty_service_submit(&service,&read)==TTY_OK);completion(&read,TTY_OK,2,0);
    assert(!memcmp(b,"\r\004",2));
    read=request(1,TTY_REQUEST_READ,b,sizeof b);
    assert(tty_service_submit(&service,&read)==TTY_OK);completion(&read,TTY_OK,sizeof b,0);
    assert(!memcmp(b,full,sizeof b));
    assert(tty_service_close(&service,1)==TTY_OK && tty_service_close(&service,2)==TTY_OK);
    assert(tty_service_retired(&service));
    puts("TTY CONFIG DISCARD PRESSURE PASS: rejected config preserves input, pending writer uses new mode, output retained");
}
/* A drain must not be an empty write: it waits for earlier output, prevents
   later writes overtaking it, and does not stop the opposing read lane. */
static void drain_order(void)
{
    TtyRequest write,drain,later,read,empty;unsigned char b[8];
    tty_service_init(&service);
    write=request(2,TTY_REQUEST_WRITE,(void *)"abc",3);
    assert(tty_service_submit(&service,&write)==TTY_OK);completion(&write,TTY_OK,3,0);
    drain=request(2,(TtyOperation)2,NULL,0);
    assert(tty_service_submit(&service,&drain)==TTY_OK);assert(!tty_service_take(&service));
    later=request(2,TTY_REQUEST_WRITE,(void *)"z",1);
    assert(tty_service_submit(&service,&later)==TTY_OK);assert(!tty_service_take(&service));
    empty=request(2,TTY_REQUEST_WRITE,NULL,0);
    assert(tty_service_submit(&service,&empty)==TTY_OK);completion(&empty,TTY_OK,0,0);
    read=request(1,TTY_REQUEST_READ,b,1);
    assert(tty_service_submit(&service,&read)==TTY_OK);completion(&read,TTY_OK,1,0);
    assert(b[0]=='a' && !tty_service_take(&service));
    read=request(1,TTY_REQUEST_READ,b,sizeof b);
    assert(tty_service_submit(&service,&read)==TTY_OK);
    completion(&drain,TTY_OK,0,0);completion(&later,TTY_OK,1,0);completion(&read,TTY_OK,2,0);
    assert(!memcmp(b,"bc",2));
    read=request(1,TTY_REQUEST_READ,b,sizeof b);
    assert(tty_service_submit(&service,&read)==TTY_OK);completion(&read,TTY_OK,1,0);assert(b[0]=='z');
    drain=request(2,(TtyOperation)2,NULL,0);
    assert(tty_service_submit(&service,&drain)==TTY_OK);completion(&drain,TTY_OK,0,0);
    assert(tty_service_close(&service,1)==TTY_OK && tty_service_close(&service,2)==TTY_OK);
    assert(tty_service_retired(&service));
    puts("TTY DRAIN ORDER PASS: partial reads, write barrier, empty I/O, final retirement");
}
/* Retire only captured output, including translated CRLF and counter wrap;
   echo arriving after the drain starts cannot keep that barrier pending. */
static void drain_boundary(void)
{
    unsigned char b[TTY_QUEUE_CAPACITY],full[TTY_QUEUE_CAPACITY];TtyConfig c;
    TtyRequest drain,write,read,older;
    tty_service_init(&service);c=service.pair.config;c.local_flags=TTY_ECHO;
    c.output_flags=TTY_OPOST|TTY_ONLCR;
    assert(tty_pair_configure(&service.pair,&c)==TTY_OK);
    service.pair.output_retired=UINT64_MAX-1;
    write=request(2,TTY_REQUEST_WRITE,(void *)"\n",1);
    assert(tty_service_submit(&service,&write)==TTY_OK);completion(&write,TTY_OK,1,0);
    drain=request(2,TTY_REQUEST_DRAIN,NULL,0);
    assert(tty_service_submit(&service,&drain)==TTY_OK);assert(!tty_service_take(&service));
    write=request(1,TTY_REQUEST_WRITE,(void *)"e",1);
    assert(tty_service_submit(&service,&write)==TTY_OK);completion(&write,TTY_OK,1,0);
    read=request(1,TTY_REQUEST_READ,b,1);
    assert(tty_service_submit(&service,&read)==TTY_OK);completion(&read,TTY_OK,1,0);assert(b[0]=='\r');
    assert(!tty_service_take(&service));
    read=request(1,TTY_REQUEST_READ,b,1);
    assert(tty_service_submit(&service,&read)==TTY_OK);completion(&drain,TTY_OK,0,0);
    completion(&read,TTY_OK,1,0);assert(b[0]=='\n');
    assert(tty_pair_read(&service.pair,TTY_MASTER,b,sizeof b).count==1 && b[0]=='e');
    assert(tty_pair_read(&service.pair,TTY_SLAVE,b,sizeof b).count==1 && b[0]=='e');
    /* Drain must not capture a target before an older blocked write runs. */
    tty_service_init(&service);memset(full,'f',sizeof full);
    assert(tty_pair_write(&service.pair,TTY_SLAVE,full,sizeof full).count==sizeof full);
    older=request(2,TTY_REQUEST_WRITE,(void *)"o",1);
    assert(tty_service_submit(&service,&older)==TTY_OK);
    drain=request(2,TTY_REQUEST_DRAIN,NULL,0);assert(tty_service_submit(&service,&drain)==TTY_OK);
    read=request(1,TTY_REQUEST_READ,b,sizeof b);
    assert(tty_service_submit(&service,&read)==TTY_OK);completion(&older,TTY_OK,1,0);
    completion(&read,TTY_OK,sizeof b,0);assert(!memcmp(b,full,sizeof b));
    assert(!tty_service_take(&service));
    assert(tty_pair_flush(&service.pair,TTY_FLUSH_OUTPUT)==TTY_OK);
    tty_service_pump(&service);completion(&drain,TTY_OK,0,0);
    puts("TTY DRAIN BOUNDARY PASS: CRLF, counter wrap, later echo, older blocked write, output flush");
}
static void drain_lifetime(void)
{
    TtyRequest drain,later,requests[TTY_SERVICE_LANE_REQUESTS],extra;unsigned i;
    TtyHandle duplicate;unsigned char b[8];
    tty_service_init(&service);
    drain=request(1,TTY_REQUEST_DRAIN,NULL,0);
    assert(tty_service_submit(&service,&drain)==TTY_UNSUPPORTED);
    drain=request(2,TTY_REQUEST_DRAIN,b,0);assert(tty_service_submit(&service,&drain)==TTY_INVALID);
    drain=request(2,TTY_REQUEST_DRAIN,NULL,1);assert(tty_service_submit(&service,&drain)==TTY_INVALID);
    assert(tty_pair_write(&service.pair,TTY_SLAVE,"a",1).count==1);
    drain=request(2,TTY_REQUEST_DRAIN,NULL,0);assert(tty_service_submit(&service,&drain)==TTY_OK);
    later=request(2,TTY_REQUEST_WRITE,(void *)"b",1);assert(tty_service_submit(&service,&later)==TTY_OK);
    assert(tty_service_cancel(&service,&drain)==TTY_OK);
    completion(&drain,TTY_CLOSED,0,1);completion(&later,TTY_OK,1,0);
    assert(tty_pair_read(&service.pair,TTY_MASTER,b,sizeof b).count==2 && !memcmp(b,"ab",2));
    assert(tty_service_duplicate(&service,2,&duplicate)==TTY_OK);
    assert(tty_pair_write(&service.pair,TTY_SLAVE,"c",1).count==1);
    drain=request(2,TTY_REQUEST_DRAIN,NULL,0);assert(tty_service_submit(&service,&drain)==TTY_OK);
    assert(tty_service_close(&service,2)==TTY_OK);completion(&drain,TTY_CLOSED,0,1);
    drain=request(duplicate,TTY_REQUEST_DRAIN,NULL,0);assert(tty_service_submit(&service,&drain)==TTY_OK);
    assert(tty_service_close(&service,1)==TTY_OK);completion(&drain,TTY_CLOSED,0,0);
    assert(tty_service_close(&service,duplicate)==TTY_OK);assert(tty_service_retired(&service));
    /* Drains share the write quota; they cannot exhaust the opposing reader. */
    tty_service_init(&service);assert(tty_pair_write(&service.pair,TTY_SLAVE,"x",1).count==1);
    for(i=0;i<TTY_SERVICE_LANE_REQUESTS;++i) {
        requests[i]=request(2,TTY_REQUEST_DRAIN,NULL,0);
        assert(tty_service_submit(&service,&requests[i])==TTY_OK);
    }
    extra=request(2,TTY_REQUEST_WRITE,(void *)"z",1);assert(tty_service_submit(&service,&extra)==TTY_BUSY);
    extra=request(1,TTY_REQUEST_READ,b,1);assert(tty_service_submit(&service,&extra)==TTY_OK);
    for(i=0;i<TTY_SERVICE_LANE_REQUESTS;++i) completion(&requests[i],TTY_OK,0,0);
    completion(&extra,TTY_OK,1,0);assert(b[0]=='x');
    assert(tty_service_close(&service,1)==TTY_OK && tty_service_close(&service,2)==TTY_OK);
    assert(tty_service_retired(&service));
    puts("TTY DRAIN LIFETIME PASS: validation, cancel, duplicate close, peer close, bounded admission");
}
/* A combined barrier must change mode before releasing later writes, never
   discard input on cancellation, and reject bad config before admission. */
static void drain_config(void)
{
    unsigned discard;unsigned char b[16];TtyConfig c;TtyRequest change,write,read;
    for(discard=0;discard<2;++discard) {
        tty_service_init(&service);c=service.pair.config;
        c.output_flags=TTY_OPOST|TTY_ONLCR;
        assert(tty_pair_write(&service.pair,TTY_SLAVE,"old",3).count==3);
        change=request(2,(TtyOperation)(3+discard),&c,sizeof c);
        assert(tty_service_submit(&service,&change)==TTY_OK);
        write=request(2,TTY_REQUEST_WRITE,(void *)"\n",1);
        assert(tty_service_submit(&service,&write)==TTY_OK);
        assert(!tty_service_take(&service));assert(service.pair.config.output_flags==0);
        if(discard) assert(tty_pair_write(&service.pair,TTY_MASTER,"stale",5).count==5);
        read=request(1,TTY_REQUEST_READ,b,3);assert(tty_service_submit(&service,&read)==TTY_OK);
        completion(&change,TTY_OK,0,0);completion(&write,TTY_OK,1,0);completion(&read,TTY_OK,3,0);
        assert(!memcmp(b,"old",3));assert(!service.pair.input.count);
        assert(tty_pair_read(&service.pair,TTY_MASTER,b,sizeof b).count==2 && !memcmp(b,"\r\n",2));
        assert(tty_service_close(&service,1)==TTY_OK && tty_service_close(&service,2)==TTY_OK);
        assert(tty_service_retired(&service));
    }
    tty_service_init(&service);c=service.pair.config;c.output_flags=3;
    assert(tty_pair_write(&service.pair,TTY_MASTER,"keep",4).count==4);
    assert(tty_pair_write(&service.pair,TTY_SLAVE,"old",3).count==3);
    change=request(2,(TtyOperation)4,&c,sizeof c);
    c.eof='\n';assert(tty_service_submit(&service,&change)==TTY_INVALID);
    assert(!tty_service_take(&service));assert(service.pair.input.count==4);
    c.eof=4;assert(tty_service_submit(&service,&change)==TTY_OK);
    assert(tty_service_cancel(&service,&change)==TTY_OK);completion(&change,TTY_CLOSED,0,1);
    assert(service.pair.input.count==4 && service.pair.config.output_flags==0);
    /* Non-discarding change preserves accepted input while releasing the next
       writer under the new output mapping. */
    change=request(2,(TtyOperation)3,&c,sizeof c);assert(tty_service_submit(&service,&change)==TTY_OK);
    write=request(2,TTY_REQUEST_WRITE,(void *)"\n",1);assert(tty_service_submit(&service,&write)==TTY_OK);
    assert(tty_pair_flush(&service.pair,TTY_FLUSH_OUTPUT)==TTY_OK);tty_service_pump(&service);
    completion(&change,TTY_OK,0,0);completion(&write,TTY_OK,1,0);assert(service.pair.input.count==4);
    assert(tty_pair_read(&service.pair,TTY_MASTER,b,sizeof b).count==2 && !memcmp(b,"\r\n",2));
    assert(service.pair.config.output_flags==3);
    assert(tty_pair_read(&service.pair,TTY_SLAVE,b,sizeof b).count==4 && !memcmp(b,"keep",4));
    assert(tty_service_close(&service,1)==TTY_OK && tty_service_close(&service,2)==TTY_OK);
    assert(tty_service_retired(&service));
    for(discard=0;discard<2;++discard) {
        tty_service_init(&service);c=service.pair.config;c.local_flags=TTY_CANON;
        assert(tty_pair_configure(&service.pair,&c)==TTY_OK);
        assert(tty_pair_write(&service.pair,TTY_MASTER,"tail",4).count==4);
        assert(tty_pair_write(&service.pair,TTY_SLAVE,"old",3).count==3);
        c.local_flags=0;change=request(2,(TtyOperation)(3+discard),&c,sizeof c);
        assert(tty_service_submit(&service,&change)==TTY_OK);
        assert(service.pair.line_count==4 && service.pair.config.local_flags==TTY_CANON);
        assert(tty_pair_read(&service.pair,TTY_MASTER,b,sizeof b).count==3);tty_service_pump(&service);
        completion(&change,TTY_OK,0,0);
        assert(!service.pair.line_count && !service.pair.config.local_flags);
        assert(service.pair.input.count==(discard?0:4));
        if(!discard) assert(tty_pair_read(&service.pair,TTY_SLAVE,b,sizeof b).count==4 && !memcmp(b,"tail",4));
        assert(tty_service_close(&service,1)==TTY_OK && tty_service_close(&service,2)==TTY_OK);
        assert(tty_service_retired(&service));
    }
    puts("TTY DRAIN CONFIG PASS: ordered mode transition, delayed discard, validation, cancellation, preserved input, canonical tail");
}
static void output_flow(void)
{
    unsigned char fill[TTY_QUEUE_CAPACITY],out[TTY_QUEUE_CAPACITY];TtyRequest read,write,drain;
    memset(fill,'q',sizeof fill);tty_service_init(&service);
    assert(tty_pair_write(&service.pair,TTY_SLAVE,fill,sizeof fill).count==sizeof fill);
    assert(tty_pair_output_flow(&service.pair,1)==TTY_OK);
    read=request(1,TTY_REQUEST_READ,out,sizeof out);write=request(2,TTY_REQUEST_WRITE,"z",1);
    drain=request(2,TTY_REQUEST_DRAIN,NULL,0);
    assert(tty_service_submit(&service,&read)==TTY_OK && tty_service_submit(&service,&write)==TTY_OK);
    assert(tty_service_submit(&service,&drain)==TTY_OK && !tty_service_take(&service));
    assert(tty_pair_read(&service.pair,TTY_MASTER,NULL,0).status==TTY_OK);
    assert(tty_pair_write(&service.pair,TTY_MASTER,"input",5).count==5);
    assert(tty_pair_output_flow(&service.pair,0)==TTY_OK);tty_service_pump(&service);
    completion(&read,TTY_OK,sizeof out,0);assert(!memcmp(out,fill,sizeof out));
    completion(&write,TTY_OK,1,0);assert(!tty_service_take(&service));
    assert(tty_pair_read(&service.pair,TTY_MASTER,out,sizeof out).count==1 && out[0]=='z');
    tty_service_pump(&service);completion(&drain,TTY_OK,0,0);
    assert(tty_pair_read(&service.pair,TTY_SLAVE,out,sizeof out).count==5 && !memcmp(out,"input",5));
    assert(tty_pair_output_flow(&service.pair,1)==TTY_OK);
    assert(tty_pair_write(&service.pair,TTY_SLAVE,"end",3).count==3);
    assert(tty_service_close(&service,2)==TTY_OK);
    assert(tty_pair_read(&service.pair,TTY_MASTER,out,sizeof out).count==3 && !memcmp(out,"end",3));
    assert(tty_pair_read(&service.pair,TTY_MASTER,out,sizeof out).status==TTY_EOF);
    puts("TTY OUTPUT FLOW PASS: full queue, held reader/writer/drain, input progress, zero I/O, exact restart, hangup");
}
static void read_timing(void)
{
    TtyConfig c;TtyRequest r,next,*timed;uint64_t token,old;unsigned char b[8],b2[8];unsigned mode;
    for(mode=0;mode<4;++mode) {
        tty_service_init(&service);c=service.pair.config;c.vmin=(mode&1)?3:0;c.vtime=(mode&2)?2:0;
        assert(tty_pair_configure(&service.pair,&c)==TTY_OK);
        r=request(2,TTY_REQUEST_READ,b,sizeof b);assert(tty_service_submit(&service,&r)==TTY_OK);
        if(!mode) { completion(&r,TTY_OK,0,0);continue; }
        assert(!tty_service_take(&service));
        assert(tty_service_read_timer(&service,&timed,&token)==(mode==2?2:0));
        if(mode==2) {
            tty_service_read_timeout(&service,timed,token);completion(&r,TTY_OK,0,0);
            assert(tty_pair_read_status(&service.pair,TTY_SLAVE)==TTY_AGAIN);continue;
        }
        assert(tty_pair_write(&service.pair,TTY_MASTER,"a",1).count==1);tty_service_pump(&service);
        assert(!tty_service_take(&service));
        if(mode==3) {
            assert(tty_service_read_timer(&service,&timed,&old)==2 && timed==&r);
            assert(tty_pair_write(&service.pair,TTY_MASTER,"b",1).count==1);tty_service_pump(&service);
            assert(tty_service_read_timer(&service,&timed,&token)==2 && token!=old);
            tty_service_read_timeout(&service,&r,old);assert(!tty_service_take(&service));
            tty_service_read_timeout(&service,&r,token);completion(&r,TTY_OK,2,0);assert(!memcmp(b,"ab",2));
        } else {
            assert(tty_pair_write(&service.pair,TTY_MASTER,"bc",2).count==2);tty_service_pump(&service);
            completion(&r,TTY_OK,3,0);assert(!memcmp(b,"abc",3));
        }
    }
    tty_service_init(&service);c=service.pair.config;c.vmin=0;c.vtime=1;
    assert(tty_pair_configure(&service.pair,&c)==TTY_OK);
    r=request(2,TTY_REQUEST_READ,b,sizeof b);next=request(2,TTY_REQUEST_READ,b2,sizeof b2);
    assert(tty_service_submit(&service,&r)==TTY_OK && tty_service_submit(&service,&next)==TTY_OK);
    assert(tty_service_read_timer(&service,&timed,&old)==1 && timed==&r);
    assert(tty_service_cancel(&service,&r)==TTY_OK);completion(&r,TTY_CLOSED,0,1);
    assert(tty_service_read_timer(&service,&timed,&token)==1 && timed==&next && token!=old);
    tty_service_read_timeout(&service,&r,old);assert(!tty_service_take(&service));
    c.local_flags=TTY_CANON;assert(tty_pair_configure(&service.pair,&c)==TTY_OK);tty_service_pump(&service);
    tty_service_read_timeout(&service,&next,token);assert(!tty_service_take(&service));
    assert(!tty_service_read_timer(&service,&timed,&token));
    assert(tty_pair_write(&service.pair,TTY_MASTER,"x\n",2).count==2);tty_service_pump(&service);
    completion(&next,TTY_OK,2,0);assert(!memcmp(b2,"x\n",2));
    c.local_flags=0;c.vmin=7;assert(tty_pair_configure(&service.pair,&c)==TTY_OK);
    r=request(2,TTY_REQUEST_READ,b,2);assert(tty_service_submit(&service,&r)==TTY_OK);
    assert(tty_pair_write(&service.pair,TTY_MASTER,"xy",2).count==2);tty_service_pump(&service);
    completion(&r,TTY_OK,2,0);assert(!memcmp(b,"xy",2));
    r=request(2,TTY_REQUEST_READ,b,sizeof b);assert(tty_service_submit(&service,&r)==TTY_OK);
    assert(tty_pair_write(&service.pair,TTY_MASTER,"z",1).count==1);tty_service_pump(&service);
    assert(tty_service_close(&service,1)==TTY_OK);completion(&r,TTY_OK,1,0);assert(b[0]=='z');
    puts("TTY READ TIMING PASS: four cases, interbyte rearm, stale expiry, FIFO/cancel, mode change, short buffer, hangup");
}
static unsigned interrupt_calls;
static void observe_interrupt(void *context)
{
    TtyRequest *blocked=context;
    assert(blocked->result.status==TTY_AGAIN);
    assert(!service.pair.interrupt_pending);
    assert(!service.pair.output.count);
    ++interrupt_calls;
}
/* A signal-induced flush must not release a blocked writer before dispatch. */
static void interrupt_order(void)
{
    unsigned char full[TTY_QUEUE_CAPACITY];TtyRequest blocked,trigger;TtyConfig c;
    memset(full,'q',sizeof full);tty_service_init(&service);c=service.pair.config;
    c.local_flags=TTY_ISIG;c.intr=4;assert(tty_pair_configure(&service.pair,&c)==TTY_OK);
    assert(tty_pair_write(&service.pair,TTY_SLAVE,full,sizeof full).count==sizeof full);
    blocked=request(2,TTY_REQUEST_WRITE,full,1);
    assert(tty_service_submit(&service,&blocked)==TTY_OK);
    service.interrupt=observe_interrupt;service.interrupt_context=&blocked;interrupt_calls=0;
    trigger=request(1,TTY_REQUEST_WRITE,"\003tail",5);
    assert(tty_service_submit(&service,&trigger)==TTY_OK);
    assert(interrupt_calls==1);
    completion(&blocked,TTY_OK,1,0);completion(&trigger,TTY_OK,1,0);
    service.interrupt=NULL;service.interrupt_context=NULL;
    assert(tty_pair_write(&service.pair,TTY_MASTER,"\003",1).count==1);
    blocked=request(2,TTY_REQUEST_WRITE,full,1);
    assert(tty_service_submit(&service,&blocked)==TTY_OK);
    assert(!tty_service_take(&service));
    assert(tty_pair_take_interrupt(&service.pair)==1);
    tty_service_pump(&service);completion(&blocked,TTY_OK,1,0);
    puts("TTY INTERRUPT ORDER PASS: dispatch before flush-unblocked writer, exact suffix, ownerless event suspends progress");
}
int main(void)
{
    /* Rebinding must invalidate accepted setters before FLUSH can discard data. */
    {
        unsigned action;TtyConfig c;TtyRequest r;unsigned char b[4];
        for(action=0;action<2;++action) {
            tty_service_init(&service);service.interrupt=observe_interrupt;
            service.signal_generation=1;c=service.pair.config;c.local_flags=TTY_ISIG;c.intr=4;
            assert(tty_pair_write(&service.pair,TTY_SLAVE,"out",3).count==3);
            assert(tty_pair_write(&service.pair,TTY_MASTER,"keep",4).count==4);
            r=request(2,action?TTY_REQUEST_DRAIN_CONFIG_DISCARD:TTY_REQUEST_DRAIN_CONFIG,&c,sizeof c);
            assert(tty_service_submit(&service,&r)==TTY_OK);assert(!tty_service_take(&service));
            service.signal_generation=2;
            assert(tty_pair_flush(&service.pair,TTY_FLUSH_OUTPUT)==TTY_OK);
            tty_service_pump(&service);completion(&r,TTY_UNSUPPORTED,0,0);
            assert(!service.pair.config.local_flags);
            assert(tty_pair_read(&service.pair,TTY_SLAVE,b,4).count==4 && !memcmp(b,"keep",4));
        }
    }
    interrupt_order();
    output_flow();
    read_timing();
    {
        unsigned char b[8];TtyConfig c;TtyRequest r;
        tty_service_init(&service);c=service.pair.config;c.vmin=3;
        assert(tty_pair_configure(&service.pair,&c)==TTY_OK);
        r=request(2,TTY_REQUEST_READ,b,sizeof b);assert(tty_service_submit(&service,&r)==TTY_OK);
        assert(tty_pair_write(&service.pair,TTY_MASTER,"ab",2).count==2);
        tty_service_pump(&service);assert(!tty_service_take(&service));
        assert(tty_pair_write(&service.pair,TTY_MASTER,"c",1).count==1);
        tty_service_pump(&service);completion(&r,TTY_OK,3,0);assert(!memcmp(b,"abc",3));
    }
    drain_config();
    drain_boundary();drain_lifetime();
    drain_order();
    configure_discard_pressure();
    flush_pressure();
    zero_length_preserves_fifo();
    duplex_pressure();
    zero_length_behind_blocked();
    atomic_cancel_all();
    lane_admission();
    wake_and_pressure();handles_and_cancel();slots_and_fifo();
    eof_and_retirement();
    puts("TTY SERVICE PASS: pending wakeup, partial write, FIFO, cancel, leases, stale handles, slot pressure");
    return 0;
}
