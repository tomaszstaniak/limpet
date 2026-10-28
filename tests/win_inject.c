/* Guest test tool: resize a window, or click into it, from inside the
 * guest, for machines where the emulator's pointer cannot be aimed (QEMU's
 * Raspberry Pi model has no calibrated absolute pointer).
 *     win-inject size TITLE WIDTH HEIGHT [DELAY_TICKS]
 *     win-inject click TITLE X Y [QUALIFIER] [DELAY_TICKS]
 *     win-inject menu TITLE X Y MX MY IX IY
 * TITLE is a part of the window title on the default public screen; X and
 * Y are pixels inside the window. "size" asks Intuition for the new box
 * (ChangeWindowBox), which the window gets as it gets a drag of its size
 * gadget; the drag itself is not exercised. "click" moves the pointer there
 * and presses and releases the left button through input.device, with
 * QUALIFIER (e.g. 0x0008 Ctrl, 0x0009 Ctrl+Shift) on all three events.
 * Intuition takes a button's qualifiers from the keyboard state it keeps,
 * not from the event, so Ctrl and Shift in QUALIFIER are also pressed as
 * keys around the click, as a user holding them would.
 * "menu" presses the right button at X Y inside the window, moves to MX MY
 * and then IX IY on the screen (a menu title, then its item) and releases
 * it there: a menu pick, as a user drags it.
 * Prints the window box before and after. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct Window *find(struct Screen *s,const char *title)
{
    struct Window *w;
    for(w=s->FirstWindow;w;w=w->NextWindow)
        if(w->Title && strstr((const char *)w->Title,title)) return w;
    return NULL;
}
static void send(struct IOStdReq *io,struct InputEvent *ev)
{
    io->io_Command=IND_WRITEEVENT; io->io_Data=ev; io->io_Length=sizeof *ev;
    DoIO((struct IORequest *)io);
}
static void key(struct IOStdReq *io,UWORD code,int up,UWORD qual)
{
    struct InputEvent ev;
    memset(&ev,0,sizeof ev);
    ev.ie_Class=IECLASS_RAWKEY; ev.ie_Code=code|(up?IECODE_UP_PREFIX:0); ev.ie_Qualifier=qual;
    send(io,&ev); Delay(5);
}
static int click(WORD x,WORD y,UWORD qual)
{
    struct MsgPort *port=CreateMsgPort();
    struct IOStdReq *io=port?(struct IOStdReq *)CreateIORequest(port,sizeof *io):NULL;
    struct IEPointerPixel pp;
    struct InputEvent ev;
    struct Screen *s;
    if(!io || OpenDevice((CONST_STRPTR)"input.device",0,(struct IORequest *)io,0)) {
        if(io) DeleteIORequest((struct IORequest *)io);
        if(port) DeleteMsgPort(port);
        return 20;
    }
    s=LockPubScreen(NULL);
    if(!s) {
        CloseDevice((struct IORequest *)io);
        DeleteIORequest((struct IORequest *)io); DeleteMsgPort(port);
        return 20;
    }
    if(qual&IEQUALIFIER_CONTROL) key(io,0x63,0,qual);
    if(qual&IEQUALIFIER_LSHIFT) key(io,0x60,0,qual);
    memset(&pp,0,sizeof pp); pp.iepp_Screen=s; pp.iepp_Position.X=x; pp.iepp_Position.Y=y;
    memset(&ev,0,sizeof ev);
    ev.ie_Class=IECLASS_NEWPOINTERPOS; ev.ie_SubClass=IESUBCLASS_PIXEL;
    ev.ie_EventAddress=&pp; ev.ie_Qualifier=qual;
    send(io,&ev); Delay(10);
    memset(&ev,0,sizeof ev);
    ev.ie_Class=IECLASS_RAWMOUSE; ev.ie_Code=IECODE_LBUTTON;
    ev.ie_Qualifier=qual|IEQUALIFIER_LEFTBUTTON|IEQUALIFIER_RELATIVEMOUSE;
    send(io,&ev); Delay(5);
    ev.ie_Code=IECODE_LBUTTON|IECODE_UP_PREFIX;
    ev.ie_Qualifier=qual|IEQUALIFIER_RELATIVEMOUSE;
    send(io,&ev); Delay(5);
    if(qual&IEQUALIFIER_LSHIFT) key(io,0x60,1,qual&~IEQUALIFIER_LSHIFT);
    if(qual&IEQUALIFIER_CONTROL) key(io,0x63,1,qual&~(IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));
    UnlockPubScreen(NULL,s);
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io); DeleteMsgPort(port);
    return 0;
}
static void pointer_to(struct IOStdReq *io,struct Screen *s,WORD x,WORD y,UWORD qual)
{
    struct IEPointerPixel pp;
    struct InputEvent ev;
    memset(&pp,0,sizeof pp); pp.iepp_Screen=s; pp.iepp_Position.X=x; pp.iepp_Position.Y=y;
    memset(&ev,0,sizeof ev);
    ev.ie_Class=IECLASS_NEWPOINTERPOS; ev.ie_SubClass=IESUBCLASS_PIXEL;
    ev.ie_EventAddress=&pp; ev.ie_Qualifier=qual;
    send(io,&ev); Delay(15);
}
static void right_button(struct IOStdReq *io,int up)
{
    struct InputEvent ev;
    memset(&ev,0,sizeof ev);
    ev.ie_Class=IECLASS_RAWMOUSE; ev.ie_Code=IECODE_RBUTTON|(up?IECODE_UP_PREFIX:0);
    ev.ie_Qualifier=(up?0:IEQUALIFIER_RBUTTON)|IEQUALIFIER_RELATIVEMOUSE;
    send(io,&ev); Delay(15);
}
static int menu(WORD x,WORD y,WORD mx,WORD my,WORD ix,WORD iy)
{
    struct MsgPort *port=CreateMsgPort();
    struct IOStdReq *io=port?(struct IOStdReq *)CreateIORequest(port,sizeof *io):NULL;
    struct Screen *s;
    if(!io || OpenDevice((CONST_STRPTR)"input.device",0,(struct IORequest *)io,0)) {
        if(io) DeleteIORequest((struct IORequest *)io);
        if(port) DeleteMsgPort(port);
        return 20;
    }
    s=LockPubScreen(NULL);
    if(s) {
        pointer_to(io,s,x,y,0);
        right_button(io,0);
        pointer_to(io,s,mx,my,IEQUALIFIER_RBUTTON);
        pointer_to(io,s,ix,iy,IEQUALIFIER_RBUTTON);
        right_button(io,1);
        UnlockPubScreen(NULL,s);
    }
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io); DeleteMsgPort(port);
    return s?0:20;
}
int main(int argc,char **argv)
{
    struct Screen *s;
    struct Window *w;
    WORD left=0,top=0,width=0,height=0,a,b;
    int resize,rc=0;
    ULONG lock;
    if(argc<5 || (strcmp(argv[1],"size") && strcmp(argv[1],"click") && strcmp(argv[1],"menu")) ||
       (!strcmp(argv[1],"menu") && argc<9)) {
        PutStr((CONST_STRPTR)"Usage: win-inject size TITLE W H [DELAY] | click TITLE X Y [QUAL] [DELAY]"
               " | menu TITLE X Y MX MY IX IY\n");
        return 20;
    }
    resize=!strcmp(argv[1],"size");
    a=(WORD)strtol(argv[3],NULL,0); b=(WORD)strtol(argv[4],NULL,0);
    if(strcmp(argv[1],"menu") && argc>(resize?5:6)) Delay(strtol(argv[resize?5:6],NULL,0));
    s=LockPubScreen(NULL);
    if(!s) { PutStr((CONST_STRPTR)"win-inject: no public screen\n"); return 20; }
    /* The window is found and, for "size", handed to Intuition under the
       IBase lock: while it is held the window cannot be closed, so the
       pointer is valid when ChangeWindowBox reads it. ChangeWindowBox only
       queues the change (DoASyncAction, nothing that waits on IBase), and
       Intuition checks the window again when it carries the change out
       (ResourceExisting in int_changewindowbox, mainline and ABIv11
       rom/intuition/changewindowbox.c): a window closed in between is
       skipped. Nothing that may wait, such as a console write, runs while
       the lock is held. A click uses only the coordinates read here. */
    lock=LockIBase(0);
    w=find(s,argv[2]);
    if(w) {
        left=w->LeftEdge; top=w->TopEdge; width=w->Width; height=w->Height;
        if(resize) ChangeWindowBox(w,left,top,a,b);
    }
    UnlockIBase(lock);
    if(!w) { printf("win-inject: no window with \"%s\"\n",argv[2]); UnlockPubScreen(NULL,s); return 10; }
    printf("before %d,%d %dx%d\n",left,top,width,height);
    if(!strcmp(argv[1],"menu"))
        rc=menu(left+a,top+b,(WORD)strtol(argv[5],NULL,0),(WORD)strtol(argv[6],NULL,0),
                (WORD)strtol(argv[7],NULL,0),(WORD)strtol(argv[8],NULL,0));
    else if(!resize) rc=click(left+a,top+b,argc>5?(UWORD)strtoul(argv[5],NULL,0):0);
    Delay(25);
    lock=LockIBase(0);
    if(find(s,argv[2])==w) { left=w->LeftEdge; top=w->TopEdge; width=w->Width; height=w->Height; }
    else w=NULL;
    UnlockIBase(lock);
    if(w) printf("after %d,%d %dx%d\n",left,top,width,height);
    UnlockPubScreen(NULL,s);
    return rc;
}
