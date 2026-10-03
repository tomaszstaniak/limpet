/* Native acceptance helper: steal focus once while XTerm owns a left drag. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/input.h>
#include <devices/inputevent.h>
#include <intuition/intuitionbase.h>
#include <string.h>
struct Device *InputBase;
int main(void)
{
    struct MsgPort *port=CreateMsgPort();
    struct IOStdReq *io=port?(struct IOStdReq *)CreateIORequest(port,sizeof *io):NULL;
    struct Window *window=NULL;
    unsigned tick; int opened=0,activated=0;
    if(!io || OpenDevice((CONST_STRPTR)"input.device",0,(struct IORequest *)io,0)) goto done;
    opened=1; InputBase=io->io_Device;
    window=OpenWindowTags(NULL,WA_Title,(IPTR)"Focus-loss test",WA_Left,5,WA_Top,30,
        WA_Width,230,WA_Height,60,WA_Flags,WFLG_DRAGBAR|WFLG_DEPTHGADGET,TAG_DONE);
    if(!window) goto done;
    for(tick=0;tick<6000;++tick) {
        int target;
        ULONG key=LockIBase(0);
        struct Window *active=IntuitionBase->ActiveWindow;
        target=active && active!=window && active->Title &&
            !strncmp((const char *)active->Title,"XTerm",5);
        UnlockIBase(key);
        if(target && (PeekQualifier()&IEQUALIFIER_LEFTBUTTON)) {
            Delay(10); WindowToFront(window); ActivateWindow(window); activated=1;
            Delay(1000); break;
        }
        Delay(1);
    }
done:
    if(window) CloseWindow(window);
    if(opened) CloseDevice((struct IORequest *)io);
    if(io) DeleteIORequest((struct IORequest *)io);
    if(port) DeleteMsgPort(port);
    return activated?0:20;
}
