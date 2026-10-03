#include "aros_clipboard.h"
#include <devices/clipboard.h>
#include <proto/exec.h>
#include <stdlib.h>
#include <string.h>

static struct IOClipReq *open_clip(unsigned unit,struct MsgPort **port)
{
    struct IOClipReq *request;
    *port=CreateMsgPort();
    if(!*port) return NULL;
    request=(struct IOClipReq *)CreateIORequest(*port,sizeof *request);
    if(!request || OpenDevice((CONST_STRPTR)"clipboard.device",unit,(struct IORequest *)request,0)) {
        if(request) DeleteIORequest((struct IORequest *)request);
        DeleteMsgPort(*port); *port=NULL; return NULL;
    }
    return request;
}

static void close_clip(struct IOClipReq *request,struct MsgPort *port)
{
    CloseDevice((struct IORequest *)request);
    DeleteIORequest((struct IORequest *)request); DeleteMsgPort(port);
}

int aros_clip_read(unsigned unit,unsigned char *bytes,size_t cap,size_t *length)
{
    struct MsgPort *port; struct IOClipReq *request;
    unsigned char *temporary; size_t count; int result;
    if(!length || (!bytes && cap) || cap>AROS_CLIP_LIMIT) return -1;
    *length=0;
    temporary=malloc(cap+1);
    if(!temporary) return -1;
    request=open_clip(unit,&port);
    if(!request) { free(temporary); return -1; }
    request->io_Command=CMD_READ; request->io_Data=(STRPTR)temporary;
    request->io_Length=cap+1; request->io_Offset=0; request->io_ClipID=0;
    result=DoIO((struct IORequest *)request)?-1:0;
    count=request->io_Actual;
    /* The mainline driver releases its read lock only on a read past EOF.
     * Skip to the maximum offset; io_Offset is a 32-bit ULONG, so this is
     * >= every representable clip size and requires no unbounded drain loop. */
    if(request->io_ClipID>=0) {
        request->io_Command=CMD_READ; request->io_Data=NULL;
        request->io_Offset=0xffffffffUL; request->io_Length=1;
        if(DoIO((struct IORequest *)request)) result=-1;
    }
    close_clip(request,port);
    if(!result && count>cap) result=-2;
    if(!result) { if(count) memcpy(bytes,temporary,count); *length=count; }
    free(temporary); return result;
}

int aros_clip_write(unsigned unit,const unsigned char *bytes,size_t length)
{
    struct MsgPort *port; struct IOClipReq *request; int result;
    if((!bytes && length) || length>AROS_CLIP_LIMIT) return -1;
    request=open_clip(unit,&port);
    if(!request) return -1;
    request->io_Command=CMD_WRITE; request->io_Data=(STRPTR)bytes;
    request->io_Length=length; request->io_Offset=0; request->io_ClipID=0;
    result=DoIO((struct IORequest *)request)?-1:0;
    if(request->io_Actual!=length) result=-1;
    if(request->io_ClipID>=0) {
        request->io_Command=CMD_UPDATE;
        if(DoIO((struct IORequest *)request)) result=-1;
    }
    close_clip(request,port); return result;
}
