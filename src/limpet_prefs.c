/* Limpet Preferences: the theme, font and window background (Appearance),
 * what a new Shell window runs once it has read S:Shell-Startup (Shell),
 * and scrollback and history (Terminal). A Zune program of its own, started
 * from a Limpet window's Project > Preferences (or from a Shell). It writes
 * Limpet.conf, Colours.conf, Window.conf and Session.conf (settings.h,
 * settings_edit.h): Use the files in ENV:Limpet, Save those in ENVARC: as
 * well, each edited from its own text; Cancel changes nothing. The window
 * it was opened from shows a chosen theme at once and runs a line at its
 * Shell prompt on Run now (prefs_port.h). */
#include "settings.h"
#include "settings_edit.h"
#include "session_start.h"
#include "session_command.h"
#include "prefs_port.h"
#include "window_alpha.h"
#include "theme_file.h"
#include "theme_list.h"
#include <exec/memory.h>
#include <dos/dos.h>
#include <libraries/mui.h>
#include <libraries/asl.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/asl.h>
#include <proto/wb.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef LIMPET_VERSION
#define LIMPET_VERSION "0.0.0-dev"
#endif
#ifndef LIMPET_DATE
#define LIMPET_DATE "1.1.2026"
#endif
static const char version_tag[] __attribute__((used))="$VER: LimpetPrefs " LIMPET_VERSION " (" LIMPET_DATE ")";

/* The settings files, each read from ENV: (else ENVARC:) as Limpet reads
   them, and written back from its own text. */
typedef int (*ParseFn)(XSettings *,const char *,size_t,const char **);
typedef int (*EditFn)(const char *,size_t,const XSettings *,char *,size_t,size_t *);
typedef struct {
    const char *name;         /* "Session.conf" */
    const char *consequence;  /* what Limpet does with a wrong file */
    ParseFn parse;
    EditFn edit;
    char text[XSETTINGS_FILE_MAX+1],kept[XSETTINGS_FILE_MAX+1];
    size_t length,kept_length;
} Conf;
enum { CONF_SESSION, CONF_WINDOW, CONF_MAIN, CONF_COLOURS, CONFS };
static Conf conf[CONFS]={
    {"Session.conf","Limpet runs nothing",settings_parse_session,settings_edit_session,"","",0,0},
    {"Window.conf","the background stays solid",settings_parse_window,settings_edit_window,"","",0,0},
    {"Limpet.conf","Limpet uses its built-in settings until the line is corrected",settings_parse,settings_edit_main,"","",0,0},
    {"Colours.conf","bold text keeps bright colours",settings_parse_colours,settings_edit_colours,"","",0,0},
};

enum { ID_CHANGED=1, ID_SAVE, ID_USE, ID_CANCEL, ID_RUN, ID_CHOOSE, ID_THEME, ID_DRAWER, ID_REFRESH, ID_ADD,
       ID_ACTIVE };

/* What this screen can show, asked of the running system. */
static int can_alpha,asked_window;
static XSettings loaded;
static char status[200];
static char line_text[600];
static char background_note_text[120];

static Object *app,*win,*start_cycle,*pages,*program,*look_cycle,*config_pop,*config,
    *show[XFF_GROUPS],*logo_cycle,*command,*choose,*line_view,*run,*status_view,
    *opacity,*blur,*background_note,
    *theme_view,*swatch[20],*bold,*font,*font_size,*scrollback,*history;

/* The themes offered: row 0 is the built-in dark theme, then the files of
   the two drawers, then, if the settings name something else (a path), that. */
static XThemeEntry themes[XTHEME_LIST_MAX],found_user[XTHEME_LIST_MAX],found_program[XTHEME_LIST_MAX];
static unsigned theme_count;
static char other_theme[XSETTINGS_PATH_MAX];   /* a theme= value that is no listed file */
static char theme_value[XSETTINGS_PATH_MAX];   /* the choice: "" is the built-in theme */
static char swatch_spec[20][40];
static struct DateStamp drawer_date;
static int previewed;

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

/* A lock without a requester for a volume or drawer that is not there. */
static BPTR quiet_lock(const char *path)
{
    struct Process *self=(struct Process *)FindTask(NULL);
    APTR window=self->pr_WindowPtr;
    BPTR lock;
    self->pr_WindowPtr=(APTR)-1;
    lock=Lock((CONST_STRPTR)path,SHARED_LOCK);
    self->pr_WindowPtr=window;
    return lock;
}
static int exists(const char *path)
{
    BPTR lock=quiet_lock(path);
    if(lock) UnLock(lock);
    return lock!=0;
}

/* A name from a requester or typed in, made absolute here: ASL can give a
   drawer relative to this program's current one ("//sub"), which a Shell
   would read relative to its own. Names that cannot be locked stay. */
static void absolute(char *path,size_t size)
{
    char full[XSETTINGS_PATH_MAX];
    BPTR lock;
    if(!path[0] || strchr(path,':')) return;
    lock=quiet_lock(path);
    if(!lock) return;
    if(NameFromLock(lock,(STRPTR)full,sizeof full) && strlen(full)<size) strcpy(path,full);
    UnLock(lock);
}

static void make_drawer(const char *name)
{
    BPTR lock=CreateDir((CONST_STRPTR)name);
    if(lock) UnLock(lock);
}

static int write_bytes(const char *name,const char *text,size_t length)
{
    char temp[XSETTINGS_PATH_MAX+8];
    BPTR f;
    LONG done;
    snprintf(temp,sizeof temp,"%s.new",name);
    if(!(f=Open((CONST_STRPTR)temp,MODE_NEWFILE))) return 0;
    done=length?Write(f,(APTR)text,(LONG)length):0;
    if(!Close(f) || done!=(LONG)length) { DeleteFile((CONST_STRPTR)temp); return 0; }
    /* Rename does not replace a file. */
    DeleteFile((CONST_STRPTR)name);
    if(!Rename((CONST_STRPTR)temp,(CONST_STRPTR)name)) { DeleteFile((CONST_STRPTR)temp); return 0; }
    return 1;
}

/* One settings file, in ENV:Limpet or ENVARC:Limpet. */
static int write_conf(int kept,const char *name,const char *text,size_t length)
{
    char path[64];
    make_drawer(kept?"ENVARC:Limpet":"ENV:Limpet");
    snprintf(path,sizeof path,"%s:Limpet/%s",kept?"ENVARC":"ENV",name);
    return write_bytes(path,text,length);
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
    /* Not offered here: stays as it was saved, for a system that can show it. */
    if(can_alpha) s->background_opacity=(unsigned)value_of(opacity,MUIA_Numeric_Value);
    absolute(s->fastfetch,sizeof s->fastfetch);
    absolute(s->fastfetch_config,sizeof s->fastfetch_config);
    copy_text(s->theme,sizeof s->theme,theme_value);
    copy_text(s->font,sizeof s->font,string_of(font));
    absolute(s->font,sizeof s->font);
    s->font_size=(unsigned)value_of(font_size,MUIA_Numeric_Value);
    s->scrollback=(unsigned)value_of(scrollback,MUIA_Numeric_Value);
    s->history=(unsigned)value_of(history,MUIA_Numeric_Value);
    s->bold_bright=value_of(bold,MUIA_Selected)!=0;
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

/* A reason as theme_file_message words it for Limpet's title bar, without
   its "Settings: " there. */
static const char *reason(const char *text)
{
    static char sentence[200];
    snprintf(sentence,sizeof sentence,"%s",strncmp(text,"Settings: ",10)?text:text+10);
    if(sentence[0]>='a' && sentence[0]<='z') sentence[0]-=32;
    return sentence;
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

/* One request to the window Preferences came from, answered before this
   returns: the window answers at once, and closing it answers too. -1
   without a window; else its answer (XPREFS_*). */
static LONG ask(ULONG action,const char *line)
{
    struct MsgPort *port;
    if(!port_name || !reply_port) return -1;
    /* A Run now still out first: its answer shares the one request. */
    while(request_out) { WaitPort(reply_port); run_answered(); }
    memset(&request,0,sizeof request);
    request.message.mn_Node.ln_Type=NT_MESSAGE;
    request.message.mn_Length=sizeof request;
    request.message.mn_ReplyPort=reply_port;
    request.magic=XPREFS_MAGIC; request.action=action; request.key=port_key;
    if(line) copy_text(request.line,sizeof request.line,line);
    /* The window may close at any time: find and send under Forbid. */
    Forbid();
    port=FindPort((CONST_STRPTR)port_name);
    if(port) PutMsg(port,&request.message);
    Permit();
    if(!port) { port_name=NULL; return -1; }
    WaitPort(reply_port);
    GetMsg(reply_port);
    return request.result;
}

/* Use: ENV: only; Save: ENVARC: too. Each file is edited from its own
   text, so lines of the user's in either stay as they are there. */
static int store(int keep)
{
    static char out[CONFS][XSETTINGS_FILE_MAX+1],kept[CONFS][XSETTINGS_FILE_MAX+1];
    size_t n[CONFS],k[CONFS];
    XSettings s;
    char text[80];
    unsigned i;
    gather(&s);
    for(i=0;i<CONFS;++i)
        if(!conf[i].edit(conf[i].text,conf[i].length,&s,out[i],sizeof out[i],&n[i]) ||
           (keep && !conf[i].edit(conf[i].kept,conf[i].kept_length,&s,kept[i],sizeof kept[i],&k[i]))) {
            snprintf(text,sizeof text,"%s would be over 4096 bytes; nothing was written",conf[i].name);
            say(text);
            return 0;
        }
    for(i=0;i<CONFS;++i)
        if(!write_conf(0,conf[i].name,out[i],n[i]) || (keep && !write_conf(1,conf[i].name,kept[i],k[i]))) {
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
    /* A Fastfetch chosen by its place: say so here when it is not there,
       instead of the Shell's "object not found" in the window. By name the
       Shell's own search path decides, which this program cannot know. */
    if(s.session_start==XSTART_FASTFETCH && s.fastfetch[0] && !exists(s.fastfetch)) {
        say("Not run: Fastfetch is not at the place set here");
        return;
    }
    request.message.mn_Node.ln_Type=NT_MESSAGE;
    request.message.mn_Length=sizeof request;
    request.message.mn_ReplyPort=reply_port;
    request.magic=XPREFS_MAGIC; request.action=XPREFS_RUN; request.key=port_key;
    Forbid();
    port=FindPort((CONST_STRPTR)port_name);
    if(port) PutMsg(port,&request.message);
    Permit();
    if(!port) { say("Not run: the Limpet window is closed"); port_name=NULL; refresh(); return; }
    request_out=1;
    say("Sent to the Limpet window...");
    refresh();
}

/* After Use and Save: the window Preferences came from reads its settings
   again, so the theme and the background show at once. A Limpet older than
   these requests still takes the background. */
static void apply_to_window(void)
{
    if(ask(XPREFS_APPLY_SETTINGS,NULL)==XPREFS_BAD) ask(XPREFS_APPLY_WINDOW,NULL);
    previewed=0;
}

/* What the screen of the window Preferences came from can show. Limpet can
   be on any public screen, so the default one says nothing about it. */
static int ask_window_background(void)
{
    if(ask(XPREFS_BACKGROUND,NULL)!=XPREFS_RAN) return 0;
    can_alpha=(request.background&XPREFS_CAN_ALPHA)!=0;
    return 1;
}

/* Without a window: the screen a new window opens on. */
static void ask_default_screen(void)
{
    struct Screen *screen=LockPubScreen(NULL);
    can_alpha=window_alpha_supported(screen);
    if(screen) UnlockPubScreen(NULL,screen);
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

/* ---- Themes ---- */

/* The files of a drawer, by name. */
static unsigned scan(const char *drawer,XThemeEntry *out,unsigned capacity,struct DateStamp *date)
{
    struct FileInfoBlock *fib=AllocDosObject(DOS_FIB,NULL);
    BPTR lock=quiet_lock(drawer);
    unsigned n=0;
    if(date) memset(date,0,sizeof *date);
    if(lock && fib && Examine(lock,fib)) {
        if(date) *date=fib->fib_Date;
        while(n<capacity && ExNext(lock,fib)) {
            if(fib->fib_DirEntryType>=0 || strlen((const char *)fib->fib_FileName)>=XTHEME_NAME_MAX) continue;
            copy_text(out[n].name,sizeof out[n].name,(const char *)fib->fib_FileName);
            out[n++].user=0;
        }
    }
    if(lock) UnLock(lock);
    if(fib) FreeDosObject(DOS_FIB,fib);
    return n;
}

/* Row of the list for the current choice. */
static LONG theme_row(void)
{
    int at;
    if(!theme_value[0]) return 0;
    at=theme_list_find(themes,theme_count,theme_value);
    if(at>=0) return 1+at;
    return other_theme[0]?(LONG)(1+theme_count):0;
}

/* Reads both drawers and fills the list, keeping the choice. */
static void list_themes(void)
{
    char row[XTHEME_NAME_MAX+XSETTINGS_PATH_MAX+16],label[XTHEME_NAME_MAX];
    unsigned nu,np,i;
    nu=scan(XTHEME_USER_DRAWER,found_user,XTHEME_LIST_MAX,&drawer_date);
    np=scan(XTHEME_PROGRAM_DRAWER,found_program,XTHEME_LIST_MAX,NULL);
    theme_count=theme_list_merge(themes,XTHEME_LIST_MAX,found_program,np,found_user,nu);
    /* A value that is a path, or names no file here: kept, as its own row. */
    other_theme[0]=0;
    if(theme_value[0] && theme_list_find(themes,theme_count,theme_value)<0)
        copy_text(other_theme,sizeof other_theme,theme_value);
    set(theme_view,MUIA_List_Quiet,TRUE);
    DoMethod(theme_view,MUIM_List_Clear);
    DoMethod(theme_view,MUIM_List_InsertSingle,(IPTR)"Limpet dark (built in)",MUIV_List_Insert_Bottom);
    for(i=0;i<theme_count;++i) {
        theme_list_label(label,sizeof label,themes[i].name);
        snprintf(row,sizeof row,themes[i].user?"%s  (yours)":"%s",label);
        DoMethod(theme_view,MUIM_List_InsertSingle,(IPTR)row,MUIV_List_Insert_Bottom);
    }
    if(other_theme[0]) {
        snprintf(row,sizeof row,"%s  (file)",other_theme);
        DoMethod(theme_view,MUIM_List_InsertSingle,(IPTR)row,MUIV_List_Insert_Bottom);
    }
    set(theme_view,MUIA_List_Quiet,FALSE);
    nnset(theme_view,MUIA_List_Active,theme_row());
}

static void show_swatches(const XTheme *t)
{
    uint32_t colour[20];
    unsigned i;
    memcpy(colour,t->ansi,sizeof t->ansi);
    colour[16]=t->background; colour[17]=t->foreground; colour[18]=t->cursor; colour[19]=t->selection_bg;
    for(i=0;i<20;++i) {
        unsigned long r=colour[i]>>16&255,g=colour[i]>>8&255,b=colour[i]&255;
        snprintf(swatch_spec[i],sizeof swatch_spec[i],"2:%08lx,%08lx,%08lx",r*0x01010101UL,g*0x01010101UL,b*0x01010101UL);
        set(swatch[i],MUIA_Background,(IPTR)swatch_spec[i]);
    }
}

/* The theme of a theme= value, read as Limpet reads it. 0, or -1 with the
   reason in text. */
static int read_theme(XTheme *t,const char *value,char *text,size_t size)
{
    char path[XSETTINGS_PATH_MAX+32];
    XThemeReport report;
    struct Process *self=(struct Process *)FindTask(NULL);
    APTR window=self->pr_WindowPtr;
    int result;
    theme_default(t);
    text[0]=0;
    if(!value[0]) return 0;
    if(theme_locate(path,sizeof path,value,exists)) {
        memset(&report,0,sizeof report); report.error=XTHEME_CANNOT_READ;
        theme_file_message(text,size,value,&report,"not chosen");
        return -1;
    }
    self->pr_WindowPtr=(APTR)-1;
    result=theme_file_load(t,path,&report);
    self->pr_WindowPtr=window;
    theme_file_message(text,size,value,&report,"not chosen");
    return result;
}

/* Shows the current choice: its colours here, and, with preview, the theme
   itself in the window Preferences came from. */
static void show_theme(int preview)
{
    XTheme t;
    char text[200];
    if(read_theme(&t,theme_value,text,sizeof text)) { say(reason(text)); return; }
    show_swatches(&t);
    if(text[0]) say(text);
    else say(theme_luma(t.background)<128?"A dark theme":"A light theme");
    if(preview && ask(XPREFS_PREVIEW_THEME,theme_value[0]?theme_value:"*")==XPREFS_RAN) previewed=1;
}

/* A row was chosen. A file that is not a colour file says why and the
   choice goes back to what it was. */
static void theme_chosen(void)
{
    char before[XSETTINGS_PATH_MAX],text[200];
    XTheme t;
    LONG row=(LONG)value_of(theme_view,MUIA_List_Active);
    copy_text(before,sizeof before,theme_value);
    if(row<=0) theme_value[0]=0;
    else if((unsigned)row<=theme_count) copy_text(theme_value,sizeof theme_value,themes[row-1].name);
    else copy_text(theme_value,sizeof theme_value,other_theme);
    if(read_theme(&t,theme_value,text,sizeof text)) {
        say(reason(text));
        copy_text(theme_value,sizeof theme_value,before);
        nnset(theme_view,MUIA_List_Active,theme_row());
        return;
    }
    show_theme(1);
}

/* The drawer the user puts downloaded colour files into, opened on
   Workbench. */
static void open_drawer(void)
{
    struct Library *WorkbenchBase;
    char text[120];
    make_drawer("ENVARC:Limpet");
    make_drawer(XTHEME_USER_DRAWER);
    if(!exists(XTHEME_USER_DRAWER)) { say("Could not make the drawer " XTHEME_USER_DRAWER); return; }
    WorkbenchBase=OpenLibrary((CONST_STRPTR)"workbench.library",44);
    if(WorkbenchBase && OpenWorkbenchObjectA((STRPTR)XTHEME_USER_DRAWER,NULL))
        say("Put colour files into the drawer, then Refresh list");
    else {
        snprintf(text,sizeof text,"Workbench did not open it: the drawer is %s",XTHEME_USER_DRAWER);
        say(text);
    }
    if(WorkbenchBase) CloseLibrary(WorkbenchBase);
}

/* A colour file from anywhere, copied unchanged into the themes drawer
   and chosen. */
static void add_file(void)
{
    struct Library *AslBase=OpenLibrary((CONST_STRPTR)"asl.library",39);
    struct FileRequester *fr;
    char path[512],target[XSETTINGS_PATH_MAX],text[200];
    if(!AslBase) { say("asl.library is not available"); return; }
    fr=AllocAslRequestTags(ASL_FileRequest,ASLFR_TitleText,(IPTR)"Add a colour file (Ghostty, Kitty, iTerm2 or Limpet)",TAG_DONE);
    set(app,MUIA_Application_Sleep,TRUE);
    if(fr && AslRequestTags(fr,ASLFR_Window,(IPTR)value_of(win,MUIA_Window_Window),TAG_DONE) && fr->fr_File[0]) {
        XTheme t;
        copy_text(path,sizeof path,(const char *)fr->fr_Drawer);
        if(!AddPart((STRPTR)path,fr->fr_File,sizeof path) || strlen((const char *)fr->fr_File)>=XTHEME_NAME_MAX)
            say("That file's name is too long for the themes drawer");
        else if((absolute(path,sizeof path),read_theme(&t,path,text,sizeof text)))
            say(reason(text));
        else {
            char *bytes=malloc(XTHEME_FILE_MAX);
            BPTR f=bytes?Open((CONST_STRPTR)path,MODE_OLDFILE):0;
            LONG n=f?Read(f,bytes,XTHEME_FILE_MAX):-1;
            if(f) Close(f);
            make_drawer("ENVARC:Limpet");
            make_drawer(XTHEME_USER_DRAWER);
            snprintf(target,sizeof target,"%s/%s",XTHEME_USER_DRAWER,(const char *)fr->fr_File);
            if(n<=0 || !write_bytes(target,bytes,(size_t)n)) say("Could not copy the file into " XTHEME_USER_DRAWER);
            else {
                copy_text(theme_value,sizeof theme_value,(const char *)fr->fr_File);
                list_themes();
                show_theme(1);
            }
            free(bytes);
        }
    }
    set(app,MUIA_Application_Sleep,FALSE);
    if(fr) FreeAslRequest(fr);
    CloseLibrary(AslBase);
}

/* The window became active: files put into the drawer meanwhile show. */
static void drawer_changed(void)
{
    struct FileInfoBlock *fib=AllocDosObject(DOS_FIB,NULL);
    BPTR lock=quiet_lock(XTHEME_USER_DRAWER);
    int changed=0;
    if(lock && fib && Examine(lock,fib)) changed=memcmp(&fib->fib_Date,&drawer_date,sizeof drawer_date)!=0;
    if(lock) UnLock(lock);
    if(fib) FreeDosObject(DOS_FIB,fib);
    if(changed) list_themes();
}

static Object *label(const char *text) { return Label2((STRPTR)text); }
static Object *swatch_object(void)
{
    return RectangleObject,TextFrame,MUIA_FixHeight,14,MUIA_Background,MUII_BACKGROUND,End;
}

int main(int argc,char **argv)
{
    (void)argv;
    static const char *starts[]={"Nothing","Fastfetch","My own command",NULL};
    static const char *looks[]={"Compact","AROS logo picture","My configuration file",NULL};
    static const char *logos[]={"As in the look","Text","None",NULL};
    static const char *group_labels[XFF_GROUPS]={"System","Hardware","Memory and disks","Network"};
    static const char *tabs[]={"Appearance","Shell","Terminal",NULL};
    struct RDArgs *args=NULL;
    IPTR arg[2]={0,0};
    const char *why;
    ULONG sigs=0;
    int result=RETURN_OK,r;
    Object *save,*use,*cancel,*groups_box,*palette_box,*roles_box,*drawer,*rescan,*add;
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
    copy_text(status,sizeof status,port_name?"":"Opened without a Limpet window: a theme shows in the terminal when this is opened from Project > Preferences");
    /* Shown: ENV:'s file, else ENVARC:'s, as Limpet reads them. A file that
       is wrong leaves its settings at their defaults and says so. */
    for(i=0;i<CONFS;++i) {
        char name[64];
        int in_env,in_kept;
        XSettings next=loaded;
        snprintf(name,sizeof name,"ENV:Limpet/%s",conf[i].name);
        in_env=read_file(name,conf[i].text,&conf[i].length);
        snprintf(name,sizeof name,"ENVARC:Limpet/%s",conf[i].name);
        in_kept=read_file(name,conf[i].kept,&conf[i].kept_length);
        if(in_env<0) conf[i].length=0;
        if(in_kept<0) conf[i].kept_length=0;
        r=in_env?in_env:in_kept;
        if(r<0) snprintf(status,sizeof status,"%s is over 4096 bytes; it will be replaced",conf[i].name);
        else if(r>0 && (r=conf[i].parse(&next,in_env?conf[i].text:conf[i].kept,
                                       in_env?conf[i].length:conf[i].kept_length,&why)))
            snprintf(status,sizeof status,"%s line %d: %s; %s",conf[i].name,r,why,conf[i].consequence);
        else loaded=next;
    }
    copy_text(theme_value,sizeof theme_value,loaded.theme);
    reply_port=CreateMsgPort();
    asked_window=ask_window_background();
    if(!asked_window) ask_default_screen();

    groups_box=HGroup,End;
    palette_box=ColGroup(8),MUIA_Group_Spacing,1,End;
    roles_box=ColGroup(4),MUIA_Group_Spacing,1,End;
    blur=MUI_MakeObject(MUIO_Checkmark,(IPTR)"Blur");
    bold=MUI_MakeObject(MUIO_Checkmark,(IPTR)"Bold");
    if(!can_alpha) copy_text(background_note_text,sizeof background_note_text,
        asked_window?"This window's screen cannot show transparent windows: the background stays solid."
                    :"The default public screen cannot show transparent windows: the background stays solid.");
    else copy_text(background_note_text,sizeof background_note_text,
        "Background blur is not available in this version of Limpet.");
    for(i=0;i<XFF_GROUPS;++i) {
        show[i]=MUI_MakeObject(MUIO_Checkmark,(IPTR)group_labels[i]);
        if(show[i]) {
            DoMethod(groups_box,OM_ADDMEMBER,(IPTR)show[i]);
            DoMethod(groups_box,OM_ADDMEMBER,(IPTR)LLabel1((STRPTR)group_labels[i]));
        }
    }
    DoMethod(groups_box,OM_ADDMEMBER,(IPTR)HSpace(0));
    for(i=0;i<20;++i) {
        swatch[i]=swatch_object();
        if(swatch[i]) DoMethod(i<16?palette_box:roles_box,OM_ADDMEMBER,(IPTR)swatch[i]);
    }
    {   static const char *const roles[]={"Background","Text","Cursor","Selection"};
        for(i=0;i<4;++i) DoMethod(roles_box,OM_ADDMEMBER,(IPTR)CLabel((STRPTR)roles[i])); }

    app=ApplicationObject,
        MUIA_Application_Title,(IPTR)"Limpet Preferences",
        MUIA_Application_Version,(IPTR)(version_tag+1),
        MUIA_Application_Description,(IPTR)"Limpet's theme, font, window background and what a new Shell window runs",
        MUIA_Application_Base,(IPTR)"LIMPETPREFS",
        SubWindow,win=WindowObject,
            MUIA_Window_Title,(IPTR)"Limpet Preferences",
            MUIA_Window_ID,MAKE_ID('L','P','R','2'),
            WindowContents,VGroup,
                Child,RegisterGroup(tabs),
                    /* Appearance */
                    Child,VGroup,
                        Child,HGroup,
                            Child,VGroup,GroupFrameT("Theme"),
                                Child,ListviewObject,MUIA_CycleChain,1,
                                    MUIA_Listview_List,(IPTR)(theme_view=ListObject,InputListFrame,
                                        MUIA_List_ConstructHook,MUIV_List_ConstructHook_String,
                                        MUIA_List_DestructHook,MUIV_List_DestructHook_String,End),
                                    End,
                                Child,HGroup,
                                    Child,drawer=SimpleButton("_Open themes drawer"),
                                    Child,rescan=SimpleButton("Re_fresh list"),
                                    Child,add=SimpleButton("_Add file..."),
                                    End,
                                End,
                            Child,VGroup,
                                Child,VGroup,GroupFrameT("Colours of the chosen theme"),
                                    Child,palette_box,
                                    Child,roles_box,
                                    Child,HGroup,Child,bold,Child,LLabel1((STRPTR)"Bold text uses the bright colours"),Child,HSpace(0),End,
                                    End,
                                Child,ColGroup(2),GroupFrameT("Font"),
                                    Child,label("Font file"),
                                    Child,PopaslObject,
                                        MUIA_Popstring_String,(IPTR)(font=StringObject,StringFrame,
                                            MUIA_String_MaxLen,XSETTINGS_PATH_MAX,MUIA_CycleChain,1,End),
                                        MUIA_Popstring_Button,(IPTR)PopButton(MUII_PopFile),
                                        ASLFR_TitleText,(IPTR)"Choose a monospace TrueType or OpenType font",
                                        End,
                                    Child,label("Size"),
                                    Child,font_size=SliderObject,MUIA_CycleChain,1,
                                        MUIA_Numeric_Min,XSETTINGS_FONT_SIZE_MIN,MUIA_Numeric_Max,XSETTINGS_FONT_SIZE_MAX,
                                        MUIA_Numeric_Format,(IPTR)"%ld pixels",End,
                                    End,
                                Child,VSpace(0),
                                End,
                            End,
                        Child,VGroup,GroupFrameT("Window background (experimental)"),
                            Child,ColGroup(2),
                                Child,label("Background opacity"),
                                Child,opacity=SliderObject,MUIA_CycleChain,1,
                                    MUIA_Numeric_Min,XSETTINGS_BACKGROUND_OPACITY_MIN,
                                    MUIA_Numeric_Max,XSETTINGS_BACKGROUND_OPACITY_MAX,
                                    MUIA_Numeric_Format,(IPTR)"%ld %%",End,
                                Child,label("Background blur"),
                                Child,HGroup,Child,blur,Child,LLabel1((STRPTR)"Blur what is behind the window"),Child,HSpace(0),End,
                                End,
                            Child,background_note=TextObject,MUIA_Text_Contents,(IPTR)background_note_text,MUIA_Text_SetMin,FALSE,End,
                            End,
                        End,
                    /* Shell */
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
                            /* No minimum width from the text: a long line would
                               widen the window past its remembered size. */
                            Child,line_view=TextObject,TextFrame,MUIA_Background,MUII_TextBack,
                                MUIA_Text_SetMin,FALSE,End,
                            End,
                        Child,HGroup,
                            Child,run=SimpleButton("_Run now"),
                            Child,HSpace(0),
                            End,
                        End,
                    /* Terminal */
                    Child,VGroup,
                        Child,ColGroup(2),GroupFrameT("Memory of a window"),
                            Child,label("Scrollback"),
                            Child,scrollback=SliderObject,MUIA_CycleChain,1,
                                MUIA_Numeric_Min,XSETTINGS_SCROLLBACK_MIN,MUIA_Numeric_Max,XSETTINGS_SCROLLBACK_MAX,
                                MUIA_Numeric_Format,(IPTR)"%ld lines",End,
                            Child,label("Command history"),
                            Child,history=SliderObject,MUIA_CycleChain,1,
                                MUIA_Numeric_Min,XSETTINGS_HISTORY_MIN,MUIA_Numeric_Max,XSETTINGS_HISTORY_MAX,
                                MUIA_Numeric_Format,(IPTR)"%ld commands",End,
                            End,
                        Child,TextObject,MUIA_Text_Contents,(IPTR)"Scrollback counts lines at the widest window (160 columns); each takes about 27 KB.\nFont, size, scrollback and history apply to windows opened after Use or Save.",End,
                        Child,VSpace(0),
                        End,
                    End,
                Child,status_view=TextObject,MUIA_Text_Contents,(IPTR)status,MUIA_Text_SetMin,FALSE,End,
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
    set(opacity,MUIA_Numeric_Value,loaded.background_opacity);
    set(opacity,MUIA_Disabled,!can_alpha);
    set(blur,MUIA_Selected,loaded.background_blur!=0);
    set(blur,MUIA_Disabled,TRUE);
    set(bold,MUIA_Selected,loaded.bold_bright!=0);
    set(font,MUIA_String_Contents,loaded.font);
    set(font_size,MUIA_Numeric_Value,loaded.font_size);
    set(scrollback,MUIA_Numeric_Value,loaded.scrollback);
    set(history,MUIA_Numeric_Value,loaded.history);
    list_themes();
    {   /* The colours of the settings' theme, without touching the window:
           it shows that theme already. */
        XTheme t;
        char text[200];
        if(read_theme(&t,theme_value,text,sizeof text)) {
            theme_default(&t);
            if(!status[0]) copy_text(status,sizeof status,reason(text));
        }
        show_swatches(&t);
        set(status_view,MUIA_Text_Contents,status); }
    DoMethod(opacity,MUIM_Notify,MUIA_Numeric_Value,MUIV_EveryTime,(IPTR)app,2,MUIM_Application_ReturnID,ID_CHANGED);
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
    DoMethod(theme_view,MUIM_Notify,MUIA_List_Active,MUIV_EveryTime,(IPTR)app,2,MUIM_Application_ReturnID,ID_THEME);
    DoMethod(drawer,MUIM_Notify,MUIA_Pressed,FALSE,(IPTR)app,2,MUIM_Application_ReturnID,ID_DRAWER);
    DoMethod(rescan,MUIM_Notify,MUIA_Pressed,FALSE,(IPTR)app,2,MUIM_Application_ReturnID,ID_REFRESH);
    DoMethod(add,MUIM_Notify,MUIA_Pressed,FALSE,(IPTR)app,2,MUIM_Application_ReturnID,ID_ADD);
    DoMethod(win,MUIM_Notify,MUIA_Window_Activate,TRUE,(IPTR)app,2,MUIM_Application_ReturnID,ID_ACTIVE);
    /* Esc and the close gadget: Cancel. */
    DoMethod(win,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,(IPTR)app,2,MUIM_Application_ReturnID,ID_CANCEL);
    {   Object *chain[]={start_cycle,look_cycle,logo_cycle,choose,run,save,use,cancel,drawer,rescan,add,bold};
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
        case MUIV_Application_ReturnID_Quit: case ID_CANCEL:
            /* The window goes back to the theme it had. */
            if(previewed) { ask(XPREFS_PREVIEW_THEME,""); previewed=0; }
            done=1; break;
        case ID_CHANGED: refresh(); break;
        case ID_CHOOSE: choose_program(); refresh(); break;
        case ID_RUN: run_now(); break;
        case ID_THEME: theme_chosen(); break;
        case ID_DRAWER: open_drawer(); break;
        case ID_REFRESH: list_themes(); say("The list shows the files of both theme drawers"); break;
        case ID_ADD: add_file(); break;
        case ID_ACTIVE: drawer_changed(); break;
        case ID_USE: case ID_SAVE:
            if((done=store(id==ID_SAVE))) apply_to_window();
            break;
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
