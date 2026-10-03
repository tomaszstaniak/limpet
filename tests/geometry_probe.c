/* Native raw-stream geometry probe. No console.device pointer dereferences. */
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
static int query(int native,unsigned *rows,unsigned *columns)
{
    const char *request=native?"\033[0 q":"\033[18t";
    char response[64]; size_t n=0; int consumed=0;
    if(Write(Output(),(APTR)request,strlen(request))!=(LONG)strlen(request)) return 0;
    while(n<sizeof response-1) {
        if(!WaitForChar(Input(),2000000) || Read(Input(),response+n,1)!=1) return 0;
        if(response[n++]==(native?'r':'t')) break;
    }
    response[n]=0;
    return sscanf(response,native?"\2331;1;%u;%u r%n":"\033[8;%u;%ut%n",rows,columns,&consumed)==2
        && consumed==(int)n && *rows && *columns;
}
int main(int argc,char **argv)
{
    int native_only=argc==2 && !strcmp(argv[1],"--native");
    int result=20;
    unsigned rows,columns;
    unsigned char key;
    if(!IsInteractive(Input()) || !SetMode(Input(),1)) return 20;
    for(;;) {
        if(!query(native_only,&rows,&columns)) { printf("\r\nGeometry query FAIL/timeout\r\n"); break; }
        printf("\r\nGeometry PASS: rows=%u columns=%u (%s)\r\n",rows,columns,
            native_only?"native reference":"VT");
        printf("Resize, then press r to query again; q returns to Shell.\r\n");
        fflush(stdout);
        do {
            if(Read(Input(),&key,1)!=1) goto done;
        } while(key!='r' && key!='q' && key!=3);
        if(key!='r') { result=0; break; }
    }
done:
    printf("\r\n"); fflush(stdout);
    if(!SetMode(Input(),0)) result=20;
    return result;
}
