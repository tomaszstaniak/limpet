/* Native acceptance driver for the isolated server in README.md.
 * The server's forced command "bulk" emits 8192 exact lines on each stream.
 * No credentials or network configuration are created by this test. */
#include <proto/dos.h>
#include <dos/dostags.h>
#include <stdio.h>
#include <string.h>

static int verify(const char *path,const char *line,const char *tail)
{
    BPTR file=Open((CONST_STRPTR)path,MODE_OLDFILE);
    char data[33],end[32];
    unsigned i;
    int ok=0;
    if(!file) return 0;
    for(i=0;i<8192;++i)
        if(Read(file,data,sizeof data)!=sizeof data || memcmp(data,line,sizeof data)) goto done;
    if(Read(file,end,(LONG)strlen(tail))!=(LONG)strlen(tail) ||
       memcmp(end,tail,strlen(tail)) || Read(file,end,1)!=0) goto done;
    ok=1;
done:
    Close(file); return ok;
}
int main(int argc,char **argv)
{
    const char *out="RAM:xterm-ssh/bulk-out",*err="RAM:xterm-ssh/bulk-err";
    BPTR input,output,error;
    LONG rc=-1;
    int ok=0;
    if(argc!=2) {
        puts("Usage: check-transfer \"ssh-path -p PORT -l USER HOST bulk\"");
        return 20;
    }
    input=Open((CONST_STRPTR)"NIL:",MODE_OLDFILE);
    output=Open((CONST_STRPTR)out,MODE_NEWFILE);
    error=Open((CONST_STRPTR)err,MODE_NEWFILE);
    if(input && output && error)
        rc=SystemTags((CONST_STRPTR)argv[1],
            SYS_Input,input,SYS_Output,output,SYS_Error,error,
            SYS_Asynch,FALSE,NP_StackSize,262144,TAG_DONE);
    if(input) Close(input);
    if(output) Close(output);
    if(error) Close(error);
    if(rc==0) ok=verify(out,"0123456789abcdef0123456789abcdef\n","STDOUT_DONE\n") &&
                 verify(err,"fedcba9876543210fedcba9876543210\n","STDERR_DONE\n");
    printf("SSH bulk %s: rc=%ld, 270347 exact bytes per stream\n",ok?"PASS":"FAIL",(long)rc);
    return ok?0:20;
}
