#include "../src/aros_session.h"
#include "session_test_options.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/resident.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

static ArosSession s;
static unsigned char collected[65536];
static size_t length;
static char peer_command[512];
static unsigned current_iteration,completed_iterations;
/* Never unload a test executable while its task-lifecycle hook is registered. */
static void fail(const char *expression,int line)
{
    fprintf(stderr,"session assertion at %d: %s (iteration=%u completed=%u)\n",
        line,expression,current_iteration,completed_iterations);
    fflush(stdout); fflush(stderr);
    aros_session_stop(&s);
    while(s.task_id) { aros_session_poll(&s); Delay(1); }
    while(aros_session_dispose(&s)) { aros_session_poll(&s); Delay(1); }
    fprintf(stderr,"SESSION FAILED completed=%u; safe cleanup finished\n",completed_iterations);
    fflush(stderr);
    exit(20);
}
#undef assert
#define assert(expression) do { if(!(expression)) fail(#expression,__LINE__); } while(0)
/* Bounded polling test deadline; GUI uses the port signal instead of Delay.
 * A deadline failure leaves memory alive while stopping/reaping the child. */
static int until(const char *needle,int exit_needed)
{
    unsigned ticks;
    for(ticks=0;ticks<500;++ticks) {
        aros_session_poll(&s);
        length+=aros_session_read(&s,collected+length,sizeof collected-1-length);
        collected[length]=0;
        if((!needle || strstr((char *)collected,needle)) && (!exit_needed || s.exited)) return 1;
        Delay(1);
    }
    printf("session deadline: result=%ld exited=%d unsupported=%ld bytes=%lu\n%s\n",
        (long)s.result,s.exited,(long)s.unsupported_action,(unsigned long)length,collected);
    puts("requesting EOF and waiting safely");
    aros_session_stop(&s);
    while(s.task_id) { aros_session_poll(&s); Delay(1); }
    return 0;
}
static void assert_disposed(void)
{
    unsigned i;
    assert(!s.task_id && !s.handles && !s.lock_mask);
    assert(!s.pending_read && !s.pending_write && !s.pending_wait);
    assert(!s.port && !s.timer_port && !s.timer && !s.timer_active);
    assert(!s.startup_input && !s.startup_file && !s.opened && !s.signal_target);
    assert(!s.breaker.registered && !s.breaker.target && !s.breaker.resource);
    assert(!s.breaker.hook.h_Entry && !s.breaker.hook.h_Data);
    assert(!s.input.length && !s.output.length && !s.line.length);
    for(i=0;i<3;++i) assert(!s.references[i]);
}
static void reset(void)
{
    assert(aros_session_dispose(&s)==0);
    assert_disposed();
    length=0; collected[0]=0;
}
static void wait_pending(void)
{
    unsigned ticks;
    for(ticks=0;ticks<100 && !s.pending_wait;++ticks) {
        aros_session_poll(&s); Delay(1);
    }
    assert(s.pending_wait && s.timer_active);
    assert(aros_session_signals(&s)&(1UL<<s.timer_port->mp_SigBit));
}
static void run_suite(int require_break)
{
    static const unsigned char payload[]="\033[32mZażółć 中文 😀 👨‍👩‍👧‍👦\033[0m\r\n";
    assert(aros_session_peer_command(peer_command,sizeof peer_command)==0);
    { char command[sizeof peer_command+32];
      snprintf(command,sizeof command,"%s --console-check",peer_command);
      assert(aros_session_start_console(&s,command)==0);
      assert(until("COOKED READY",0));
      assert(aros_session_write(&s,(const unsigned char *)"abc",3)==3);
      { unsigned i; for(i=0;i<5;++i) { aros_session_poll(&s); Delay(1); } }
      assert(s.pending_read && !s.input.length);
      assert(aros_session_write(&s,(const unsigned char *)"\bD\r",3)==3);
      assert(until("RAW READY",0));
      assert(aros_session_write(&s,(const unsigned char *)"\033[A",3)==3);
      assert(until("RAW CTRL-C READY",0));
      assert(aros_session_write(&s,(const unsigned char *)"\003",1)==1);
      assert(until("CONSOLE PASS",1));
      assert(!s.result && !s.unsupported_action);
      reset();
    }
    assert(aros_session_start_shell_profile(&s,"")==0);
    assert(until("> ",0));
    printf("Break notifications=%d\n",s.breaker.registered); fflush(stdout);
    if(require_break) assert(s.breaker.registered);
    if(s.breaker.registered) {
    assert(aros_session_write(&s,(const unsigned char *)"Echo CANCEL_ME",14)==14);
    assert(s.line.length==14);
    length=0;
    assert(aros_session_interrupt(&s));
    assert(!s.line.length && !s.eof);
    assert(until("> ",0));
    { static const unsigned char wait_cmd[]="Wait 30 SECS\r";
      assert(aros_session_write(&s,wait_cmd,sizeof wait_cmd-1)==sizeof wait_cmd-1);
      { unsigned i; for(i=0;i<20;++i) { aros_session_poll(&s); Delay(1); } }
      assert(!s.pending_read && !s.exited);
      while(aros_session_read(&s,collected,sizeof collected)) {}
      length=0;
      assert(aros_session_interrupt(&s));
      assert(until("> ",0)); /* 10s bound: cannot pass by completing Wait 30. */
      assert(!s.exited);
      /* Cancellation can queue an empty line: wait until the Shell consumes
       * it before checking subsequent command output at a line boundary. */
      { unsigned i;
        for(i=0;i<100 && (!s.pending_read || s.input.length);++i) {
            aros_session_poll(&s);
            while(aros_session_read(&s,collected,sizeof collected)) {}
            Delay(1);
        }
        assert(s.pending_read && !s.input.length);
      }
    }
    puts("Ctrl-C prompt cancellation and command interruption PASS");
    } else {
        assert(!aros_session_interrupt(&s));
        assert(!s.eof && !s.line.length && !s.input.length);
        puts("SKIP Ctrl-C forwarding: task lifecycle notifications unavailable");
        puts("Unsupported interrupt leaves Shell input and lifetime intact PASS");
    }
    length=0;
    assert(aros_session_write(&s,(const unsigned char *)"Echo XTERM_SHELL_OK\r",20)==20);
    assert(until("\r\nXTERM_SHELL_OK\r\n",0));
    assert(aros_session_write(&s,(const unsigned char *)"EndCLI\r",7)==7);
    assert(until(NULL,1));
    assert(!s.breaker.target && !aros_break_send(&s.breaker));
    printf("Shell result=%ld unsupported=%ld handles=%lu\n%s\n",
        (long)s.result,(long)s.unsupported_action,(unsigned long)s.handles,collected);
    fflush(stdout);
    assert(!s.result && !s.unsupported_action);
    reset();
    { char command[sizeof peer_command+32];
      snprintf(command,sizeof command,"%s --wait-check",peer_command);
      assert(aros_session_start(&s,command)==0);
      assert(until("WAIT INPUT",0));
      wait_pending();
      assert(aros_session_write(&s,(const unsigned char *)"X",1)==1);
      assert(until("WAIT EOF",0));
      wait_pending();
      aros_session_eof(&s);
      assert(until("WAIT PASS",1));
      printf("Wait result=%ld unsupported=%ld handles=%lu timer=%d\n%s\n",
          (long)s.result,(long)s.unsupported_action,(unsigned long)s.handles,
          s.timer_active,collected);
      fflush(stdout);
      { LONG result=s.result,unsupported=s.unsupported_action;
        reset();
        assert(!result && !unsupported);
      }
      assert(aros_session_start(&s,command)==0);
      assert(until("WAIT INPUT",0));
      wait_pending();
      aros_session_stop(&s);
      assert(!s.pending_wait && !s.timer_active);
      assert(until(NULL,1));
      reset();
    }
    assert(aros_session_start(&s,"C:Type IN:")==0);
    assert(aros_session_start(&s,"C:Type IN:")==-1);
    assert(aros_session_dispose(&s)==-1);
    assert(aros_session_write(&s,payload,sizeof payload-1)==sizeof payload-1);
    aros_session_eof(&s);
    assert(aros_session_write(&s,payload,1)==0);
    assert(until(NULL,1));
    printf("Type result=%ld bytes=%lu/%lu unsupported=%ld handles=%lu\n",
        (long)s.result,(unsigned long)length,(unsigned long)sizeof(payload)-1,
        (long)s.unsupported_action,(unsigned long)s.handles);
    if(s.result || length!=sizeof payload-1) printf("Captured: %s\n",collected);
    assert(s.result==0 && length==sizeof payload-1 && !memcmp(collected,payload,length));
    reset();
    assert(aros_session_start(&s,peer_command)==0);
    assert(until("peer> ",0)); /* Fails if output requires stdin EOF. */
    assert(!s.exited);
    length=0;
    assert(aros_session_write(&s,(const unsigned char *)"unicode\r",8)==8);
    assert(until("👨‍👩‍👧‍👦",0));
    length=0;
    assert(aros_session_write(&s,(const unsigned char *)"burst\r",6)==6);
    assert(until("BURST END",0)); /* Larger than the output queue. */
    assert(length>8192);
    length=0;
    assert(aros_session_write(&s,(const unsigned char *)"exit\r",5)==5);
    assert(until("Bye",1));
    assert(s.result==0);
    reset();
    assert(aros_session_start(&s,peer_command)==0);
    assert(until("peer> ",0));
    aros_session_stop(&s); /* A read is pending; must release it without GUI I/O. */
    assert(until(NULL,1));
    reset();
    assert(aros_session_start(&s,peer_command)==0);
    assert(until("peer> ",0));
    assert(aros_session_write(&s,(const unsigned char *)"burst\r",6)==6);
    { unsigned ticks;
      for(ticks=0;ticks<500 && !s.pending_write;++ticks) {
          aros_session_poll(&s); Delay(1);
      }
      assert(s.pending_write && s.output.length==XSESSION_CAPACITY);
    }
    aros_session_stop(&s); /* Must reply to the borrowed, blocked write. */
    assert(!s.pending_write && !s.output.length);
    assert(until(NULL,1));
    reset();
    assert(aros_session_start(&s,"C:xterm-command-that-does-not-exist")==0);
    assert(until(NULL,1));
    /* Mainline Shell can return zero for an unknown command. Preserve the
     * returned status; verify that its error stream reaches the terminal. */
    assert(strstr((char *)collected,"object not found"));
    reset();
    { char command[sizeof peer_command+32];
      snprintf(command,sizeof command,"%s --lock-exit",peer_command);
      assert(aros_session_start(&s,command)==0);
      assert(until("LOCK LEASE",1));
      assert(!s.lock_mask && !s.handles);
      reset();
    }
    puts("DOS session PASS: exact bytes, live prompt, Unicode, burst, EOF, restart, missing command, blocked-write close, lease cleanup");
}
static int64_t timestamp_ticks(void)
{
    struct DateStamp stamp;
    DateStamp(&stamp);
    return ((int64_t)stamp.ds_Days*24*60+stamp.ds_Minute)*60*TICKS_PER_SECOND+stamp.ds_Tick;
}
int main(int argc,char **argv)
{
    SessionTestOptions options;
    int64_t total_start,total_end;
    if(!session_test_options(argc,(const char *const *)argv,&options)) {
        fprintf(stderr,"Usage: test_aros_session [--repeat N] [--require-break]\n"
            "N must be an unsigned decimal integer from 1 through 1000.\n");
        return 20;
    }
    { struct Resident *resident=FindResident((CONST_STRPTR)"task.resource");
      printf("task.resource: open=%d resident=%s\n",
          OpenResource((CONST_STRPTR)"task.resource")!=NULL,
          resident && resident->rt_IdString?(const char *)resident->rt_IdString:"unavailable");
    }
    puts("Timing uses DateStamp wall-clock ticks for the whole suite, not Ctrl-C latency or FPS.");
    total_start=timestamp_ticks();
    for(current_iteration=1;current_iteration<=options.repeat;++current_iteration) {
        int64_t start,end;
        assert_disposed();
        start=timestamp_ticks();
        printf("SESSION ITERATION %u/%u BEGIN require_break=%d start_ticks=%lld\n",
            current_iteration,options.repeat,options.require_break,(long long)start);
        fflush(stdout);
        run_suite(options.require_break);
        assert_disposed();
        end=timestamp_ticks();
        ++completed_iterations;
        printf("SESSION ITERATION %u/%u DISPOSED handles=%lu locks=%u pending_read=%d "
            "pending_write=%d pending_wait=%d hook=%d hook_target=%d port=%d timer=%d "
            "startup_handles=%d references=%u\n",current_iteration,options.repeat,
            (unsigned long)s.handles,s.lock_mask,s.pending_read!=NULL,s.pending_write!=NULL,
            s.pending_wait!=NULL,s.breaker.registered,s.breaker.target!=NULL,s.port!=NULL,
            s.timer!=NULL,(s.startup_input!=0)+(s.startup_file!=0),
            s.references[0]+s.references[1]+s.references[2]);
        printf("SESSION ITERATION %u/%u PASS end_ticks=%lld whole_suite_ticks=%lld ticks_per_second=%d\n",
            current_iteration,options.repeat,(long long)end,(long long)(end-start),TICKS_PER_SECOND);
        fflush(stdout);
    }
    total_end=timestamp_ticks();
    printf("SESSION COMPLETE completed=%u requested=%u total_whole_suite_ticks=%lld ticks_per_second=%d\n",
        completed_iterations,options.repeat,(long long)(total_end-total_start),TICKS_PER_SECOND);
    fflush(stdout);
    return 0;
}
