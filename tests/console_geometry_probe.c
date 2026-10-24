/* Run as a Limpet --command, or pass an explicit mounted console name.
 * Usage: console-geometry-probe LOG [ENDPOINT]
 * Never reads handler-owned pointers or consumes console input. */
#include "../src/console_geometry.h"
#include "../src/tty_control.h"
#include <proto/dos.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>

int main(int argc,char **argv)
{
    BPTR handle;
    struct FileHandle *fh;
    struct { struct ConsoleGeometry geometry; unsigned char guard[8]; } block,before;
    XtyControl private_query;
    FILE *log;
    LONG result,error;
    unsigned i;
    int failures=0;
    const struct { const char *name; SIPTR capacity,version; int null_buffer; } bad[] = {
        {"short",23,1,0},{"negative",-1,1,0},{"version-zero",24,0,0},
        {"version-new",24,2,0},{"null",24,1,1}
    };
    if(argc<2 || argc>3) return 20;
    log=fopen(argv[1],"w");if(!log) return 20;
    handle=argc==3?Open((CONST_STRPTR)argv[2],MODE_OLDFILE):Output();
    if(!handle) { fprintf(log,"FAIL open endpoint error=%ld\n",(long)IoErr());fclose(log);return 20; }
    fh=BADDR(handle);
    memset(&block,0xa5,sizeof block);
    result=DoPkt(fh->fh_Type,ACTION_CONSOLE_GETGEOMETRY,fh->fh_Arg1,
                 (SIPTR)&block,sizeof block,CONSOLE_GEOMETRY_VERSION,0);
    error=IoErr();
    if(result!=DOSTRUE || error || block.geometry.cg_Size!=24 || block.geometry.cg_Version!=1 ||
       !block.geometry.cg_Columns || !block.geometry.cg_Rows ||
       !block.geometry.cg_Width || !block.geometry.cg_Height ||
       block.geometry.cg_Width%block.geometry.cg_Columns ||
       block.geometry.cg_Height%block.geometry.cg_Rows) ++failures;
    for(i=0;i<sizeof block.guard;++i) if(block.guard[i]!=0xa5) ++failures;
    fprintf(log,"%s snapshot result=%ld error=%ld columns=%lu rows=%lu width=%lu height=%lu\n",
        failures?"FAIL":"PASS",(long)result,(long)error,
        (unsigned long)block.geometry.cg_Columns,(unsigned long)block.geometry.cg_Rows,
        (unsigned long)block.geometry.cg_Width,(unsigned long)block.geometry.cg_Height);
    memset(&private_query,0,sizeof private_query);
    private_query.version=XTY_CONTROL_VERSION;private_query.operation=XTY_GET_PRESENTATION;
    result=DoPkt(fh->fh_Type,XTY_ACTION_CONTROL,fh->fh_Arg1,(SIPTR)&private_query,
                 sizeof private_query,XTY_PACKET_MAGIC,0);
    if(result!=DOSTRUE || private_query.values[4]!=block.geometry.cg_Rows ||
       private_query.values[5]!=block.geometry.cg_Columns) {
        fprintf(log,"FAIL public/private grid agreement\n");++failures;
    } else fprintf(log,"PASS public/private grid agreement\n");
    for(i=0;i<sizeof bad/sizeof bad[0];++i) {
        int ok;
        memset(&block,0x5a,sizeof block);before=block;
        result=DoPkt(fh->fh_Type,ACTION_CONSOLE_GETGEOMETRY,fh->fh_Arg1,
                     bad[i].null_buffer?0:(SIPTR)&block,bad[i].capacity,bad[i].version,0);
        error=IoErr();
        ok=result==DOSFALSE && error==ERROR_BAD_NUMBER && !memcmp(&block,&before,sizeof block);
        fprintf(log,"%s %s result=%ld error=%ld unchanged=%d\n",ok?"PASS":"FAIL",
            bad[i].name,(long)result,(long)error,!memcmp(&block,&before,sizeof block));
        if(!ok) ++failures;
    }
    before=block;
    result=DoPkt(fh->fh_Type,ACTION_CONSOLE_GETGEOMETRY,0,(SIPTR)&block,sizeof block,1,0);
    error=IoErr();
    if(result!=DOSFALSE || error!=ERROR_INVALID_LOCK || memcmp(&block,&before,sizeof block)) {
        fprintf(log,"FAIL invalid handle result=%ld error=%ld\n",(long)result,(long)error);++failures;
    } else fprintf(log,"PASS invalid handle\n");
    if(argc==3) Close(handle);
    fprintf(log,"failures=%d\n",failures);
    if(fclose(log)) return 20;
    return failures?10:0;
}
