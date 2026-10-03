#include "../src/aros_session.h"
#include "../src/session_command.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
static ArosSession session;
static char output[32768];
static size_t length;
extern int aros_session_test_fail_shell_launch;
static int until(const char *needle,int exited)
{
    unsigned ticks;
    for(ticks=0;ticks<500;ticks++) {
        aros_session_poll(&session);
        length+=aros_session_read(&session,(unsigned char *)output+length,sizeof output-1-length);
        output[length]=0;
        /* Do not send the next command while the previous one is still
         * producing output: wait for the Shell to request input again. */
        if((!needle || strstr(output,needle)) &&
           (exited?session.exited:session.pending_read!=NULL)) return 1;
        Delay(1);
    }
    printf("deadline: %s\n%s\n",needle?needle:"exit",output); return 0;
}
static int send(const char *text)
{ length=0; output[0]=0; return aros_session_write(&session,(const unsigned char *)text,strlen(text))==strlen(text); }
static void dispose(void)
{
    aros_session_stop(&session);
    while(session.task_id || session.handles) { aros_session_poll(&session); Delay(1); }
    while(aros_session_dispose(&session)) { aros_session_poll(&session); Delay(1); }
    length=0; output[0]=0;
}
#define CHECK(e) do { if(!(e)) { printf("FAIL startup line %d: %s (IoErr=%ld)\n",__LINE__,#e,(long)IoErr()); goto cleanup; } } while(0)
int main(int argc,char **argv)
{
    static const char script[]="Alias ndg \"Echo PROFILE_ALIAS_OK []\"\nSet XTProfile active\nPrompt \"PROFILE> \"\nEcho STARTUP_RAN\n";
    char dir[128],path[180],missing[180];
    struct Process *me=(struct Process *)FindTask(NULL);
    BPTR lock=BNULL,fh=BNULL;
    unsigned attempt,round;
    int made=0,filemade=0,result=20;
    if(argc==2 && !strcmp(argv[1],"--return-minus-one")) return -1;
    for(attempt=0;attempt<100;attempt++) {
        snprintf(dir,sizeof dir,"RAM:xt-profile-%lu-%u",(unsigned long)me->pr_TaskNum,attempt);
        lock=CreateDir((CONST_STRPTR)dir);
        if(lock) { made=1; break; }
        CHECK(IoErr()==ERROR_OBJECT_EXISTS || IoErr()==ERROR_OBJECT_IN_USE);
    }
    CHECK(lock); UnLock(lock); lock=BNULL;
    snprintf(path,sizeof path,"%s/Startup Test",dir);
    snprintf(missing,sizeof missing,"%s/Missing",dir);
    fh=Open((CONST_STRPTR)path,MODE_NEWFILE); CHECK(fh); filemade=1;
    CHECK(Write(fh,script,sizeof script-1)==sizeof script-1); Close(fh); fh=BNULL;
    CHECK(aros_session_start_shell_profile(&session,missing)==-1);
    CHECK(!session.port && !session.task_id);
    CHECK(aros_session_start_shell_profile(&session,dir)==-1);
    CHECK(!session.port && !session.task_id);
    for(round=0;round<2;round++) {
        CHECK(!aros_session_start_shell_profile(&session,path));
        CHECK(until("PROFILE> ",0)); CHECK(strstr(output,"STARTUP_RAN"));
        CHECK(send("ndg argument\r")); CHECK(until("\r\nPROFILE_ALIAS_OK argument\r\n",0));
        CHECK(send("Echo $XTProfile\r")); CHECK(until("\r\nactive\r\n",0));
        CHECK(send("EndCLI\r")); CHECK(until(NULL,1));
        CHECK(!session.result && !session.unsupported_action);
        CHECK(session.startup_closes==1 && !session.startup_file && !session.startup_input);
        dispose();
    }
    /* A negative completed command is not a Shell startup failure. */
    {
        char command[640],program[256],quoted[520];
        CHECK(GetProgramName((STRPTR)program,sizeof program));
        CHECK(NameFromLock(GetProgramDir(),(STRPTR)command,sizeof command));
        CHECK(AddPart((STRPTR)command,(CONST_STRPTR)FilePart((CONST_STRPTR)program),sizeof command));
        CHECK(!session_quote_path(command,quoted,sizeof quoted));
        snprintf(command,sizeof command,"%s --return-minus-one\r",quoted);
        CHECK(!aros_session_start_shell_profile(&session,path));
        CHECK(until("PROFILE> ",0)); CHECK(send(command)); CHECK(until("PROFILE> ",0));
        aros_session_eof(&session); CHECK(until(NULL,1));
        CHECK(session.result==-1 && session.startup_closes==1); dispose();
    }
    aros_session_test_fail_shell_launch=1;
    CHECK(!aros_session_start_shell_profile(&session,path));
    CHECK(until(NULL,1));
    CHECK(session.result==-1 && session.startup_closes==1);
    dispose(); aros_session_test_fail_shell_launch=0;
    fh=Open((CONST_STRPTR)path,MODE_NEWFILE); CHECK(fh);
    CHECK(Write(fh,"EndCLI\n",7)==7); Close(fh); fh=BNULL;
    CHECK(!aros_session_start_shell_profile(&session,path));
    CHECK(until(NULL,1)); CHECK(session.startup_closes==1); dispose();
    CHECK(!aros_session_start_shell_profile(&session,""));
    CHECK(until("> ",0)); CHECK(!strstr(output,"STARTUP_RAN"));
    CHECK(send("Echo CLEAN_SESSION\r")); CHECK(until("\r\nCLEAN_SESSION\r\n",0));
    CHECK(send("EndCLI\r")); CHECK(until(NULL,1)); dispose();
    result=0;
cleanup:
    if(fh) Close(fh);
    dispose();
    if(lock) UnLock(lock);
    if(filemade && !DeleteFile((CONST_STRPTR)path)) result=20;
    if(made && !DeleteFile((CONST_STRPTR)dir)) result=20;
    if(!result) puts("Shell startup PASS: aliases, variables, restart, clean, missing, exactly-once close (EOF, EndCLI, -1, launch failure)");
    return result;
}
