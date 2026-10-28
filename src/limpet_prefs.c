/* Limpet Preferences: what a new Shell window runs once it has read
 * S:Shell-Startup. A Zune program of its own, started from a Limpet
 * window's Project > Preferences (or from a Shell). It writes Session.conf
 * (settings.h, settings_edit.h): Use ENV:Limpet/Session.conf, Save the one
 * in ENVARC: as well, each edited from its own text; Cancel changes
 * nothing. Run now asks the window it was opened from to run the line at
 * its Shell prompt (prefs_port.h). */
#include "settings.h"
#include "settings_edit.h"
#include "session_start.h"
#include "session_command.h"
#include "prefs_port.h"
#include <exec/memory.h>
#include <dos/dos.h>
#include <libraries/mui.h>
#include <libraries/asl.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/asl.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>

#ifndef LIMPET_VERSION
#define LIMPET_VERSION "0.0.0-dev"
#endif
#ifndef LIMPET_DATE
#define LIMPET_DATE "01.01.2026"
#endif
static const char version_tag[] __attribute__((used))="$VER: LimpetPrefs " LIMPET_VERSION " (" LIMPET_DATE ")";

#define SESSION_FILE "ENV:Limpet/Session.conf"
#define KEPT_FILE "ENVARC:Limpet/Session.conf"

enum { ID_CHANGED=1, ID_SAVE, ID_USE, ID_CANCEL, ID_RUN, ID_CHOOSE };

static char session_text[XSETTINGS_FILE_MAX+1],kept_text[XSETTINGS_FILE_MAX+1];
static size_t session_length,kept_length;
static XSettings loaded;
static char status[160];
static char line_text[600];

static Object *app,*win,*start_cycle,*pages,*program,*look_cycle,*config_pop,*config,
    *show[XFF_GROUPS],*logo_cycle,*command,*choose,*line_view,*run,*status_view;

/* 1 with the file's text, 0 without the file, -1 when it is too large. */
static int read_file(const char *name,char *text,size_t *length)
{
    BPTR f=Open((CONST_STRPTR)name,MODE_OLDFILE);
    LONG n;
    *length=0; text[0]=0;
    if(!f) return 0;
    n=Read(f,text,XSETTINGS_FILE_MAX+1);
    Close(f);
    if(n<0 || n>XSETTINGS_FILE_MAX) return -1;
    *length=(size_t)n; text[n]=0;
    return 1;
}

/* A name from a requester or typed in, made absolute here: ASL can give a
   drawer relative to this program's current one ("//sub"), which a Shell
   would read relative to its own. Names that cannot be locked stay. */
static void absolute(char *path,size_t size)
{
    char full[XSETTINGS_PATH_MAX];
    BPTR lock;
    struct Process *self=(struct Process *)FindTask(NULL);
    APTR window=self->pr_WindowPtr;
    if(!path[0] || strchr(path,':')) return;
    self->pr_WindowPtr=(APTR)-1;
    lock=Lock((CONST_STRPTR)path,SHARED_LOCK);
    self->pr_WindowPtr=window;
    if(!lock) return;
    if(NameFromLock(lock,(STRPTR)full,sizeof full) && strlen(full)<size) strcpy(path,full);
    UnLock(lock);
}

static int write_one(const char *name,const char *text,size_t length)
{
    char temp[64],dir[32];
    BPTR f,lock;
    LONG done;
    snprintf(dir,sizeof dir,"%.*s",(int)(strchr(name,'/')-name),name);
    if((lock=CreateDir((CONST_STRPTR)dir))) UnLock(lock);
    snprintf(temp,sizeof temp,"%s.new",name);
    if(!(f=Open((CONST_STRPTR)temp,MODE_NEWFILE))) return 0;
    done=length?Write(f,(APTR)text,(LONG)length):0;
    if(!Close(f) || done!=(LONG)length) { DeleteFile((CONST_STRPTR)temp); return 0; }
    /* Rename does not replace a file. */
    DeleteFile((CONST_STRPTR)name);
    if(!Rename((CONST_STRPTR)temp,(CONST_STRPTR)name)) { DeleteFile((CONST_STRPTR)temp); return 0; }
    return 1;
}

static const char *string_of(Object *o)
{
    STRPTR s=NULL;
    get(o,MUIA_String_Contents,&s);
    return s?(const char *)s:"";
}
static IPTR value_of(Object *o,ULONG attribute)
{
    IPTR v=0;
    get(o,attribute,&v);
    return v;
}

static void copy_text(char *dst,size_t size,const char *src)
{
    snprintf(dst,size,"%s",src);
}

/* The window's choices over what was read; other keys as they were. */
static void gather(XSettings *s)
{
    unsigned i;
    *s=loaded;
    s->session_start=(unsigned)value_of(start_cycle,MUIA_Cycle_Active);
    copy_text(s->fastfetch,sizeof s->fastfetch,string_of(program));
    s->fastfetch_look=(unsigned)value_of(look_cycle,MUIA_Cycle_Active);
    copy_text(s->fastfetch_config,sizeof s->fastfetch_config,string_of(config));
    s->fastfetch_hide=0;
    for(i=0;i<XFF_GROUPS;++i)
        if(!value_of(show[i],MUIA_Selected)) s->fastfetch_hide|=1u<<i;
    s->fastfetch_logo=(unsigned)value_of(logo_cycle,MUIA_Cycle_Active);
    copy_text(s->session_command,sizeof s->session_command,string_of(command));
    absolute(s->fastfetch,sizeof s->fastfetch);
    absolute(s->fastfetch_config,sizeof s->fastfetch_config);
}

static struct MsgPort *reply_port;
static XPrefsRequest request;
static int request_out;
static const char *port_name;
static ULONG port_key;

static void refresh(void)
{
    XSettings s;
    const char *why;
    char line[XPREFS_LINE_MAX];
    int r;
    gather(&s);
    set(pages,MUIA_Group_ActivePage,s.session_start);
    set(config_pop,MUIA_Disabled,s.fastfetch_look!=XFF_FILE);
    r=session_start_line(&s,line,sizeof line-2,&why);
    if(r>0) snprintf(line_text,sizeof line_text,"%s",line);
    else if(!r) copy_text(line_text,sizeof line_text,"(nothing)");
    else snprintf(line_text,sizeof line_text,"Not possible: %s",why);
    set(line_view,MUIA_Text_Contents,line_text);
    set(run,MUIA_Disabled,r<=0 || !port_name || !reply_port || request_out);
}

static void say(const char *text)
{
    copy_text(status,sizeof status,text);
    set(status_view,MUIA_Text_Contents,status);
}

/* Use: ENV: only; Save: ENVARC: too. Each file is edited from its own
   text, so lines of the user's in either stay as they are there. */
static int store(int keep)
{
    static char out[XSETTINGS_FILE_MAX+1],kept[XSETTINGS_FILE_MAX+1];
    XSettings s;
    size_t n=0,k=0;
    gather(&s);
    if(!settings_edit_session(session_text,session_length,&s,out,sizeof out,&n) ||
       (keep && !settings_edit_session(kept_text,kept_length,&s,kept,sizeof kept,&k))) {
        say("Session.conf would be over 4096 bytes; nothing was written");
        return 0;
    }
    if(!write_one(SESSION_FILE,out,n) || (keep && !write_one(KEPT_FILE,kept,k))) {
        say(keep?"Could not write ENV:Limpet or ENVARC:Limpet":"Could not write ENV:Limpet");
        return 0;
    }
    return 1;
}

static void run_now(void)
{
    XSettings s;
    const char *why;
    struct MsgPort *port;
    gather(&s);
    if(request_out || !port_name) return;
    memset(&request,0,sizeof request);
    if(session_start_line(&s,request.line,sizeof request.line-2,&why)<=0) return;
    request.message.mn_Node.ln_Type=NT_MESSAGE;
    request.message.mn_Length=sizeof request;
    request.message.mn_ReplyPort=reply_port;
    request.magic=XPREFS_MAGIC; request.action=XPREFS_RUN; request.key=port_key;
    /* The window may close at any time: find and send under Forbid. */
    Forbid();
    port=FindPort((CONST_STRPTR)port_name);
    if(port) PutMsg(port,&request.message);
    Permit();
    if(!port) { say("Not run: the Limpet window is closed"); port_name=NULL; refresh(); return; }
    request_out=1;
    say("Sent to the Limpet window...");
    refresh();
}

static void run_answered(void)
{
    static const char *const answers[]={
        "Run in the Limpet window",
        "Not run: a program is running in the Limpet window",
        "Not run: something is being typed in the Limpet window",
        "Not run: the Limpet window has no Shell",
        "Not run: the Limpet window is closed",
    };
    if(!GetMsg(reply_port)) return;
    request_out=0;
    say(request.result>=0 && request.result<=XPREFS_BAD?answers[request.result]:answers[XPREFS_BAD]);
    refresh();
}

/* A program for the command line, quoted for the Shell. */
static void choose_program(void)
{
    struct Library *AslBase=OpenLibrary((CONST_STRPTR)"asl.library",39);
    struct FileRequester *fr;
    char path[512],quoted[1100];
    if(!AslBase) { say("asl.library is not available"); return; }
    fr=AllocAslRequestTags(ASL_FileRequest,ASLFR_TitleText,(IPTR)"Choose a program",TAG_DONE);
    set(app,MUIA_Application_Sleep,TRUE);
    if(fr && AslRequestTags(fr,ASLFR_Window,(IPTR)value_of(win,MUIA_Window_Window),TAG_DONE)) {
        copy_text(path,sizeof path,(const char *)fr->fr_Drawer);
        if(AddPart((STRPTR)path,fr->fr_File,sizeof path) && (absolute(path,sizeof path),1) &&
           !session_quote_path(path,quoted,sizeof quoted) &&
           strlen(quoted)<XSETTINGS_COMMAND_MAX) {
            set(command,MUIA_String_Contents,quoted);
        } else say("That name cannot be used in a Shell line");
    }
    set(app,MUIA_Application_Sleep,FALSE);
    if(fr) FreeAslRequest(fr);
    CloseLibrary(AslBase);
}

static Object *label(const char *text) { return Label2((STRPTR)text); }

int main(int argc,char **argv)
{
    (void)argv;
    static const char *starts[]={"Nothing","Fastfetch","My own command",NULL};
    static const char *looks[]={"Compact","AROS logo picture","My configuration file",NULL};
    static const char *logos[]={"As in the look","Text","None",NULL};
    static const char *group_labels[XFF_GROUPS]={"System","Hardware","Memory and disks","Network"};
    struct RDArgs *args=NULL;
    IPTR arg[2]={0,0};
    const char *why;
    ULONG sigs=0;
    int result=RETURN_OK,r;
    Object *save,*use,*cancel,*groups_box;
    unsigned i;

    if(argc>0) {
        args=ReadArgs((CONST_STRPTR)"PORT/K,KEY/K/N",(APTR)arg,NULL);
        if(!args) { PrintFault(IoErr(),(CONST_STRPTR)"LimpetPrefs"); return RETURN_FAIL; }
        if(arg[0] && arg[1] && !strncmp((const char *)arg[0],XPREFS_PORT_PREFIX,strlen(XPREFS_PORT_PREFIX))) {
            port_name=(const char *)arg[0];
            port_key=(ULONG)*(LONG *)arg[1];
        }
    }
    settings_default(&loaded);
    /* Shown: ENV:'s file, else ENVARC:'s, as Limpet reads them. */
    {   int in_session=read_file(SESSION_FILE,session_text,&session_length);
        int in_kept=read_file(KEPT_FILE,kept_text,&kept_length);
        const char *text=in_session?session_text:kept_text;
        size_t length=in_session?session_length:kept_length;
        if(in_session<0) session_length=0;
        if(in_kept<0) kept_length=0;
        r=in_session?in_session:in_kept;
        if(r<0) copy_text(status,sizeof status,"Session.conf is over 4096 bytes; it will be replaced");
        else if(r>0 && (r=settings_parse_session(&loaded,text,length,&why))) {
            snprintf(status,sizeof status,"Session.conf line %d: %s; Limpet runs nothing",r,why);
            settings_default(&loaded);
        } else copy_text(status,sizeof status,port_name?"":"Run now: open Preferences from a Limpet window"); }
    reply_port=CreateMsgPort();

    groups_box=HGroup,End;
    for(i=0;i<XFF_GROUPS;++i) {
        show[i]=MUI_MakeObject(MUIO_Checkmark,(IPTR)group_labels[i]);
        if(show[i]) {
            DoMethod(groups_box,OM_ADDMEMBER,(IPTR)show[i]);
            DoMethod(groups_box,OM_ADDMEMBER,(IPTR)LLabel1((STRPTR)group_labels[i]));
        }
    }
    DoMethod(groups_box,OM_ADDMEMBER,(IPTR)HSpace(0));

    app=ApplicationObject,
        MUIA_Application_Title,(IPTR)"Limpet Preferences",
        MUIA_Application_Version,(IPTR)(version_tag+1),
        MUIA_Application_Description,(IPTR)"What a new Limpet Shell window runs",
        MUIA_Application_Base,(IPTR)"LIMPETPREFS",
        SubWindow,win=WindowObject,
            MUIA_Window_Title,(IPTR)"Limpet Preferences",
            MUIA_Window_ID,MAKE_ID('L','P','R','F'),
            WindowContents,VGroup,
                Child,VGroup,GroupFrameT("When a new Shell window opens"),
                    Child,ColGroup(2),
                        Child,label("Run"),
                        Child,start_cycle=CycleObject,MUIA_Cycle_Entries,(IPTR)starts,End,
                        End,
                    Child,pages=VGroup,MUIA_Group_PageMode,TRUE,
                        Child,VGroup,
                            Child,TextObject,MUIA_Text_Contents,(IPTR)"The Shell starts as usual.",End,
                            Child,VSpace(0),
                            End,
                        Child,ColGroup(2),
                            Child,label("Fastfetch"),
                            Child,PopaslObject,
                                MUIA_Popstring_String,(IPTR)(program=StringObject,StringFrame,
                                    MUIA_String_MaxLen,XSETTINGS_PATH_MAX,MUIA_CycleChain,1,End),
                                MUIA_Popstring_Button,(IPTR)PopButton(MUII_PopFile),
                                ASLFR_TitleText,(IPTR)"Choose the Fastfetch program",
                                End,
                            Child,Label1((STRPTR)""),
                            Child,TextObject,MUIA_Text_Contents,(IPTR)"\33i(empty: fastfetch from the Shell's search path)",End,
                            Child,label("Look"),
                            Child,look_cycle=CycleObject,MUIA_Cycle_Entries,(IPTR)looks,End,
                            Child,label("File"),
                            Child,config_pop=PopaslObject,
                                MUIA_Popstring_String,(IPTR)(config=StringObject,StringFrame,
                                    MUIA_String_MaxLen,XSETTINGS_PATH_MAX,MUIA_CycleChain,1,End),
                                MUIA_Popstring_Button,(IPTR)PopButton(MUII_PopFile),
                                ASLFR_TitleText,(IPTR)"Choose a Fastfetch configuration file",
                                End,
                            Child,label("Show"),
                            Child,groups_box,
                            Child,label("Logo"),
                            Child,logo_cycle=CycleObject,MUIA_Cycle_Entries,(IPTR)logos,End,
                            End,
                        Child,ColGroup(2),
                            Child,label("Command"),
                            Child,HGroup,
                                Child,command=StringObject,StringFrame,
                                    MUIA_String_MaxLen,XSETTINGS_COMMAND_MAX,MUIA_CycleChain,1,End,
                                Child,choose=SimpleButton("C_hoose program..."),
                                End,
                            Child,Label1((STRPTR)""),
                            Child,VSpace(0),
                            End,
                        End,
                    Child,HGroup,
                        Child,label("Command line"),
                        Child,line_view=TextObject,TextFrame,MUIA_Background,MUII_TextBack,End,
                        End,
                    Child,HGroup,
                        Child,run=SimpleButton("_Run now"),
                        Child,HSpace(0),
                        End,
                    Child,status_view=TextObject,MUIA_Text_Contents,(IPTR)status,End,
                    End,
                Child,HGroup,
                    Child,HSpace(0),
                    Child,HGroup,MUIA_Group_SameWidth,TRUE,
                        Child,save=SimpleButton("_Save"),
                        Child,use=SimpleButton("_Use"),
                        Child,cancel=SimpleButton("_Cancel"),
                        End,
                    End,
                End,
            End,
        End;
    if(!app) {
        PutStr((CONST_STRPTR)"LimpetPrefs: cannot open its window (is muimaster.library there?)\n");
        if(reply_port) DeleteMsgPort(reply_port);
        if(args) FreeArgs(args);
        return RETURN_FAIL;
    }

    set(start_cycle,MUIA_Cycle_Active,loaded.session_start);
    set(program,MUIA_String_Contents,loaded.fastfetch);
    set(look_cycle,MUIA_Cycle_Active,loaded.fastfetch_look);
    set(config,MUIA_String_Contents,loaded.fastfetch_config);
    for(i=0;i<XFF_GROUPS;++i) set(show[i],MUIA_Selected,!(loaded.fastfetch_hide&(1u<<i)));
    set(logo_cycle,MUIA_Cycle_Active,loaded.fastfetch_logo);
    set(command,MUIA_String_Contents,loaded.session_command);
    for(i=0;i<XFF_GROUPS;++i)
        DoMethod(show[i],MUIM_Notify,MUIA_Selected,MUIV_EveryTime,(IPTR)app,2,MUIM_Application_ReturnID,ID_CHANGED);
    DoMethod(start_cycle,MUIM_Notify,MUIA_Cycle_Active,MUIV_EveryTime,(IPTR)app,2,MUIM_Application_ReturnID,ID_CHANGED);
    DoMethod(look_cycle,MUIM_Notify,MUIA_Cycle_Active,MUIV_EveryTime,(IPTR)app,2,MUIM_Application_ReturnID,ID_CHANGED);
    DoMethod(logo_cycle,MUIM_Notify,MUIA_Cycle_Active,MUIV_EveryTime,(IPTR)app,2,MUIM_Application_ReturnID,ID_CHANGED);
    DoMethod(program,MUIM_Notify,MUIA_String_Contents,MUIV_EveryTime,(IPTR)app,2,MUIM_Application_ReturnID,ID_CHANGED);
    DoMethod(config,MUIM_Notify,MUIA_String_Contents,MUIV_EveryTime,(IPTR)app,2,MUIM_Application_ReturnID,ID_CHANGED);
    DoMethod(command,MUIM_Notify,MUIA_String_Contents,MUIV_EveryTime,(IPTR)app,2,MUIM_Application_ReturnID,ID_CHANGED);
    DoMethod(choose,MUIM_Notify,MUIA_Pressed,FALSE,(IPTR)app,2,MUIM_Application_ReturnID,ID_CHOOSE);
    DoMethod(run,MUIM_Notify,MUIA_Pressed,FALSE,(IPTR)app,2,MUIM_Application_ReturnID,ID_RUN);
    DoMethod(save,MUIM_Notify,MUIA_Pressed,FALSE,(IPTR)app,2,MUIM_Application_ReturnID,ID_SAVE);
    DoMethod(use,MUIM_Notify,MUIA_Pressed,FALSE,(IPTR)app,2,MUIM_Application_ReturnID,ID_USE);
    DoMethod(cancel,MUIM_Notify,MUIA_Pressed,FALSE,(IPTR)app,2,MUIM_Application_ReturnID,ID_CANCEL);
    /* Esc and the close gadget: Cancel. */
    DoMethod(win,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,(IPTR)app,2,MUIM_Application_ReturnID,ID_CANCEL);
    {   Object *chain[]={start_cycle,look_cycle,logo_cycle,choose,run,save,use,cancel};
        for(i=0;i<sizeof chain/sizeof chain[0];++i) set(chain[i],MUIA_CycleChain,1);
        for(i=0;i<XFF_GROUPS;++i) set(show[i],MUIA_CycleChain,1); }
    refresh();
    set(win,MUIA_Window_Open,TRUE);
    if(!value_of(win,MUIA_Window_Open)) {
        PutStr((CONST_STRPTR)"LimpetPrefs: cannot open its window\n");
        result=RETURN_FAIL;
    } else for(;;) {
        ULONG id=DoMethod(app,MUIM_Application_NewInput,(IPTR)&sigs);
        int done=0;
        switch(id) {
        case MUIV_Application_ReturnID_Quit: case ID_CANCEL: done=1; break;
        case ID_CHANGED: refresh(); break;
        case ID_CHOOSE: choose_program(); refresh(); break;
        case ID_RUN: run_now(); break;
        case ID_USE: case ID_SAVE: done=store(id==ID_SAVE); break;
        }
        if(done) break;
        if(sigs) {
            ULONG reply=reply_port?1UL<<reply_port->mp_SigBit:0;
            sigs=Wait(sigs|reply|SIGBREAKF_CTRL_C);
            if(sigs&reply) run_answered();
            if(sigs&SIGBREAKF_CTRL_C) break;
        }
    }
    set(win,MUIA_Window_Open,FALSE);
    /* A request still out: its reply comes, the window replies at once. */
    while(request_out && reply_port) { WaitPort(reply_port); run_answered(); }
    MUI_DisposeObject(app);
    if(reply_port) DeleteMsgPort(reply_port);
    if(args) FreeArgs(args);
    return result;
}
