/* Synthetic input -> real DOS peer -> production parser/renderer -> RGB readback.
 * This deliberately does not claim physical keyboard or scanout latency. */
#define XTERM_FLOOD_DIAG
#define main xterm_preview_main
#include "../src/aros_xterm.c"
#undef main
#include "flood_protocol.h"
#include "flood_clock.h"
#include <dos/dostags.h>
#include <stdlib.h>

typedef struct {
    struct Message msg;
    uint64_t seq,due,offer;
    unsigned op;
} Injection;
typedef struct {
    struct MsgPort *port;
    struct SignalSemaphore gate;
    Injection *messages;
    uint64_t epoch;
    unsigned seconds;
    ULONG hz,task;
    unsigned sent;
    int stop,error,done,launched;
} Injector;
static struct {
    FloodClock clock;
    Injector injector;
    FloodSample *samples;
    FloodParser parser;
    Injection *pending;
    uint64_t start,epoch,end,parsed,completed,offered,admitted,rejected;
    uint64_t input_full,output_full,pending_write,coalesced,groups;
    uint64_t clock_min,clock_max;
    size_t input_high,output_high;
    unsigned seconds,readback,batch;
    int failed,active,timer_active;
    const char *failure;
} bench;
static void fail(const char *reason)
{ if(!bench.failed) bench.failure=reason;bench.failed=1; }
static void injector_worker(void)
{
    Injector *j=FindTask(NULL)->tc_UserData;
    FloodClock clock={0};uint64_t next,minute;unsigned normal=0,burst=0;int stopping=0;
    Wait(SIGBREAKF_CTRL_F);
    if(!flood_clock_open(&clock)) { j->error=1;flood_clock_close(&clock);return; }
    next=j->epoch;minute=j->epoch+(uint64_t)j->hz*60;
    while(!stopping) {
        uint64_t now=flood_now(&clock);unsigned op=FLOOD_KEY;int due=0;
        uint64_t deadline=next;
        ObtainSemaphore(&j->gate);stopping=j->stop;ReleaseSemaphore(&j->gate);
        if(clock.bad || clock.hz!=j->hz) { j->error=1;stopping=1; }
        if(stopping) { op=FLOOD_STOP;deadline=now;due=1; }
        else if(normal>=j->seconds*20) {
            deadline=j->epoch+(uint64_t)j->seconds*j->hz;
            if(now<deadline) { Delay(1);continue; }
            op=FLOOD_STOP;due=1;stopping=1;
        }
        else if(burst) { --burst;deadline=minute-(uint64_t)j->hz*60;due=1; }
        else if(now>=minute && minute<j->epoch+(uint64_t)j->seconds*j->hz) {
            op=FLOOD_PAUSE;deadline=minute;minute+=(uint64_t)j->hz*60;burst=1024;due=1;
        } else if(now>=next) { ++normal;next=j->epoch+(uint64_t)normal*j->hz/20;due=1; }
        if(due) {
            Injection *m;
            if(j->sent==FLOOD_SAMPLES) { j->error=1;break; }
            m=&j->messages[j->sent];memset(m,0,sizeof *m);
            m->seq=++j->sent;m->op=op;m->due=deadline;m->offer=flood_now(&clock);
            PutMsg(j->port,&m->msg);
        } else Delay(1);
    }
    flood_clock_close(&clock);
}
static void timer_arm(void)
{
    bench.clock.io->tr_node.io_Command=TR_ADDREQUEST;
    bench.clock.io->tr_time.tv_secs=0;bench.clock.io->tr_time.tv_micro=50000;
    SendIO((struct IORequest *)bench.clock.io);bench.timer_active=1;
}
static int flood_diag_start(struct Window *window)
{
    char command[600];unsigned i;
    (void)window;
    if(!flood_clock_open(&bench.clock)) { fail("clock open");return 0; }
    bench.start=flood_now(&bench.clock);bench.clock_min=UINT64_MAX;
    for(i=0;i<1000;++i) {
        uint64_t a=flood_now(&bench.clock),b=flood_now(&bench.clock),d=b-a;
        if(d<bench.clock_min) bench.clock_min=d;
        if(d>bench.clock_max) bench.clock_max=d;
    }
    bench.injector.port=CreateMsgPort();
    if(!bench.injector.port) { fail("injector port");return 0; }
    InitSemaphore(&bench.injector.gate);
    snprintf(command,sizeof command,"%s --flood-check",peer_command);
    if(aros_session_start(&process_session,command)) { fail("peer launch");return 0; }
    echo_mode=2;shell_session=0;
    screen_init(&screen,cells,MAX_ROWS*MAX_COLUMNS,24,80);reset_links();
    screen.cursor_visible=0;bench.active=1;timer_arm();return 1;
}
static ULONG flood_diag_signals(void)
{
    if(!bench.active) return 0;
    return (1UL<<bench.injector.port->mp_SigBit)|(1UL<<bench.clock.port->mp_SigBit);
}
static int flood_diag_ready(void)
{ return bench.pending && XSESSION_CAPACITY-process_session.input.length>=FLOOD_INPUT_SIZE; }
static int parsed_record(void *unused,unsigned kind,uint64_t seq,uint64_t tick)
{
    (void)unused;
    if(kind=='A') {
        FloodSample *s;
        if(!seq || seq>bench.admitted || seq>FLOOD_SAMPLES) return 0;
        s=&bench.samples[seq-1];
        if(s->parsed || tick<s->admit) return 0;
        s->peer=tick;s->parsed=flood_now(&bench.clock);bench.parsed=seq;
    }
    return 1;
}
static void flood_diag_output(const unsigned char *b,size_t n)
{
    if(bench.active && n && !flood_output_feed(&bench.parser,b,n,parsed_record,NULL)) fail("output byte/order mismatch");
}
static void flood_diag_queues(void)
{
    if(!bench.active) return;
    if(process_session.input.length>bench.input_high) bench.input_high=process_session.input.length;
    if(process_session.output.length>bench.output_high) bench.output_high=process_session.output.length;
    if(process_session.input.length==XSESSION_CAPACITY) ++bench.input_full;
    if(process_session.output.length==XSESSION_CAPACITY) ++bench.output_full;
    if(process_session.pending_write) ++bench.pending_write;
}
static void flood_diag_upload(struct Window *w,unsigned row,int ok)
{
    unsigned char pixels[72*XRASTER_MAX*3];unsigned i,width=72*xfont_width();
    uint64_t uploaded,readback,count;
    if(!bench.active || row || !bench.parsed || bench.parsed==bench.completed) return;
    /* A pump can end in the middle of the next marker. Its intermediate pixels
     * must not complete the preceding cumulative marker's samples. */
    if(bench.parser.length && (bench.parser.length<7 || bench.parser.bytes[6]=='A')) return;
    if(!ok) { fail("partial marker upload");return; }
    uploaded=flood_now(&bench.clock);
    if(bench.readback) {
        if(ReadPixelArray(pixels,0,0,width*3,w->RPort,MARGIN,MARGIN+xfont_height()/2,width,1,RECTFMT_RGB)!=width) {
            fail("marker readback clipped");return;
        }
        for(i=0;i<72;++i) {
            unsigned bit=i<8?(0xa5u>>(7-i))&1u:(unsigned)((bench.parsed>>(71-i))&1u);
            unsigned at=(i*xfont_width()+xfont_width()/2)*3;
            unsigned char expected=bit?255:0;
            if(pixels[at]!=expected || pixels[at+1]!=expected || pixels[at+2]!=expected) { fail("marker pixel mismatch");return; }
        }
    }
    readback=flood_now(&bench.clock);count=bench.parsed-bench.completed;
    if(!flood_complete(bench.samples,(size_t)bench.admitted,&bench.completed,bench.parsed,uploaded,readback,1)) {
        fail("timestamp/order/completion violation");return;
    }
    ++bench.groups;if(count>1) bench.coalesced+=count;
}
static void flood_diag_event(ULONG event)
{
    if(!bench.active || !bench.epoch) return;
    if(event==IDCMP_NEWSIZE || event==IDCMP_INACTIVEWINDOW || event==IDCMP_CLOSEWINDOW)
        fail("benchmark geometry/focus changed");
}
static int flood_diag_step(struct Window *window)
{
    Injector *j=&bench.injector;unsigned count=0;uint64_t now;
    (void)window;
    if(!bench.active) return 0;
    now=flood_now(&bench.clock);
    if(bench.timer_active && CheckIO((struct IORequest *)bench.clock.io)) {
        WaitIO((struct IORequest *)bench.clock.io);bench.timer_active=0;timer_arm();
    }
    if(bench.clock.bad) fail("nonmonotonic clock/frequency");
    if(!j->launched && bench.parser.frames && now>=bench.start+bench.clock.hz) {
        struct Process *child;
        j->seconds=bench.seconds;j->hz=bench.clock.hz;j->epoch=now+j->hz/10;
        bench.epoch=j->epoch;bench.end=j->epoch+(uint64_t)j->seconds*j->hz;
        child=CreateNewProcTags(NP_Entry,(IPTR)injector_worker,NP_UserData,(IPTR)j,
            NP_Name,(IPTR)"XTerm flood injector",NP_StackSize,65536,NP_NotifyOnDeath,TRUE,
            NP_Input,0,NP_Output,0,NP_Error,0,NP_CurrentDir,0,NP_HomeDir,0,NP_WindowPtr,(IPTR)-1,TAG_DONE);
        if(!child) fail("injector launch");
        else { j->task=GetETaskID(&child->pr_Task);j->launched=1;Signal(&child->pr_Task,SIGBREAKF_CTRL_F); }
    }
    if(j->task && ChildStatus(j->task)==CHILD_EXITED) {
        ChildFree(j->task);j->task=0;j->done=1;
        if(j->error) fail("injector clock/capacity");
    }
    while(count++<bench.batch) {
        Injection *m;FloodSample *s;unsigned char bytes[16];uint64_t admission;
        if(!bench.pending) {
            bench.pending=(Injection *)GetMsg(j->port);
            if(!bench.pending) break;
            m=bench.pending;
            if(m->seq!=bench.offered+1 || m->seq>FLOOD_SAMPLES) { fail("injection sequence");break; }
            s=&bench.samples[m->seq-1];s->due=m->due;s->offer=m->offer;s->dispatch=flood_now(&bench.clock);
            ++bench.offered;
        }
        m=bench.pending;s=&bench.samples[m->seq-1];flood_input(bytes,m->op,m->seq);
        admission=flood_now(&bench.clock);
        if(session_bytes(bytes,sizeof bytes)!=sizeof bytes) { ++bench.rejected;break; }
        s->admit=admission;++bench.admitted;bench.pending=NULL;
    }
    flood_diag_queues();
    if((bench.epoch && now>bench.end+(uint64_t)60*bench.clock.hz) ||
       (!bench.epoch && now>bench.start+(uint64_t)30*bench.clock.hz)) fail("drain/start watchdog");
    if(bench.failed) {
        ObtainSemaphore(&j->gate);j->stop=1;ReleaseSemaphore(&j->gate);
        aros_session_stop(&process_session);
    }
    if(process_session.exited && !process_session.output.length && output_offset==output_length && !j->task) {
        if(!bench.failed && (!flood_finish(&bench.parser) || bench.completed!=bench.admitted ||
            bench.admitted!=j->sent || process_session.result)) fail("final counts/peer status");
        return 1;
    }
    return 0;
}
static void summary(FILE *f,const char *name,uint64_t *v,size_t n)
{
    uint64_t p50=flood_quantile(v,n,50),p95=flood_quantile(v,n,95),p99=flood_quantile(v,n,99),max=flood_quantile(v,n,100);
    fprintf(f,"%s ticks p50=%llu p95=%llu p99=%llu max=%llu\n",name,
        (unsigned long long)p50,(unsigned long long)p95,(unsigned long long)p99,(unsigned long long)max);
}
int main(int argc,char **argv)
{
    char *args[]={"bench-flood",NULL};FILE *csv;uint64_t *v;size_t i;int rc,ok;
    if(argc<3 || argc>5) { puts("bench-flood SECONDS UNIQUE-CSV [upload-only] [BATCH]");return 20; }
    bench.seconds=(unsigned)strtoul(argv[1],NULL,10);bench.readback=argc<4 || strcmp(argv[3],"upload-only");
    bench.batch=argc==5?(unsigned)strtoul(argv[4],NULL,10):8;
    if(!bench.seconds || bench.seconds>600 || !bench.batch || bench.batch>2048) return 20;
    bench.samples=calloc(FLOOD_SAMPLES,sizeof *bench.samples);
    bench.injector.messages=calloc(FLOOD_SAMPLES,sizeof *bench.injector.messages);
    v=calloc(FLOOD_SAMPLES,sizeof *v);
    if(!bench.samples || !bench.injector.messages || !v) return 20;
    rc=xterm_preview_main(1,args);
    if(bench.timer_active) {
        if(!CheckIO((struct IORequest *)bench.clock.io)) AbortIO((struct IORequest *)bench.clock.io);
        WaitIO((struct IORequest *)bench.clock.io);
    }
    if(bench.injector.port) DeleteMsgPort(bench.injector.port);
    ok=!rc && !bench.failed && bench.completed && !bench.injector.task && !process_session.port;
    printf("FLOOD %s reason=%s seconds=%u hz=%lu readback=%u batch=%u\n",ok?"PASS":"FAIL",
        bench.failure?bench.failure:"none",bench.seconds,(unsigned long)bench.clock.hz,bench.readback,bench.batch);
    printf("samples offered=%llu admitted=%llu completed=%llu rejected_attempts=%llu output_bytes=%llu frames=%llu groups=%llu coalesced=%llu\n",
        (unsigned long long)bench.offered,(unsigned long long)bench.admitted,(unsigned long long)bench.completed,
        (unsigned long long)bench.rejected,(unsigned long long)bench.parser.bytes_seen,(unsigned long long)bench.parser.frames,
        (unsigned long long)bench.groups,(unsigned long long)bench.coalesced);
    printf("pressure input_high=%lu output_high=%lu input_full_observations=%llu output_full_observations=%llu pending_write_observations=%llu clock_call_min=%llu max=%llu\n",
        (unsigned long)bench.input_high,(unsigned long)bench.output_high,(unsigned long long)bench.input_full,
        (unsigned long long)bench.output_full,(unsigned long long)bench.pending_write,
        (unsigned long long)bench.clock_min,(unsigned long long)bench.clock_max);
    for(i=0;i<bench.completed;++i) v[i]=bench.samples[i].readback-bench.samples[i].offer;
    summary(stdout,bench.readback?"readback-offer":"upload-offer",v,(size_t)bench.completed);
    for(i=0;i<bench.completed;++i) v[i]=bench.samples[i].readback-bench.samples[i].due;
    summary(stdout,bench.readback?"readback-due":"upload-due",v,(size_t)bench.completed);
    csv=fopen(argv[2],"w");
    if(!csv) ok=0;
    else {
        fputs("sequence,due,offer,dispatch,admit,peer,parsed,uploaded,readback\n",csv);
        for(i=0;i<bench.offered;++i) {
            FloodSample *s=&bench.samples[i];
            fprintf(csv,"%lu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",(unsigned long)i+1,
                (unsigned long long)s->due,(unsigned long long)s->offer,(unsigned long long)s->dispatch,
                (unsigned long long)s->admit,(unsigned long long)s->peer,(unsigned long long)s->parsed,
                (unsigned long long)s->uploaded,(unsigned long long)s->readback);
        }
        if(ferror(csv)) ok=0;
        if(fclose(csv)) ok=0;
    }
    flood_clock_close(&bench.clock);
    free(bench.samples);free(bench.injector.messages);free(v);
    if(fflush(stdout)) ok=0;
    return ok?0:20;
}
