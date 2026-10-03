/* Reuse production keyboard admission/dispatch and actual DOS sessions.
 * This does not fabricate IntuiMessages or claim physical keyboard latency. */
#define main xterm_preview_main
#include "../src/aros_xterm.c"
#undef main
static char received[32768];
static size_t received_length;
static void pump(void)
{
    unsigned char bytes[4096]; size_t n;
    aros_session_poll(&process_session);
    n=aros_session_read(&process_session,bytes,sizeof bytes);
    if(n) {
        screen_write(&screen,bytes,n);
        if(n<sizeof received-received_length) {
            memcpy(received+received_length,bytes,n); received_length+=n;
            received[received_length]=0;
        }
    }
    key_queue_drain(&keyboard,16,keyboard_deliver,NULL);
}
static int await_text(const char *needle)
{
    unsigned i;
    for(i=0;i<500;++i) {
        pump();
        if(strstr(received,needle)) return 1;
        Delay(1);
    }
    return 0;
}
static void capture_reset(void) { received_length=0; received[0]=0; }
static int await_ready(void)
{
    unsigned i;
    for(i=0;i<500;++i) {
        pump(); if(editor_ready() && !keyboard.count) return 1; Delay(1);
    }
    return 0;
}
static void type_keys(const char *text)
{
    while(*text) { keyboard_submit(0,0,(const unsigned char *)text,1); ++text; }
}
#define REQUIRE(x) do { if(!(x)) { printf("KEYBOARD FAIL line=%u: %s\n%s\n",__LINE__,#x,received); failed=1; goto done; } } while(0)
int main(void)
{
    int failed=0; unsigned i;
    echo_mode=1; type_keys("X");
    clip_job.task_id=1; REQUIRE(!keyboard_ready()); clip_job.task_id=0;
    paste.length=1; REQUIRE(!keyboard_ready()); paste.length=0;
    REQUIRE(keyboard_ready()); key_queue_clear(&keyboard);
    echo_mode=2; shell_session=1;
    screen_init(&screen,cells,MAX_ROWS*MAX_COLUMNS,24,80);
    REQUIRE(!aros_session_start_shell_profile(&process_session,""));
    type_keys("\003");
    REQUIRE(process_session.breaker.registered);
    REQUIRE(!aros_break_ready(&process_session.breaker));
    REQUIRE(!keyboard_ready()); /* No bound target: sleep, do not spin. */
    key_queue_clear(&keyboard);
    REQUIRE(await_text("> "));
    REQUIRE(await_ready());
    for(i=0;i<10;++i) {
        unsigned j;
        REQUIRE(editor_ready());
        REQUIRE(session_bytes((const unsigned char *)"Wait 1 SECS\r",12)==12);
        for(j=0;j<10;++j) { pump(); Delay(1); }
        REQUIRE(!editor_ready()); capture_reset();
        type_keys("Echo KEYBOARD_OX");
        keyboard_submit(RAWKEY_LEFT,0,(const unsigned char *)"\033[D",3);
        keyboard_submit(RAWKEY_DELETE,0,(const unsigned char *)"\033[3~",4);
        type_keys("K\r");
        REQUIRE(!key_queue_drain(&keyboard,16,keyboard_deliver,NULL));
        REQUIRE(keyboard.count==20);
        /* GUI editor echoes the command/newline to the model, not this DOS
         * capture; only the child's actual Echo output belongs here. */
        REQUIRE(await_text("KEYBOARD_OK\r\n"));
        REQUIRE(await_text("> "));
        REQUIRE(await_ready());
        printf("KEYBOARD handoff %u/10 PASS\n",i+1); fflush(stdout);
    }
    REQUIRE(session_bytes((const unsigned char *)"Wait 30 SECS\r",13)==13);
    for(i=0;i<10;++i) { pump(); Delay(1); }
    REQUIRE(!editor_ready()); capture_reset();
    type_keys("Echo NEVER_RUN\r");
    type_keys("\003");
    REQUIRE(keyboard.count==1);
    /* The asynchronous clipboard task is an external readiness dependency.
     * Model a still-pending reader without starting or destroying a worker. */
    clip_job.task_id=1;
    REQUIRE(keyboard_ready());
    REQUIRE(key_queue_drain(&keyboard,16,keyboard_deliver,NULL)==1);
    clip_job.task_id=0;
    REQUIRE(await_text("> "));
    REQUIRE(!strstr(received,"NEVER_RUN"));
    REQUIRE(await_ready());
    capture_reset(); type_keys("Echo AFTER_INTERRUPT\r");
    REQUIRE(await_text("AFTER_INTERRUPT\r\n"));
    puts("KEYBOARD interrupt priority and clipboard barrier PASS");
    REQUIRE(await_ready());
    { char command[600];
      REQUIRE(!aros_session_peer_command(peer_command,sizeof peer_command));
      snprintf(command,sizeof command,"%s --console-check\r",peer_command);
      capture_reset(); type_keys(command);
    }
    REQUIRE(await_text("COOKED READY"));
    REQUIRE(await_ready());
    type_keys("abc"); keyboard_submit(RAWKEY_BACKSPACE,0,(const unsigned char *)"\b",1);
    type_keys("D\r");
    REQUIRE(await_text("RAW READY"));
    keyboard_submit(RAWKEY_UP,0,(const unsigned char *)"\033[A",3);
    REQUIRE(await_text("RAW CTRL-C READY"));
    type_keys("\003");
    REQUIRE(await_text("CONSOLE PASS"));
    puts("KEYBOARD cooked/raw navigation and raw Ctrl-C byte PASS");
done:
    clip_job.task_id=0; key_queue_clear(&keyboard);
    aros_session_stop(&process_session);
    while(aros_session_active(&process_session)) { aros_session_poll(&process_session); Delay(1); }
    while(aros_session_dispose(&process_session)) { aros_session_poll(&process_session); Delay(1); }
    printf("KEYBOARD COMPLETE result=%s\n",failed?"FAIL":"PASS");
    return failed?20:0;
}
