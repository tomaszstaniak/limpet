/* Run only as a foreground child in XTerm's cooked Shell. Results go to RAM:
 * so protocol traffic and the diagnostic log cannot contaminate one another. */
#include <proto/dos.h>
#include <dos/dos.h>
#include <string.h>

static int output(const char *p)
{ LONG n=(LONG)strlen(p); return Write(Output(),(APTR)p,n)==n; }
static int query(const char *request,const char *expected)
{
    unsigned i;
    if(!output(request)) return 0;
    for(i=0;expected[i];++i) {
        unsigned char ch;
        if(!WaitForChar(Input(),2000000) || Read(Input(),&ch,1)!=1 ||
           ch!=(unsigned char)expected[i]) return 0;
    }
    return 1;
}
int main(void)
{
    BPTR log=Open((CONST_STRPTR)"RAM:vt-replies.log",MODE_NEWFILE);
    int ok,restored;
    const char *result;
    if(!log) return 20;
    if(!IsInteractive(Input()) || !IsInteractive(Output()) || !SetMode(Input(),1)) {
        Write(log,"VT REPLIES FAIL: raw mode\n",26); Close(log); return 20;
    }
    ok=output("\033[?1049h\033[2;3H\033[4d\033[6G") &&
       query("\033[5n","\033[0n") &&
       query("\033[6n","\033[4;6R") &&
       query("\033[?6n","\033[?4;6R") &&
       output("\033[E\033[F") && query("\033[6n","\033[4;1R");
    if(!output("\033[?1049l")) ok=0;
    restored=SetMode(Input(),0)!=0;
    result=ok && restored ? "VT REPLIES PASS: DSR CPR DECXCPR motion raw-to-cooked\n" :
                           "VT REPLIES FAIL: reply or mode restoration\n";
    if(Write(log,(APTR)result,(LONG)strlen(result))!=(LONG)strlen(result)) ok=0;
    Close(log);
    output(ok && restored?"VT_REPLY_RETURNED_OK\n":"VT_REPLY_FAILED\n");
    return ok && restored?0:20;
}
