#include "../src/tty_pair.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static TtyPair pair;
static void result(TtyResult r, TtyStatus s, size_t n)
{ assert(r.status==s); assert(r.count==n); }

/* Catches mixed directions, strlen-based binary writes, and premature close. */
static void raw_and_leases(void)
{
    unsigned char b[8];
    const unsigned char binary[]={0,255,13,10,4};
    tty_pair_init(&pair);
    assert(!tty_pair_readable(&pair,TTY_SLAVE));
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_AGAIN,0);
    result(tty_pair_write(&pair,TTY_MASTER,binary,sizeof binary),TTY_OK,5);
    result(tty_pair_write(&pair,TTY_SLAVE,"out",3),TTY_OK,3);
    result(tty_pair_read(&pair,TTY_SLAVE,b,2),TTY_OK,2);
    assert(b[0]==0 && b[1]==255);
    result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_OK,3);
    assert(!memcmp(b,"out",3));
    assert(tty_pair_retain(&pair,TTY_MASTER)==TTY_OK);
    assert(tty_pair_release(&pair,TTY_MASTER)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"z",1),TTY_OK,1);
    assert(tty_pair_release(&pair,TTY_MASTER)==TTY_OK);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,4);
    assert(!memcmp(b,"\r\n\004z",4));
    assert(tty_pair_readable(&pair,TTY_SLAVE));
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_EOF,0);
    result(tty_pair_write(&pair,TTY_SLAVE,"x",1),TTY_CLOSED,0);
    assert(tty_pair_retain(&pair,TTY_MASTER)==TTY_CLOSED);
    assert(tty_pair_release(&pair,TTY_MASTER)==TTY_CLOSED);
    assert(tty_pair_release(&pair,TTY_SLAVE)==TTY_OK);
}

/* Catches VEOF-as-hangup, merged records, and losing the last partial line. */
static void canonical_records(void)
{
    unsigned char b[32];TtyConfig c;
    tty_pair_init(&pair);
    assert(tty_pair_get_config(&pair,1,&c)==TTY_OK);
    c.local_flags=TTY_CANON;
    assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"ab",2),TTY_OK,2);
    assert(!tty_pair_readable(&pair,TTY_SLAVE));
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_AGAIN,0);
    result(tty_pair_write(&pair,TTY_MASTER,"\004\004x\ny\004tail",10),TTY_OK,10);
    result(tty_pair_read(&pair,TTY_SLAVE,b,1),TTY_OK,1);assert(b[0]=='a');
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,1);assert(b[0]=='b');
    result(tty_pair_read(&pair,TTY_SLAVE,NULL,0),TTY_OK,0);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_EOF,0);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,2);assert(!memcmp(b,"x\n",2));
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,1);assert(b[0]=='y');
    assert(tty_pair_configure(&pair,&c)==TTY_OK);
    assert(!tty_pair_readable(&pair,TTY_SLAVE));
    assert(tty_pair_release(&pair,TTY_MASTER)==TTY_OK);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,4);assert(!memcmp(b,"tail",4));
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_EOF,0);
    assert(tty_pair_release(&pair,TTY_SLAVE)==TTY_OK);
}

/* Catches consuming input before its echo fits, and splitting CRLF expansion. */
static void echo_backpressure(void)
{
    unsigned char b[TTY_QUEUE_CAPACITY],fill[TTY_QUEUE_CAPACITY];TtyConfig c;
    memset(fill,'Q',sizeof fill);tty_pair_init(&pair);
    assert(tty_pair_get_config(&pair,1,&c)==TTY_OK);
    c.input_flags=TTY_ICRNL;c.output_flags=TTY_OPOST|TTY_ONLCR;c.local_flags=TTY_ECHO;
    assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_SLAVE,fill,sizeof fill-1),TTY_OK,sizeof fill-1);
    result(tty_pair_write(&pair,TTY_MASTER,"\r",1),TTY_AGAIN,0);
    assert(!tty_pair_readable(&pair,TTY_SLAVE));
    result(tty_pair_write(&pair,TTY_SLAVE,"\n",1),TTY_AGAIN,0);
    result(tty_pair_read(&pair,TTY_MASTER,b,1),TTY_OK,1);assert(b[0]=='Q');
    result(tty_pair_write(&pair,TTY_MASTER,"\r",1),TTY_OK,1);
    result(tty_pair_read(&pair,TTY_SLAVE,b,1),TTY_OK,1);assert(b[0]=='\n');
    result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_OK,sizeof b);
    assert(!memcmp(b,fill,sizeof b-2));assert(!memcmp(b+sizeof b-2,"\r\n",2));
    c.local_flags=TTY_CANON|TTY_ECHO;
    assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"a\004\004",3),TTY_OK,3);
    result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_OK,1);assert(b[0]=='a');
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,1);assert(b[0]=='a');
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_EOF,0);
    assert(tty_pair_release(&pair,TTY_SLAVE)==TTY_OK);
    assert(tty_pair_release(&pair,TTY_MASTER)==TTY_OK);
}

/* Catches accepting unsupported control versions or mutating failed requests. */
static void controls(void)
{
    TtyConfig c,old;TtyGeometry g,before;
    unsigned bit;
    tty_pair_init(&pair);
    assert(tty_pair_get_config(&pair,1,&old)==TTY_OK);c=old;c.version=99;
    assert(tty_pair_configure(&pair,&c)==TTY_UNSUPPORTED);
    for(bit=3;bit<32;++bit) {
        TtyPair saved=pair;
        c=old;c.input_flags=UINT32_C(1)<<bit;
        assert(tty_pair_configure(&pair,&c)==TTY_UNSUPPORTED);
        assert(!memcmp(&pair,&saved,sizeof pair));
    }
    c=old;c.local_flags=0x80000000u;
    assert(tty_pair_configure(&pair,&c)==TTY_UNSUPPORTED);
    c=old;c.eof='\n';assert(tty_pair_configure(&pair,&c)==TTY_INVALID);
    assert(tty_pair_get_config(&pair,1,&c)==TTY_OK);
    assert(!memcmp(&c,&old,sizeof c));
    c.eof=7;assert(tty_pair_get_config(&pair,99,&c)==TTY_UNSUPPORTED);assert(c.eof==7);
    assert(tty_pair_get_geometry(&pair,1,&g)==TTY_OK);
    assert(g.rows==24 && g.columns==80);
    g.rows=31;g.columns=109;g.xpixel=872;g.ypixel=527;
    assert(tty_pair_set_geometry(&pair,&g)==TTY_OK);before=g;
    g.rows=0;assert(tty_pair_set_geometry(&pair,&g)==TTY_INVALID);
    g=before;g.columns=65536;assert(tty_pair_set_geometry(&pair,&g)==TTY_INVALID);
    g=before;g.version=2;assert(tty_pair_set_geometry(&pair,&g)==TTY_UNSUPPORTED);
    assert(tty_pair_get_geometry(&pair,1,&g)==TTY_OK);assert(!memcmp(&g,&before,sizeof g));
    g.rows=7;assert(tty_pair_get_geometry(&pair,2,&g)==TTY_UNSUPPORTED);assert(g.rows==7);
    result(tty_pair_write(NULL,TTY_MASTER,"x",1),TTY_INVALID,0);
    result(tty_pair_read(&pair,(TtyEnd)9,&g,1),TTY_INVALID,0);
    result(tty_pair_write(&pair,TTY_MASTER,NULL,1),TTY_INVALID,0);
    result(tty_pair_read(&pair,TTY_SLAVE,NULL,1),TTY_INVALID,0);
    result(tty_pair_write(&pair,TTY_MASTER,NULL,0),TTY_OK,0);
    assert(tty_pair_release(&pair,TTY_MASTER)==TTY_OK);
    assert(tty_pair_set_geometry(&pair,&before)==TTY_CLOSED);
    assert(tty_pair_configure(&pair,&old)==TTY_CLOSED);
    assert(tty_pair_release(&pair,TTY_SLAVE)==TTY_OK);
}

/* Catches dropped/reordered bytes when partial writes repeatedly wrap both rings.
   Expected streams are generated independently of the queue implementation. */
static void duplex_stress(void)
{
    static unsigned char input[8193], output[8193];
    unsigned char received[257];
    unsigned cycle;
    for (cycle = 0; cycle < 1000; ++cycle)
    {
        size_t sent[2] = {0, 0}, read_count[2] = {0, 0}, i;
        unsigned step = 0, blocked[2] = {0, 0};
        uint32_t random = cycle + 1;
        tty_pair_init(&pair);
        for (i = 0; i < sizeof input; ++i)
        {
            random = random * 1664525u + 1013904223u;
            input[i] = (unsigned char)(random >> 24);
            output[i] = (unsigned char)(random >> 16);
        }
        while (read_count[0] < sizeof input || read_count[1] < sizeof output)
        {
            unsigned direction;
            assert(++step < 4096); /* A stalled retry loop must fail, not hang. */
            for (direction = 0; direction < 2; ++direction)
            {
                const unsigned char *source = direction ? output : input;
                TtyEnd writer = direction ? TTY_SLAVE : TTY_MASTER;
                TtyEnd reader = direction ? TTY_MASTER : TTY_SLAVE;
                size_t remaining = sizeof input - sent[direction];
                size_t read_request = 1 + (step * 37 + cycle + direction * 53) % sizeof received;
                TtyResult r;
                if (remaining)
                {
                    size_t request = remaining < 719 ? remaining : 719;
                    r = tty_pair_write(&pair, writer, source + sent[direction], request);
                    assert(r.count <= request);
                    assert(r.status == (r.count ? TTY_OK : TTY_AGAIN));
                    if (r.count < request) ++blocked[direction];
                    sent[direction] += r.count;
                }
                /* Let the rings fill first; then vary read sizes independently. */
                if (step < 4) continue;
                r = tty_pair_read(&pair, reader, received, read_request);
                assert(r.status == (r.count ? TTY_OK : TTY_AGAIN));
                assert(r.count <= read_request);
                assert(read_count[direction] + r.count <= sent[direction]);
                assert(!memcmp(received, source + read_count[direction], r.count));
                read_count[direction] += r.count;
            }
        }
        assert(blocked[0] && blocked[1]);
        assert(tty_pair_release(&pair, TTY_SLAVE) == TTY_OK);
        result(tty_pair_read(&pair, TTY_MASTER, received, sizeof received), TTY_EOF, 0);
        assert(tty_pair_release(&pair, TTY_MASTER) == TTY_OK);
        assert(!tty_pair_readable(&pair, TTY_MASTER));
        assert(!tty_pair_readable(&pair, TTY_SLAVE));
    }
    puts("TTY STRESS PASS: cycles=1000 input_bytes=8193000 output_bytes=8193000");
}

/* Catches overwriting older records or losing an unpublished line at hangup. */
static void full_canonical_hangup(void)
{
    unsigned char lines[TTY_QUEUE_CAPACITY], tail[TTY_LINE_CAPACITY], b[TTY_LINE_CAPACITY];
    size_t i;
    TtyConfig c;
    tty_pair_init(&pair);
    assert(tty_pair_get_config(&pair, TTY_PAIR_VERSION, &c) == TTY_OK);
    c.local_flags = TTY_CANON;
    assert(tty_pair_configure(&pair, &c) == TTY_OK);
    memset(lines, '\n', sizeof lines);
    for (i = 0; i < sizeof tail; ++i) tail[i] = (unsigned char)('a' + i % 26);
    result(tty_pair_write(&pair, TTY_MASTER, lines, sizeof lines), TTY_OK, sizeof lines);
    result(tty_pair_write(&pair, TTY_MASTER, tail, sizeof tail), TTY_OK, sizeof tail);
    result(tty_pair_write(&pair, TTY_MASTER, "x", 1), TTY_AGAIN, 0);
    result(tty_pair_write(&pair, TTY_MASTER, "\004", 1), TTY_AGAIN, 0);
    assert(tty_pair_release(&pair, TTY_MASTER) == TTY_OK);
    for (i = 0; i < sizeof lines; ++i)
    {
        result(tty_pair_read(&pair, TTY_SLAVE, b, sizeof b), TTY_OK, 1);
        assert(b[0] == '\n');
    }
    result(tty_pair_read(&pair, TTY_SLAVE, b, 7), TTY_OK, 7);
    assert(!memcmp(b, tail, 7));
    result(tty_pair_read(&pair, TTY_SLAVE, b, sizeof b), TTY_OK, sizeof tail - 7);
    assert(!memcmp(b, tail + 7, sizeof tail - 7));
    result(tty_pair_read(&pair, TTY_SLAVE, b, sizeof b), TTY_EOF, 0);
    assert(tty_pair_release(&pair, TTY_SLAVE) == TTY_OK);
}

/* Catches delimiter bits leaking into binary data and VEOF becoming permanent. */
static void canonical_wrap(void)
{
    const unsigned char record[] = {'A', 0, 255, '\n', 4};
    unsigned char b[8];
    unsigned i;
    TtyConfig c;
    tty_pair_init(&pair);
    assert(tty_pair_get_config(&pair, TTY_PAIR_VERSION, &c) == TTY_OK);
    c.local_flags = TTY_CANON;
    assert(tty_pair_configure(&pair, &c) == TTY_OK);
    for (i = 0; i < 4096; ++i)
    {
        result(tty_pair_write(&pair, TTY_MASTER, record, sizeof record), TTY_OK, sizeof record);
        result(tty_pair_read(&pair, TTY_SLAVE, b, 2), TTY_OK, 2);
        assert(b[0] == 'A' && b[1] == 0);
        result(tty_pair_read(&pair, TTY_SLAVE, b, sizeof b), TTY_OK, 2);
        assert(b[0] == 255 && b[1] == '\n');
        result(tty_pair_read(&pair, TTY_SLAVE, NULL, 0), TTY_OK, 0);
        result(tty_pair_read(&pair, TTY_SLAVE, b, sizeof b), TTY_EOF, 0);
        result(tty_pair_read(&pair, TTY_SLAVE, b, sizeof b), TTY_AGAIN, 0);
    }
    assert(tty_pair_release(&pair, TTY_MASTER) == TTY_OK);
    assert(tty_pair_release(&pair, TTY_SLAVE) == TTY_OK);
    puts("TTY CANONICAL PASS: records=4096 temporary_eof=4096 full_ring_hangup=1");
}

static void read_status(void)
{
    unsigned char b[4];TtyConfig c;
    tty_pair_init(&pair);
    assert(tty_pair_read_status(NULL,TTY_SLAVE)==TTY_INVALID);
    assert(tty_pair_read_status(&pair,(TtyEnd)9)==TTY_INVALID);
    assert(tty_pair_read_status(&pair,TTY_SLAVE)==TTY_AGAIN);
    result(tty_pair_write(&pair,TTY_MASTER,"\0",1),TTY_OK,1);
    assert(tty_pair_read_status(&pair,TTY_SLAVE)==TTY_OK);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,1);assert(b[0]==0);
    assert(tty_pair_get_config(&pair,1,&c)==TTY_OK);c.local_flags=TTY_CANON;
    assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"\004a",2),TTY_OK,2);
    assert(tty_pair_read_status(&pair,TTY_SLAVE)==TTY_EOF);
    assert(tty_pair_read_status(&pair,TTY_SLAVE)==TTY_EOF);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_EOF,0);
    assert(tty_pair_read_status(&pair,TTY_SLAVE)==TTY_AGAIN);
    assert(tty_pair_release(&pair,TTY_MASTER)==TTY_OK);
    assert(tty_pair_read_status(&pair,TTY_SLAVE)==TTY_OK);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,1);assert(b[0]=='a');
    assert(tty_pair_read_status(&pair,TTY_SLAVE)==TTY_EOF);
    assert(tty_pair_read_status(&pair,TTY_MASTER)==TTY_CLOSED);
}
/* Catches double translation, IGNCR after echo, and consuming a blocked
   translated byte twice. Expected streams are literal, not derived by flags. */
static void input_mapping(void)
{
    static const struct { unsigned flags; const char *want; size_t count; } cases[]={
        {0,"A\r\nB",4}, {1,"A\n\nB",4}, {2,"A\nB",3},
        {3,"A\nB",3}, {4,"A\r\rB",4}, {5,"A\n\rB",4},
        {6,"A\rB",3}, {7,"A\rB",3}
    };
    unsigned i,split;TtyConfig c;unsigned char b[TTY_QUEUE_CAPACITY],fill[TTY_QUEUE_CAPACITY];
    for(i=0;i<sizeof cases/sizeof cases[0];++i) for(split=0;split<=4;++split) {
        tty_pair_init(&pair);c=pair.config;c.input_flags=cases[i].flags;c.local_flags=TTY_ECHO;
        assert(tty_pair_configure(&pair,&c)==TTY_OK);
        result(tty_pair_write(&pair,TTY_MASTER,"A\r\nB",split),TTY_OK,split);
        result(tty_pair_write(&pair,TTY_MASTER,&"A\r\nB"[split],4-split),TTY_OK,4-split);
        result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,cases[i].count);
        assert(!memcmp(b,cases[i].want,cases[i].count));
        result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_OK,cases[i].count);
        assert(!memcmp(b,cases[i].want,cases[i].count));
        result(tty_pair_write(&pair,TTY_SLAVE,"\r\n",2),TTY_OK,2);
        result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_OK,2);
        assert(!memcmp(b,"\r\n",2)); /* Input flags must not affect output. */
    }
    tty_pair_init(&pair);c=pair.config;c.input_flags=7;c.local_flags=TTY_ECHO;
    assert(tty_pair_configure(&pair,&c)==TTY_OK);memset(fill,'Q',sizeof fill);
    result(tty_pair_write(&pair,TTY_MASTER,fill,sizeof fill),TTY_OK,sizeof fill);
    /* Both queues full: ignored CR consumes no input/echo space. */
    result(tty_pair_write(&pair,TTY_MASTER,"\r\nZ",3),TTY_OK,1);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,sizeof b);
    assert(!memcmp(b,fill,sizeof b));
    result(tty_pair_write(&pair,TTY_MASTER,"\nZ",2),TTY_AGAIN,0);
    result(tty_pair_read(&pair,TTY_MASTER,b,1),TTY_OK,1);
    result(tty_pair_write(&pair,TTY_MASTER,"\nZ",2),TTY_OK,1);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,1);assert(b[0]=='\r');
    result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_OK,sizeof b);
    assert(!memcmp(b,fill,sizeof b-1) && b[sizeof b-1]=='\r');
    result(tty_pair_write(&pair,TTY_MASTER,"Z",1),TTY_OK,1);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,1);assert(b[0]=='Z');
    result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_OK,1);assert(b[0]=='Z');
    /* A translated NL is not a canonical delimiter; a translated CR is. */
    tty_pair_init(&pair);c=pair.config;c.input_flags=5;c.local_flags=TTY_CANON;
    assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"A\n",2),TTY_OK,2);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_AGAIN,0);
    result(tty_pair_write(&pair,TTY_MASTER,"\r",1),TTY_OK,1);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,3);assert(!memcmp(b,"A\r\n",3));
    puts("TTY INPUT MAPPING PASS: combinations=8 split_cases=40 pressure=1 canonical=1");
}
/* Mode switches preserve accepted bytes without reapplying mapping/echo.
   Raw data before a canonical EOF must be returned before the zero read. */
static void mode_preservation(void)
{
    TtyConfig c;unsigned char b[TTY_QUEUE_CAPACITY],full[TTY_QUEUE_CAPACITY];unsigned cycle;
    for(cycle=0;cycle<100;++cycle) {
        tty_pair_init(&pair);c=pair.config;
        result(tty_pair_write(&pair,TTY_MASTER,"r\r\004",3),TTY_OK,3);
        c.local_flags=TTY_CANON|TTY_ECHO;c.input_flags=TTY_ICRNL;
        assert(tty_pair_configure(&pair,&c)==TTY_OK);
        result(tty_pair_write(&pair,TTY_MASTER,"\004tail",5),TTY_OK,5);
        result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,3);
        assert(!memcmp(b,"r\r\004",3));
        result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_EOF,0);
        result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_AGAIN,0);
        c.local_flags=0;assert(tty_pair_configure(&pair,&c)==TTY_OK);
        assert(tty_pair_read_status(&pair,TTY_SLAVE)==TTY_OK);
        result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,4);assert(!memcmp(b,"tail",4));
        result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_OK,4);assert(!memcmp(b,"tail",4));
        result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_AGAIN,0);
    }
    /* A full ready queue plus a partial line exceeds one ring. A switch must
       retain the tail, even if switched back before any queue space opens. */
    memset(full,'q',sizeof full);tty_pair_init(&pair);c=pair.config;
    result(tty_pair_write(&pair,TTY_MASTER,full,sizeof full),TTY_OK,sizeof full);
    c.local_flags=TTY_CANON;assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"tail",4),TTY_OK,4);
    c.local_flags=0;assert(tty_pair_configure(&pair,&c)==TTY_OK);
    c.local_flags=TTY_CANON;assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"new\n",4),TTY_AGAIN,0);
    result(tty_pair_read(&pair,TTY_SLAVE,b,3),TTY_OK,3);assert(!memcmp(b,"qqq",3));
    result(tty_pair_write(&pair,TTY_MASTER,"new\n",4),TTY_AGAIN,0);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,sizeof full-3);
    assert(!memcmp(b,full,sizeof full-3));
    result(tty_pair_write(&pair,TTY_MASTER,"new\n",4),TTY_OK,4);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,4);assert(!memcmp(b,"tail",4));
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,4);assert(!memcmp(b,"new\n",4));
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_AGAIN,0);
    puts("TTY MODE PRESERVATION PASS: cycles=100 raw/canonical, EOF boundary, exact echo, full ring, repeated switch, ordered tail");
}
static void frozen_tail_lifetime(void)
{
    unsigned action;TtyConfig c;TtyPair saved;unsigned char b[TTY_QUEUE_CAPACITY],full[TTY_QUEUE_CAPACITY];
    memset(full,'q',sizeof full);
    for(action=0;action<3;++action) {
        tty_pair_init(&pair);c=pair.config;
        result(tty_pair_write(&pair,TTY_MASTER,full,sizeof full),TTY_OK,sizeof full);
        c.local_flags=TTY_CANON;assert(tty_pair_configure(&pair,&c)==TTY_OK);
        result(tty_pair_write(&pair,TTY_MASTER,"tail",4),TTY_OK,4);
        c.local_flags=0;assert(tty_pair_configure(&pair,&c)==TTY_OK);
        assert(pair.line_ready && pair.line_count==4);saved=pair;
        result(tty_pair_read(&pair,TTY_SLAVE,NULL,0),TTY_OK,0);
        result(tty_pair_read(&pair,TTY_MASTER,NULL,0),TTY_OK,0);
        result(tty_pair_write(&pair,TTY_MASTER,NULL,0),TTY_OK,0);
        assert(!memcmp(&pair,&saved,sizeof pair));
        c.eof='\n';assert(tty_pair_configure(&pair,&c)==TTY_INVALID);
        assert(!memcmp(&pair,&saved,sizeof pair));c.eof=4;
        if(action==2) {
            assert(tty_pair_release(&pair,TTY_MASTER)==TTY_OK);
            result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,sizeof full);assert(!memcmp(b,full,sizeof full));
            result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,4);assert(!memcmp(b,"tail",4));
            result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_EOF,0);
            assert(tty_pair_read_status(&pair,TTY_SLAVE)==TTY_EOF);
        } else {
            if(action==0) assert(tty_pair_flush(&pair,TTY_FLUSH_INPUT)==TTY_OK);
            else assert(tty_pair_configure_discard_input(&pair,&c)==TTY_OK);
            assert(!pair.line_ready && !pair.line_count);
            result(tty_pair_write(&pair,TTY_MASTER,"new",3),TTY_OK,3);
            result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,3);assert(!memcmp(b,"new",3));
            result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_AGAIN,0);
        }
    }
    tty_pair_init(&pair);c=pair.config;c.local_flags=TTY_CANON;
    assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"\004",1),TTY_OK,1);
    c.eof=5;assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"\004\005\005",3),TTY_OK,3);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_EOF,0);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,1);assert(b[0]==4);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_EOF,0);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_AGAIN,0);
    puts("TTY FROZEN TAIL PASS: empty I/O, invalid config, flush/discard, hangup, old/new VEOF");
}
static void canonical_pressure(void)
{
    TtyConfig c;TtyPair saved;unsigned char full[TTY_QUEUE_CAPACITY],b[TTY_QUEUE_CAPACITY];unsigned i;
    memset(full,'q',sizeof full);
    tty_pair_init(&pair);c=pair.config;c.local_flags=TTY_CANON|TTY_ECHO|TTY_ECHOE|TTY_ECHOK;
    c.erase=128;c.kill=22;c.eol=1; /* NUL is a real delimiter, not disabled. */
    assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"\177",1),TTY_OK,1);
    result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_AGAIN,0);
    result(tty_pair_write(&pair,TTY_MASTER,full,TTY_LINE_CAPACITY),TTY_OK,TTY_LINE_CAPACITY);
    result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_OK,TTY_LINE_CAPACITY);
    result(tty_pair_write(&pair,TTY_SLAVE,full,sizeof full),TTY_OK,sizeof full);
    saved=pair;result(tty_pair_write(&pair,TTY_MASTER,"\177",1),TTY_AGAIN,0);
    assert(!memcmp(&pair,&saved,sizeof pair));
    result(tty_pair_read(&pair,TTY_MASTER,b,2),TTY_OK,2);
    saved=pair;result(tty_pair_write(&pair,TTY_MASTER,"\177",1),TTY_AGAIN,0);
    assert(!memcmp(&pair,&saved,sizeof pair));
    result(tty_pair_read(&pair,TTY_MASTER,b,1),TTY_OK,1);
    result(tty_pair_write(&pair,TTY_MASTER,"\177",1),TTY_OK,1);
    assert(pair.line_count==TTY_LINE_CAPACITY-1);
    result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_OK,sizeof b);
    for(i=0;i<sizeof b-3;++i) assert(b[i]=='q');
    assert(!memcmp(b+sizeof b-3,"\b \b",3));
    result(tty_pair_write(&pair,TTY_MASTER,"\025",1),TTY_OK,1);
    result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_OK,2);assert(!memcmp(b,"\025\n",2));
    result(tty_pair_write(&pair,TTY_MASTER,"x\0",2),TTY_OK,2);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,2);assert(!memcmp(b,"x\0",2));
    result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_OK,2);
    c.output_flags=TTY_OPOST|TTY_ONLCR;assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"z",1),TTY_OK,1);
    result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_OK,1);
    result(tty_pair_write(&pair,TTY_SLAVE,full,sizeof full-2),TTY_OK,sizeof full-2);
    saved=pair;result(tty_pair_write(&pair,TTY_MASTER,"\025",1),TTY_AGAIN,0);
    assert(!memcmp(&pair,&saved,sizeof pair));
    result(tty_pair_read(&pair,TTY_MASTER,b,1),TTY_OK,1);
    result(tty_pair_write(&pair,TTY_MASTER,"\025",1),TTY_OK,1);
    result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_OK,sizeof b);
    assert(!memcmp(b+sizeof b-3,"\025\r\n",3));assert(!pair.line_count);
    c.local_flags=TTY_CANON|TTY_NOEOF;c.erase=c.kill=c.eol=0;
    assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"\004\177\025\0\n",5),TTY_OK,5);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,5);assert(!memcmp(b,"\004\177\025\0\n",5));
    c.local_flags=0;c.erase=128;c.kill=22;c.eol=1;
    assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"\004\177\025\0",4),TTY_OK,4);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,4);assert(!memcmp(b,"\004\177\025\0",4));
    puts("TTY CANONICAL PRESSURE PASS: full line, atomic erase echo, NUL delimiter, disabled controls, raw bypass");
}
/* Missing recognition leaks VINTR to input; missing flush retains old data;
   missing event boundary silently consumes the suffix or loses repeated events. */
static void input_interrupt(void)
{
    TtyConfig c;unsigned byte;unsigned char b[TTY_QUEUE_CAPACITY],full[TTY_QUEUE_CAPACITY];
    memset(full,'q',sizeof full);tty_pair_init(&pair);c=pair.config;
    c.local_flags=TTY_ISIG|TTY_ECHO;c.intr=4;
    assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"old",3),TTY_OK,3);
    result(tty_pair_write(&pair,TTY_SLAVE,"output",6),TTY_OK,6);
    result(tty_pair_write(&pair,TTY_MASTER,"\003tail",5),TTY_OK,1);
    assert(!pair.input.count && !pair.output.count && pair.output_retired==9);
    result(tty_pair_write(&pair,TTY_MASTER,"tail",4),TTY_AGAIN,0);
    result(tty_pair_write(&pair,TTY_MASTER,NULL,0),TTY_OK,0);
    assert(tty_pair_take_interrupt(&pair)==1);assert(!tty_pair_take_interrupt(&pair));
    result(tty_pair_write(&pair,TTY_MASTER,"tail",4),TTY_OK,4);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,4);assert(!memcmp(b,"tail",4));
    result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_OK,4);assert(!memcmp(b,"tail",4));
    c.local_flags=TTY_ISIG|TTY_NOFLSH;assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,full,sizeof full),TTY_OK,sizeof full);
    result(tty_pair_write(&pair,TTY_SLAVE,full,sizeof full),TTY_OK,sizeof full);
    result(tty_pair_write(&pair,TTY_MASTER,"\003",1),TTY_OK,1);
    assert(pair.input.count==sizeof full && pair.output.count==sizeof full);
    assert(tty_pair_take_interrupt(&pair)==1);
    c.local_flags=TTY_ISIG;assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"\003\003",2),TTY_OK,1);
    assert(tty_pair_take_interrupt(&pair)==1);
    result(tty_pair_write(&pair,TTY_MASTER,"\003",1),TTY_OK,1);
    assert(tty_pair_retain(&pair,TTY_SLAVE)==TTY_OK);
    assert(tty_pair_release(&pair,TTY_SLAVE)==TTY_OK);
    assert(tty_pair_take_interrupt(&pair)==1);
    result(tty_pair_write(&pair,TTY_MASTER,"\003",1),TTY_OK,1);
    assert(tty_pair_release(&pair,TTY_SLAVE)==TTY_OK);
    assert(!tty_pair_take_interrupt(&pair));

    tty_pair_init(&pair);c=pair.config;c.intr=4; /* ISIG disabled: literal byte. */
    assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"\003",1),TTY_OK,1);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,1);assert(b[0]==3);
    assert(!tty_pair_take_interrupt(&pair));
    c.local_flags=TTY_ISIG;c.intr=0;assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"\003",1),TTY_OK,1);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,1);assert(b[0]==3);
    c.intr=11;c.input_flags=TTY_ICRNL;assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"\r",1),TTY_OK,1);
    assert(tty_pair_take_interrupt(&pair)==1);
    c.intr=257;assert(tty_pair_configure(&pair,&c)==TTY_INVALID);
    /* VINTR must bypass a frozen tail and full stopped echo output. */
    tty_pair_init(&pair);
    result(tty_pair_write(&pair,TTY_MASTER,full,sizeof full),TTY_OK,sizeof full);
    c=pair.config;c.local_flags=TTY_CANON;
    assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"tail",4),TTY_OK,4);
    c.local_flags=TTY_ISIG|TTY_ECHO;c.intr=4;
    assert(tty_pair_configure(&pair,&c)==TTY_OK);assert(pair.line_ready);
    result(tty_pair_write(&pair,TTY_SLAVE,full,sizeof full),TTY_OK,sizeof full);
    assert(tty_pair_output_flow(&pair,1)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"\003x",2),TTY_OK,1);
    assert(!pair.line_ready && !pair.line_count && !pair.input.count && !pair.output.count);
    assert(pair.output_stopped && tty_pair_take_interrupt(&pair)==1);
    result(tty_pair_write(&pair,TTY_MASTER,"\003",1),TTY_OK,1);
    assert(tty_pair_release(&pair,TTY_MASTER)==TTY_OK);assert(!tty_pair_take_interrupt(&pair));
    /* All encoded bytes, including NUL/255; config/flush must not lose events. */
    for(byte=0;byte<256;++byte) {
        unsigned char ch=(unsigned char)byte;
        tty_pair_init(&pair);c=pair.config;c.local_flags=TTY_ISIG;c.intr=(uint16_t)(byte+1);
        assert(tty_pair_configure(&pair,&c)==TTY_OK);
        result(tty_pair_write(&pair,TTY_MASTER,&ch,1),TTY_OK,1);
        c.local_flags=0;assert(tty_pair_configure(&pair,&c)==TTY_OK);
        assert(tty_pair_flush(&pair,TTY_FLUSH_INPUT|TTY_FLUSH_OUTPUT)==TTY_OK);
        result(tty_pair_write(&pair,TTY_MASTER,"x",1),TTY_AGAIN,0);
        assert(tty_pair_take_interrupt(&pair)==1);assert(!tty_pair_take_interrupt(&pair));
    }
    tty_pair_init(&pair);
    result(tty_pair_write(&pair,TTY_MASTER,full,sizeof full),TTY_OK,sizeof full);
    c=pair.config;c.local_flags=TTY_CANON;assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"tail",4),TTY_OK,4);
    c.local_flags=TTY_ISIG|TTY_NOFLSH;c.intr=4;assert(tty_pair_configure(&pair,&c)==TTY_OK);
    result(tty_pair_write(&pair,TTY_MASTER,"\003x",2),TTY_OK,1);
    assert(pair.line_ready && pair.line_count==4 && pair.input.count==sizeof full);
    assert(tty_pair_take_interrupt(&pair)==1);
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,sizeof b);
    assert(!memcmp(b,full,sizeof full));
    result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,4);assert(!memcmp(b,"tail",4));
    puts("TTY INPUT INTERRUPT PASS: flush, noflush, exact suffix, event ownership, close, disabled and mapped bytes");
}
int main(void)
{
    input_interrupt();
    canonical_pressure();
    /* ECHONL must echo the delimiter even when ordinary ECHO is disabled. */
    {
        TtyConfig c;unsigned char b[8];
        tty_pair_init(&pair);c=pair.config;c.local_flags=TTY_CANON|TTY_ECHONL;
        assert(tty_pair_configure(&pair,&c)==TTY_OK);
        result(tty_pair_write(&pair,TTY_MASTER,"x\n",2),TTY_OK,2);
        result(tty_pair_read(&pair,TTY_MASTER,b,sizeof b),TTY_OK,1);assert(b[0]=='\n');
        result(tty_pair_read(&pair,TTY_SLAVE,b,sizeof b),TTY_OK,2);assert(!memcmp(b,"x\n",2));
    }
    frozen_tail_lifetime();
    mode_preservation();
    input_mapping();
    read_status();
    raw_and_leases();
    canonical_records();
    echo_backpressure();controls();
    full_canonical_hangup();canonical_wrap();duplex_stress();
    puts("TTY PAIR PASS: raw binary duplex, leases, drain and hangup");
    return 0;
}
