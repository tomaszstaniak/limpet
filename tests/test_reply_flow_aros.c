/* Production GUI pump + real DOS packet endpoint; no fabricated GUI events. */
#define main xterm_preview_main
#include "../src/aros_xterm.c"
#undef main
static struct MsgPort *port;
static struct DosPacket *packet;
static struct FileHandle *handle,*output_handle;
static int queued,failed;
static unsigned char fill[XSESSION_CAPACITY],received[XSESSION_CAPACITY];
static SIPTR call(LONG action,IPTR a,IPTR b,IPTR c)
{
    unsigned tick;
    packet->dp_Type=action; packet->dp_Arg1=a; packet->dp_Arg2=b; packet->dp_Arg3=c;
    queued=1; SendPkt(packet,process_session.port,port);
    for(tick=0;tick<500;++tick) {
        aros_session_poll(&process_session);
        if(GetMsg(port)) { queued=0; return packet->dp_Res1; }
        Delay(1);
    }
    return -1;
}
#define CHECK(x) do { if(!(x)) { printf("REPLY FLOW FAIL line %u: %s\n",__LINE__,#x); failed=1; goto done; } } while(0)
int main(void)
{
    const unsigned char stream[]="\033[6nX\033[6nY";
    unsigned i;
    port=CreateMsgPort(); packet=AllocDosObject(DOS_STDPKT,NULL);
    handle=AllocDosObject(DOS_FILEHANDLE,NULL);
    output_handle=AllocDosObject(DOS_FILEHANDLE,NULL);
    CHECK(port && packet && handle && output_handle);
    CHECK(!aros_session_start_endpoint(&process_session));
    CHECK(call(ACTION_FINDINPUT,(IPTR)MKBADDR(handle),0,0)==DOSTRUE);
    CHECK(call(ACTION_FINDOUTPUT,(IPTR)MKBADDR(output_handle),0,0)==DOSTRUE);
    echo_mode=2; shell_session=1;
    memset(fill,'!',sizeof fill);
    for(i=0;i<10;++i) {
        output_reset(); key_queue_clear(&keyboard);
        CHECK(!screen_init(&screen,cells,MAX_ROWS*MAX_COLUMNS,4,10));
        CHECK(call(ACTION_SCREEN_MODE,1,0,0)==DOSTRUE);
        CHECK(aros_session_write(&process_session,fill,sizeof fill)==sizeof fill);
        CHECK(call(ACTION_WRITE,1,(IPTR)stream,sizeof stream-1)==sizeof stream-1);
        CHECK(output_pump()==4 && reply_flow.length==6);
        CHECK(!output_ready() && output_offset<output_length);
        keyboard_submit(0,0,(const unsigned char *)"K",1);
        CHECK(!keyboard_ready());
        CHECK(!mouse_write(&process_session,(const unsigned char *)"M",1));
        CHECK(!session_bytes((const unsigned char *)"P",1));
        CHECK(call(ACTION_READ,1,(IPTR)received,sizeof received)==sizeof received);
        CHECK(!memcmp(received,fill,sizeof fill));
        CHECK(output_ready()); CHECK(output_pump()==sizeof stream-1-4);
        CHECK(!reply_flow.length && output_offset==output_length);
        CHECK(keyboard_ready());
        CHECK(key_queue_drain(&keyboard,1,keyboard_deliver,NULL)==1);
        CHECK(call(ACTION_READ,1,(IPTR)received,13)==13);
        CHECK(!memcmp(received,"\033[1;1R\033[1;2RK",13));
        CHECK(screen_cell(&screen,0,0)=='X' && screen_cell(&screen,0,1)=='Y');
        printf("REPLY FLOW saturation %u/10 PASS\n",i+1);
    }
    CHECK(aros_session_write(&process_session,fill,sizeof fill)==sizeof fill);
    CHECK(call(ACTION_WRITE,1,(IPTR)stream,sizeof stream-1)==sizeof stream-1);
    CHECK(output_pump()==4 && reply_flow.length);
    CHECK(call(ACTION_SCREEN_MODE,0,0,0)==DOSTRUE);
    replies_sync(); CHECK(!reply_flow.length && output_ready());
    CHECK(output_pump()==sizeof stream-1-4);
    CHECK(call(ACTION_READ,1,(IPTR)received,sizeof received)==sizeof received);
    CHECK(!memcmp(received,fill,sizeof fill));
    CHECK(!process_session.input.length);
    CHECK(call(ACTION_SCREEN_MODE,1,0,0)==DOSTRUE);
    CHECK(aros_session_write(&process_session,fill,sizeof fill)==sizeof fill);
    CHECK(call(ACTION_WRITE,1,(IPTR)stream,sizeof stream-1)==sizeof stream-1);
    CHECK(output_pump()==4 && reply_flow.length);
    /* Keep output open. No GUI synchronization between close and reopen. */
    CHECK(call(ACTION_END,1,0,0)==DOSTRUE);
    CHECK(call(ACTION_FINDINPUT,(IPTR)MKBADDR(handle),0,0)==DOSTRUE);
    replies_sync(); CHECK(!reply_flow.length);
    CHECK(output_pump()==sizeof stream-1-4);
    CHECK(!reply_flow.length); /* Old buffered queries cannot target the new reader. */
    CHECK(call(ACTION_READ,1,(IPTR)received,sizeof received)==sizeof received);
    CHECK(!memcmp(received,fill,sizeof fill));
    puts("REPLY FLOW input close/reopen epoch PASS");
    {
        const unsigned char split[]="\033[6n\033[6";
        CHECK(aros_session_write(&process_session,fill,sizeof fill)==sizeof fill);
        CHECK(call(ACTION_WRITE,1,(IPTR)split,sizeof split-1)==sizeof split-1);
        CHECK(output_pump()==4 && reply_flow.length);
        CHECK(call(ACTION_END,1,0,0)==DOSTRUE);
        CHECK(call(ACTION_FINDINPUT,(IPTR)MKBADDR(handle),0,0)==DOSTRUE);
        CHECK(output_pump()==3 && !reply_flow.length);
        CHECK(call(ACTION_READ,1,(IPTR)received,sizeof received)==sizeof received);
        CHECK(call(ACTION_WRITE,1,(IPTR)"nZ",2)==2);
        CHECK(output_pump()==2 && !reply_flow.length && !process_session.input.length);
        /* Also invalidate a query begun while replies WERE enabled. */
        CHECK(call(ACTION_WRITE,1,(IPTR)"\033[6",3)==3);
        CHECK(output_pump()==3);
        CHECK(call(ACTION_END,1,0,0)==DOSTRUE);
        CHECK(call(ACTION_FINDINPUT,(IPTR)MKBADDR(handle),0,0)==DOSTRUE);
        CHECK(call(ACTION_WRITE,1,(IPTR)"n",1)==1);
        CHECK(output_pump()==1 && !process_session.input.length);
        puts("REPLY FLOW fragmented-query ownership PASS");
    }
    CHECK(aros_session_write(&process_session,fill,sizeof fill)==sizeof fill);
    CHECK(call(ACTION_WRITE,1,(IPTR)stream,sizeof stream-1)==sizeof stream-1);
    CHECK(output_pump()==4 && reply_flow.length);
    aros_session_eof(&process_session); replies_sync();
    CHECK(!reply_flow.length && output_ready());
    CHECK(output_pump()==sizeof stream-1-4);
    puts("REPLY FLOW cooked transition and EOF cancellation PASS");
done:
    /* No external producers. Retain all borrowed packet storage until replied. */
    aros_session_stop(&process_session);
    while(queued) {
        aros_session_poll(&process_session);
        if(GetMsg(port)) queued=0; else Delay(1);
    }
    for(i=1;i<=2;++i) if((process_session.handles&(1u<<(i-1))) && call(ACTION_END,i,0,0)!=DOSTRUE) {
        puts("REPLY FLOW cleanup blocked; retaining endpoint");
        for(;;) { aros_session_poll(&process_session); Delay(50); }
    }
    if(aros_session_dispose(&process_session)) {
        puts("REPLY FLOW disposal blocked; retaining endpoint");
        for(;;) { aros_session_poll(&process_session); Delay(50); }
    }
    if(handle) FreeDosObject(DOS_FILEHANDLE,handle);
    if(output_handle) FreeDosObject(DOS_FILEHANDLE,output_handle);
    if(packet) FreeDosObject(DOS_STDPKT,packet);
    if(port) DeleteMsgPort(port);
    puts(failed?"REPLY FLOW COMPLETE FAIL":"REPLY FLOW COMPLETE PASS");
    return failed?20:0;
}
