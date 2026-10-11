#include "screen_model.h"
#include "theme.h"
#include "aros_font.h"
#include "cluster_font.h"
#include "aros_input.h"
#include "echo_session.h"
#include "selection.h"
#include "paste.h"
#include "session_reset.h"
#include "input_order.h"
#include "clipboard_text.h"
#include "aros_clip_job.h"
#include "aros_session.h"
#include "aros_handler.h"
#include "line_view.h"
#include "aros_completion.h"
#ifdef XTERM_PROCESS_PORT_CLIENTS
#include "aros_completion_job.h"
#endif
#include "hyperlink.h"
#include "link_gesture.h"
#include "aros_hyperlink.h"
#include "paint_damage.h"
#include "key_queue.h"
#include "kitty_graphics.h"
#include <dos/var.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <proto/gadtools.h>
#include <libraries/gadtools.h>
#include <cybergraphx/cybergraphics.h>
#include <devices/rawkeycodes.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <graphics/text.h>
#include <graphics/rpattr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../tools/flood_diag.h"

#ifdef XTERM_PTY_FRONTEND
#include "aros_pty_frontend.h"
#define aros_session_start aros_pty_start
#define aros_session_active aros_pty_active
#define aros_session_signals aros_pty_signals
#define aros_session_poll aros_pty_poll
#define aros_session_write aros_pty_write
#define aros_session_read aros_pty_read
#define aros_session_eof aros_pty_eof
#define aros_session_stop aros_pty_stop
#define aros_session_dispose aros_pty_dispose
static const int pty_mode=1;
#else
static const int pty_mode=0;
#endif

#define MAX_ROWS 80
#define MAX_COLUMNS 160
#define HISTORY_LINES 256
#define MARGIN 12

static XTermScreen screen;
static XTermCell cells[MAX_ROWS * MAX_COLUMNS];
static XTermCell alternate[MAX_ROWS * MAX_COLUMNS];
static XTermCell history[HISTORY_LINES * MAX_COLUMNS];
/* Reflow writes the rewrapped history here and swaps the two. */
static XTermCell history_spare[HISTORY_LINES * MAX_COLUMNS];
#include "title_buffer.h"
static XTitleBuffers titles;
#include "window_title.h"
/* What runs in the window (read from the Shell's process) and the title a
   program set for itself, which lasts while that program runs. */
static XProgramTitle program_title;
static XForeground foreground;
/* Ticks left to watch the foreground after a line was submitted: a command
   that does no I/O (Wait 30) sends no packet that would wake the loop. */
static unsigned foreground_watch;
static EchoSession echo;
static int echo_mode;
static ArosSession process_session;
static int process_reported;
static char peer_command[512];
static const char *process_command=peer_command;
static int shell_session;
#ifdef XTERM_DOS_HANDLER
static const int handler_mode=1;
#else
static const int handler_mode=0;
#endif
static const char *shell_profile;
static XSelection selection;
#include "boxdraw.h"
#include "mouse_release.h"
static unsigned mouse_buttons;
static unsigned mouse_last_row,mouse_last_col,mouse_position_valid;
static XMouseRelease mouse_releases;
static XLineEditor command_editor;
static XCompletion completion;
static int completion_active;
static char completion_status[96];
#ifdef XTERM_PROCESS_PORT_CLIENTS
/* This process port also carries client packets (aros_session.h), so TAB
 * enumeration runs in a child. While it runs, nothing may reply the Shell's
 * pending Read: the child reads that Shell's current/home dirs. */
static ArosCompletionJob completion_job;
static int completion_shift, stop_after_completion;
#endif
static XTermScreen command_view;
static XTermCell command_cells[MAX_ROWS*MAX_COLUMNS];
static XTermCell command_history[2*XLINE_CAPACITY];
static XPaste paste;
/* Bracketed pastes and resize reports, kept in order against keys and
   replies (src/input_order.h), and the input owner they belong to. */
static XInputOrder input_order;
static unsigned order_session_generation, order_input_generation;
static int raw_seen;
static ArosClipJob clip_job;
static unsigned char clip_text[CLIP_TEXT_LIMIT], clip_raw[CLIP_RAW_LIMIT];
static unsigned session_generation, paste_generation, copy_loss;
static const char *clip_status;
static unsigned char row_pixels[MAX_COLUMNS * XRASTER_MAX * XRASTER_MAX * 3];
static XTheme theme;
static XHyperlinks links;
static XHyperlinkHit hovered_link;
static int link_hovered;
static XLinkGesture link_gesture;
static ArosHyperlinkJob link_job;
static XPaintDamage paint_damage;
static XPaintCell paint_cache[MAX_ROWS*MAX_COLUMNS], paint_row[MAX_COLUMNS];
static unsigned long paint_calls,paint_rows;
static int title_cache=1,title_stats,apc_nodraw=1,kitty_shown_change;
#ifdef XTERM_PTY_DIAG
static int diag_exit_stage;
#endif
static unsigned long title_updates,title_skipped,output_redraws,output_quiet;
static XKeyQueue keyboard;
#include "reply_flow.h"
#include "resize_report.h"
static XReplyFlow reply_flow;
static unsigned char pending_output[4096];
static size_t output_length,output_offset;
static unsigned reply_generation;
static unsigned output_generation;
static int output_replies;
typedef char key_bytes_fit[(AROS_INPUT_MAX_BYTES<=XKEY_BYTES)?1:-1];

static void kitty_attach(void);
static void reset_links(void)
{
    hyperlinks_init(&links);
    screen_attach_links(&screen,&links);
    link_hovered=0;
    link_gesture_cancel(&link_gesture);
    /* Every screen_init site calls this: a new screen starts without images. */
    kitty_attach();
}

static void write_text(const char *text)
{
    screen_write(&screen, (const unsigned char *)text, strlen(text));
}

static void replay(void)
{
    unsigned i;
    char line[100];
    memset(&selection,0,sizeof selection);
    screen_init(&screen, cells, MAX_ROWS * MAX_COLUMNS,
                screen.rows, screen.columns);
    reset_links();
    screen_attach_history(&screen, history, HISTORY_LINES * MAX_COLUMNS);
    screen_attach_reflow(&screen, history_spare, HISTORY_LINES * MAX_COLUMNS);
    screen_attach_alternate(&screen, alternate, MAX_ROWS * MAX_COLUMNS);
    for (i = 1; i <= 32; ++i) {
        snprintf(line, sizeof line, "  replay history %02u  |  streamed bytes -> terminal cells\r\n", i);
        write_text(line);
    }
    write_text("\033[2J\033[H");
    write_text("\033[1;36m  Limpet\033[0m  /  native replay preview\r\n");
    write_text("  Mainline x86_64  |  Intuition + graphics.library\r\n\r\n");
    write_text("  UTF-8: Zażółć gęślą jaźń  |  ĄĆĘŁŃÓŚŹŻ\r\n");
    write_text("  Grüße, AROS!  |  €  |  Greek: Ελληνικά  Cyrillic: Привет\r\n");
    write_text("  CJK: 中文 日本語 한글  Emoji: 😀 🚀 🦀\r\n");
    write_text("  Clusters: é ä  ❤️  👍🏽  👨‍👩‍👧‍👦  🇵🇱  1️⃣\r\n");
    write_text("  Wide cells: |\033[44mAA\033[0m|\033[44m中\033[0m|\033[44m😀\033[0m|  each field = 2 columns\r\n");
    write_text("  \033[31mRED   \033[32mGREEN   \033[33mYELLOW   \033[34mBLUE   \033[35mMAGENTA   \033[36mCYAN\033[0m\r\n");
    write_text("  \033[1;37;44m  SGR foreground / background / bold  \033[0m\r\n\r\n");
    write_text("  Fragmented CSI: ");
    write_text("\033[");
    write_text("32mPASS\033[0m\r\n");
    write_text("  Links: https://aros.org | \033]8;;https://aros.sourceforge.io/\033\\AROS website\033]8;;\033\\\r\n");
    write_text("  Carriage return: old text\r  Carriage return: replaced\r\n");
    write_text("  Backspace: ABC\bD\r\n");
    write_text("  Erase line: visible stale suffix\r  Erase line: visible\033[K\r\n\r\n");
    write_text("  [u] history up   [d] history down   [r] replay\r\n");
    write_text("  [F1] replay  [F2] echo  [F4] DOS process  [F5] EOF\r\n");
    write_text("  Drag selects | RAmiga+C/V or Ctrl+Shift+C/V\r\n");
    write_text("  Shift+PgUp/PgDn: history | F3 confirms multiline paste\r\n");
    write_text("\033[1;32m  replay> \033[0m");
}

static void colours(struct RastPort *rp, ULONG fg, ULONG bg)
{
    SetRPAttrs(rp, RPTAG_PenMode, FALSE,
               RPTAG_FgColor, fg, RPTAG_BgColor, bg, TAG_DONE);
}

static int editor_ready(void)
{
    return echo_mode==2 && shell_session && aros_session_active(&process_session) &&
        !process_session.line.raw && !process_session.eof &&
        process_session.pending_read && !process_session.input.length &&
#ifdef XTERM_PROCESS_PORT_CLIENTS
        !completion_job.task_id &&
#endif
        !process_session.output.length && !process_session.pending_write;
}

/* The command line while Ctrl-R searches: label, query and match. */
static XLineEditor search_view;

static const XTermScreen *display_screen(void)
{
    if(echo_mode==2 && shell_session && !process_session.line.raw &&
       !process_session.eof && command_editor.searching && !screen.view_offset) {
        size_t cursor=0;
        search_view.length=line_editor_search_label(&command_editor,search_view.text,
                                                    sizeof search_view.text,&cursor);
        search_view.cursor=search_view.anchor=cursor; search_view.selecting=0;
        if(search_view.length &&
           line_view_build(&command_view,&screen,&search_view,command_cells,
                           MAX_ROWS*MAX_COLUMNS,command_history,2*XLINE_CAPACITY))
            return &command_view;
    }
    if(echo_mode==2 && shell_session && !process_session.line.raw &&
       !process_session.eof && command_editor.length && !screen.view_offset &&
       line_view_build(&command_view,&screen,&command_editor,command_cells,
                       MAX_ROWS*MAX_COLUMNS,command_history,2*XLINE_CAPACITY))
        return &command_view;
    return &screen;
}

static void editor_sync(void)
{
    console_input_stage(&process_session.line,command_editor.text,command_editor.length);
    selection.active=selection.dragging=0;
    screen_scroll_view(&screen,-(int)screen.view_offset);
}

static int editor_submit(void)
{
    char committed[XLINE_CAPACITY];
    size_t n;
    if(!editor_ready() || !console_input_submit(&process_session.line,&process_session.input)) return 0;
    n=line_editor_commit(&command_editor,committed,sizeof committed);
    /* The model and staged line have identical bounds; admission above has
     * already reserved the complete command. Echo once, at the output cursor. */
    screen.cluster_active=0; screen.cluster_state=0;
    if(n>1) screen_write(&screen,(const unsigned char *)committed,n-1);
    write_text("\r\n");
    selection.active=selection.dragging=0;
    aros_session_poll(&process_session);
    foreground_watch=8;
    return 1;
}

static int editor_key(UWORD code,UWORD qualifiers)
{
    XLineAction action;
    int shift=(qualifiers&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT))!=0;
    if(!editor_ready()) return 0;
    /* Ctrl-R search: Esc puts the line back, Backspace edits the query,
       a movement or editing key takes the match and then does its job. */
    if(command_editor.searching) {
        switch(code) {
        case RAWKEY_ESCAPE: line_editor_search_cancel(&command_editor); editor_sync(); return 1;
        case RAWKEY_BACKSPACE: line_editor_search_backspace(&command_editor); editor_sync(); return 1;
        case RAWKEY_LEFT: case RAWKEY_RIGHT: case RAWKEY_HOME: case RAWKEY_END:
        case RAWKEY_UP: case RAWKEY_DOWN: case RAWKEY_DELETE: case RAWKEY_TAB:
            line_editor_search_accept(&command_editor); editor_sync(); break;
        default: break;
        }
    }
    if(qualifiers&(IEQUALIFIER_CONTROL|IEQUALIFIER_LALT|IEQUALIFIER_RALT|IEQUALIFIER_RCOMMAND)) return 0;
    if(code==RAWKEY_TAB) {
        if(paste.length || clip_job.task_id) {
            clip_status="TAB unavailable during clipboard operation"; return 1;
        }
        if(!completion_active || !completion_current(&completion,&command_editor)) {
            struct MsgPort *reply=process_session.pending_read->dp_Port;
            struct Task *reader=reply?reply->mp_SigTask:NULL;
            completion_active=0;
            if(!completion_prepare(&completion,&command_editor)) {
                clip_status="TAB: move to token end and clear selection"; return 1;
            }
#ifdef XTERM_PROCESS_PORT_CLIENTS
            /* The Shell stays in its unreplied Read until the job ends, since
             * editor_ready() is false meanwhile. Result: completion_finished. */
            if(!reader || reader->tc_Node.ln_Type!=NT_PROCESS ||
               aros_completion_job_start(&completion_job,&completion,(struct Process *)reader)) {
                clip_status="TAB: directory unavailable or no matches"; return 1;
            }
            completion_shift=shift;
            clip_status="TAB: searching..."; return 1;
#else
            /* A synchronous Read pins its sender until we reply. Enumeration
             * does not service session packets or retain this process pointer. */
            if(!reader || reader->tc_Node.ln_Type!=NT_PROCESS ||
               !aros_completion_collect(&completion,(struct Process *)reader)) {
                clip_status="TAB: directory unavailable or no matches"; return 1;
            }
            completion_active=1;
#endif
        }
        if(!completion.count) clip_status="TAB: no matches";
        else if(!completion_apply(&completion,&command_editor,shift))
            clip_status="TAB: completion exceeds editor bounds";
        else {
            snprintf(completion_status,sizeof completion_status,
                "TAB %lu/%lu%s",
                (unsigned long)completion.next,(unsigned long)completion.count,
                completion.limited?" (limited)":"");
            clip_status=completion_status; editor_sync();
        }
        return 1;
    }
    switch(code) {
    case RAWKEY_LEFT: action=shift?XLINE_SELECT_LEFT:XLINE_LEFT; break;
    case RAWKEY_RIGHT: action=shift?XLINE_SELECT_RIGHT:XLINE_RIGHT; break;
    case RAWKEY_HOME: action=shift?XLINE_SELECT_HOME:XLINE_HOME; break;
    case RAWKEY_END: action=shift?XLINE_SELECT_END:XLINE_END; break;
    case RAWKEY_UP: if(shift) return 0; action=XLINE_PREVIOUS; break;
    case RAWKEY_DOWN: if(shift) return 0; action=XLINE_NEXT; break;
    case RAWKEY_BACKSPACE: action=XLINE_BACKSPACE; break;
    case RAWKEY_DELETE: action=XLINE_DELETE; break;
    default: return 0;
    }
    completion_active=0;
    line_editor_action(&command_editor,action); editor_sync();
    return 1;
}

#ifdef XTERM_PROCESS_PORT_CLIENTS
static void completion_finished(void)
{
    /* Keys were held while the child ran, so the editor still matches the
     * prepared token unless the session ended meanwhile. */
    if(!completion_job.result || !completion_current(&completion,&command_editor)) {
        clip_status="TAB: directory unavailable or no matches"; return;
    }
    completion_active=1;
    if(!completion.count) clip_status="TAB: no matches";
    else if(!completion_apply(&completion,&command_editor,completion_shift))
        clip_status="TAB: completion exceeds editor bounds";
    else {
        snprintf(completion_status,sizeof completion_status,
            "TAB %lu/%lu%s",
            (unsigned long)completion.next,(unsigned long)completion.count,
            completion.limited?" (limited)":"");
        clip_status=completion_status; editor_sync();
    }
}
#endif

static void terminal_reply(void *context,const unsigned char *bytes,size_t length)
{
    ArosSession *session=context;
    /* Never put protocol replies into a cooked Shell command line. Native
     * console writes are all-or-nothing when input capacity is exhausted. */
    if(session->console && session->line.raw && !reply_flow.length &&
       input_order_others_may_send(&input_order))
        (void)aros_session_write(session,bytes,length);
}

static size_t mouse_write(void *context,const unsigned char *bytes,size_t length)
{ return reply_flow.length || !input_order_others_may_send(&input_order)?0:
         aros_session_write(context,bytes,length); }

static int replies_enabled(void)
{
    return process_session.console && process_session.line.raw &&
        !process_session.eof && aros_session_active(&process_session) &&
        (!process_session.external || (process_session.handles&1));
}
static void replies_sync(void)
{
    if(!replies_enabled() || reply_generation!=process_session.input_generation) {
        memset(&reply_flow,0,sizeof reply_flow);
        screen.csi_reply_enabled=screen.escape_reply_enabled=0;
    }
    reply_generation=process_session.input_generation;
}
/* A reply that must wait for a paste keeps the output paused meanwhile. */
static size_t reply_send(void *context,const unsigned char *bytes,size_t length)
{ return input_order_others_may_send(&input_order)?aros_session_write(context,bytes,length):0; }
static int output_ready(void)
{
    if(reply_flow.length)
        return XSESSION_CAPACITY-process_session.input.length>=reply_flow.length-reply_flow.offset;
    return output_offset<output_length || process_session.output.length;
}
static size_t output_pump(void)
{
    size_t n;
    replies_sync();
    /* Retain the unread suffix before acknowledging any more child output.
     * A blocked response is retried even if its query ended the last chunk. */
    if(!output_ready()) return 0;
    if(output_offset==output_length && !reply_flow.length) {
        output_offset=0;
        output_length=aros_session_read(&process_session,pending_output,sizeof pending_output);
        output_generation=process_session.input_generation;
        output_replies=replies_enabled();
    }
    if(replies_enabled() && output_replies && output_generation==process_session.input_generation)
        n=reply_flow_write(&reply_flow,&screen,pending_output+output_offset,
                          output_length-output_offset,reply_send,&process_session);
    else {
        n=output_length-output_offset;
        screen_write(&screen,pending_output+output_offset,n);
    }
    flood_diag_output(pending_output+output_offset,n);
    output_offset+=n;
    return n;
}
static void output_reset(void)
{
    output_length=output_offset=0;
    memset(&reply_flow,0,sizeof reply_flow);
}

static void mouse_flush(void)
{
    static unsigned screen_generation,input_generation;
    if(screen_generation!=screen.mouse_generation || input_generation!=process_session.input_generation ||
       !process_session.console || !process_session.line.raw || process_session.eof || !screen.mouse_tracking) {
        mouse_buttons=mouse_position_valid=0; mouse_releases.count=0;
    } else mouse_release_flush(&mouse_releases,mouse_write,&process_session);
    screen_generation=screen.mouse_generation;
    input_generation=process_session.input_generation;
}

static void mouse_release_button(unsigned button,unsigned col,unsigned row,unsigned modifiers)
{
    unsigned char report[64]; size_t n;
    if(button>=3 || !(mouse_buttons&(1u<<button))) return;
    if(col>=screen.columns) col=screen.columns-1;
    if(row>=screen.rows) row=screen.rows-1;
    if(!screen.mouse_sgr) {if(col>222) col=222;if(row>222) row=222;}
    n=screen_mouse_encode(&screen,button,col,row,modifiers,1,report,sizeof report);
    if(n && mouse_release_add(&mouse_releases,report,n)) mouse_buttons&=~(1u<<button);
    mouse_flush();
}

/* Kitty graphics (src/kitty_graphics.h). Images are composited into each
   text row before it is uploaded, so text and image never flicker against
   each other and exposure, resize and scrolling reuse the row path. Limits:
   1024x1024 pixels and 4 MiB per image, 16 MiB for all images plus one
   transmission in progress.

   Fast path: when the only change is new pixels for an opaque RGB image at
   the same place and size, not overlapping another image, the image
   rectangle is uploaded on its own instead of repainting every text row
   under it. Everything else (moves, deletes, RGBA, scrolling, exposure)
   takes the row path. XTERM_KITTY_FASTPATH=0 turns it off for comparison. */
typedef struct {
    uint32_t id;uint64_t serial;int visible;
    long first;unsigned count,column,width,height,channels;
} KittyShown;
static XKitty kitty;
static int kitty_ready,kitty_dirty,kitty_fast_enabled=1,kitty_refreshing;
static unsigned long kitty_drawn,kitty_scroll_seen,kitty_fast_blits,kitty_fast_failed;
static unsigned kitty_alternate_seen,kitty_view_seen,kitty_fast;
static KittyShown kitty_shown[XKITTY_MAX_IMAGES];
/* A serial counts as drawn once every visible row of it was uploaded. */
static uint64_t kitty_ok_serial[XKITTY_MAX_IMAGES],kitty_drawn_serial[XKITTY_MAX_IMAGES];
static unsigned char kitty_ok_rows[XKITTY_MAX_IMAGES][MAX_ROWS];
static unsigned char kitty_rows_prev[MAX_ROWS];
static struct DateStamp kitty_first,kitty_last_drawn;

static void kitty_cursor(XKittyCursor *c,const XTermScreen *s)
{
    c->row=s->cursor_row;c->column=s->cursor_column;c->rows=s->rows;c->columns=s->columns;
    c->alternate=s->alternate_active;c->scroll_count=s->scroll_count;
    c->cell_width=xfont_width();c->cell_height=xfont_height();
    c->scroll_top=s->scroll_top;c->scroll_bottom=s->scroll_bottom;
}
static void kitty_sink(void *context,int event,const unsigned char *bytes,size_t n)
{
    XKittyCursor c;XKittyResult r;
    (void)context;
    if(event==XSCREEN_APC_BEGIN) xkitty_apc_begin(&kitty);
    else if(event==XSCREEN_APC_DATA) xkitty_apc_data(&kitty,bytes,n);
    else if(event==XSCREEN_APC_ABORT) xkitty_apc_abort(&kitty);
    else if(event==XSCREEN_APC_ERASE) {
        if(xkitty_erase_screen(&kitty,screen.alternate_active)) kitty_shown_change=1;
    } else {
        kitty_cursor(&c,&screen);
        if(!xkitty_apc_end(&kitty,&c,&r)) return;
        if(kitty.stats.frames_received==1 && !kitty_first.ds_Days && !kitty_first.ds_Tick) DateStamp(&kitty_first);
        /* Replies go the way model replies do, so only while the child owns
           the terminal's reply channel (raw mode), like CSI queries. */
        if(r.reply_length && screen.reply && r.reply_length<=XSCREEN_REPLY_CAPACITY)
            screen.reply(screen.reply_context,r.reply,r.reply_length);
        /* C=0 at the bottom: make room first, so the image moves up with
           the text it was placed on (src/kitty_graphics.h). */
        if(r.scroll) screen_index_lines(&screen,r.scroll);
        if(r.move_cursor) {
            screen.cursor_row=r.cursor_row;screen.cursor_column=r.cursor_column;
            screen.wrap_pending=r.wrap_pending;
        }
        /* What changed on screen is worked out per image when drawing; this
           only says that a redraw is due at all. */
        if(r.changed || r.move_cursor) kitty_shown_change=1;
    }
}
static void kitty_forget_state(void)
{
    memset(kitty_shown,0,sizeof kitty_shown);
    memset(kitty_ok_serial,0,sizeof kitty_ok_serial);
    memset(kitty_drawn_serial,0,sizeof kitty_drawn_serial);
    memset(kitty_ok_rows,0,sizeof kitty_ok_rows);
    kitty_dirty=1;
}
static void kitty_attach(void)
{
    static const XKittyLimits limits={1024,1024,4u<<20,16u<<20};
    UBYTE value[8];
    if(!kitty_ready) { xkitty_init(&kitty,&limits);kitty_ready=1; }
    else xkitty_reset(&kitty);
    screen.apc=kitty_sink;screen.apc_context=NULL;
    kitty_fast_enabled=!(GetVar((CONST_STRPTR)"XTERM_KITTY_FASTPATH",value,sizeof value,GVF_GLOBAL_ONLY)>0 &&
                         value[0]=='0');
    kitty_forget_state();
}
/* Optional measurement record for a demonstration: XTERM_KITTY_STATS names
   a file that gets one line when a session ends. Counts, not a claim. */
static void kitty_write_stats(void)
{
    UBYTE path[256];char line[640];BPTR file;
    long ms;
    if(!kitty.stats.commands) return;
    if(GetVar((CONST_STRPTR)"XTERM_KITTY_STATS",path,sizeof path,GVF_GLOBAL_ONLY)<=0) return;
    ms=((kitty_last_drawn.ds_Days-kitty_first.ds_Days)*86400L*1000L)+
       ((kitty_last_drawn.ds_Minute-kitty_first.ds_Minute)*60000L)+
       ((kitty_last_drawn.ds_Tick-kitty_first.ds_Tick)*1000L/TICKS_PER_SECOND);
    snprintf(line,sizeof line,
        "kitty frames_received=%lu frames_drawn=%lu dropped=%lu errors=%lu commands=%lu "
        "payload_bytes=%llu memory_now=%lu memory_peak=%lu first_to_last_drawn_ms=%ld "
        "fastpath=%d fast_blits=%lu fast_failed=%lu rows_uploaded=%lu draws=%lu output_redraws=%lu "
        "output_quiet=%lu titles_set=%lu titles_skipped=%lu title_cache=%d apc_nodraw=%d\n",
        kitty.stats.frames_received,kitty_drawn,kitty.stats.transmissions_dropped,kitty.stats.errors,
        kitty.stats.commands,kitty.stats.payload_bytes,(unsigned long)kitty.stats.memory_bytes,
        (unsigned long)kitty.stats.memory_peak,ms,kitty_fast_enabled,kitty_fast_blits,kitty_fast_failed,paint_rows,
        paint_calls,output_redraws,output_quiet,title_updates,title_skipped,title_cache,apc_nodraw);
    if((file=Open(path,MODE_READWRITE))) { Seek(file,0,OFFSET_END);Write(file,line,(LONG)strlen(line));Close(file); }
}
static void kitty_state(const XTermScreen *s,int live,unsigned i,KittyShown *out)
{
    const XKittyImage *image=&kitty.images[i];XKittyCursor c;
    memset(out,0,sizeof *out);
    kitty_cursor(&c,s);
    if(!live || !xkitty_placement_rows(image,&c,&out->first,&out->count)) return;
    out->visible=1;out->id=image->id;out->serial=image->serial;out->column=image->placed_column;
    out->width=image->width;out->height=image->height;out->channels=image->channels;
}
static int kitty_overlaps(const KittyShown *a,const KittyShown *b)
{
    unsigned w=xfont_width(),h=xfont_height();
    long ax=(long)a->column*w,ay=a->first*(long)h,bx=(long)b->column*w,by=b->first*(long)h;
    return a->visible && b->visible && ax<bx+(long)b->width && bx<ax+(long)a->width &&
           ay<by+(long)b->height && by<ay+(long)a->height;
}
static void kitty_forget_rows(const XTermScreen *s,const KittyShown *k)
{
    long r;
    if(!k->visible) return;
    for(r=k->first;r<k->first+(long)k->count;++r)
        if(r>=0 && r<(long)s->rows) paint_damage_forget_row(&paint_damage,(unsigned)r);
}
/* Decide per image what this pass repaints: nothing, its rectangle alone
   (kitty_fast), or every text row it covers or covered. */
static void kitty_damage(const XTermScreen *s,int live)
{
    unsigned char rows[MAX_ROWS];KittyShown now[XKITTY_MAX_IMAGES];unsigned i,j,r;
    int moved=s->scroll_count!=kitty_scroll_seen || s->alternate_active!=kitty_alternate_seen ||
              s->view_offset!=kitty_view_seen;
    memset(rows,0,sizeof rows);
    kitty_fast=0;
    for(i=0;i<XKITTY_MAX_IMAGES;++i) {
        kitty_state(s,live,i,&now[i]);
        if(now[i].visible) for(r=0;r<now[i].count;++r)
            if(now[i].first+(long)r>=0 && now[i].first+(long)r<(long)s->rows) rows[now[i].first+r]=1;
    }
    for(i=0;i<XKITTY_MAX_IMAGES;++i) {
        const KittyShown *was=&kitty_shown[i],*is=&now[i];
        int same_place=was->visible && is->visible && was->id==is->id && was->first==is->first &&
            was->count==is->count && was->column==is->column && was->width==is->width &&
            was->height==is->height && was->channels==is->channels;
        int alone=1;
        if(!kitty_dirty && !moved && same_place && was->serial==is->serial) continue;
        if(!was->visible && !is->visible) continue;
        for(j=0;j<XKITTY_MAX_IMAGES;++j) if(j!=i && kitty_overlaps(is,&now[j])) alone=0;
        if(kitty_fast_enabled && !kitty_refreshing && !kitty_dirty && !moved && same_place &&
           is->channels==3 && alone) { kitty_fast|=1u<<i;continue; }
        kitty_forget_rows(s,was);kitty_forget_rows(s,is);
    }
    for(r=0;r<s->rows && r<MAX_ROWS;++r)
        if(((kitty_dirty || moved) && (rows[r] || kitty_rows_prev[r])) || rows[r]!=kitty_rows_prev[r])
            paint_damage_forget_row(&paint_damage,r);
    memcpy(kitty_rows_prev,rows,sizeof rows);
    memcpy(kitty_shown,now,sizeof now);
    kitty_scroll_seen=s->scroll_count;kitty_alternate_seen=s->alternate_active;kitty_view_seen=s->view_offset;
    kitty_dirty=0;
}
static void kitty_mark_rows(unsigned i,long first,long end)
{
    long r;
    if(kitty_ok_serial[i]!=kitty.images[i].serial) {
        memset(kitty_ok_rows[i],0,sizeof kitty_ok_rows[i]);kitty_ok_serial[i]=kitty.images[i].serial;
    }
    for(r=first;r<end;++r) if(r>=0 && r<MAX_ROWS) kitty_ok_rows[i][r]=1;
}
/* Copy the part of each visible image that falls in this text row; returns
   which images it drew, to be confirmed once the row upload succeeds. */
static unsigned kitty_composite(const XTermScreen *s,int live,unsigned row,unsigned char *pixels,unsigned pitch)
{
    XKittyCursor c;unsigned i,w=xfont_width(),h=xfont_height(),drawn=0;
    if(!live) return 0;
    kitty_cursor(&c,s);
    for(i=0;i<XKITTY_MAX_IMAGES;++i) {
        const XKittyImage *image=&kitty.images[i];long first;unsigned count,y,x0,width;
        if(!xkitty_placement_rows(image,&c,&first,&count)) continue;
        if((long)row<first || (long)row>=first+(long)count) continue;
        x0=image->placed_column*w;
        if(x0>=s->columns*w) continue;
        width=image->width<s->columns*w-x0?image->width:s->columns*w-x0;
        for(y=0;y<h;++y) {
            unsigned iy=(unsigned)(((long)row-first)*h)+y;
            const unsigned char *src;unsigned char *dst=pixels+y*pitch+x0*3;
            if(iy>=image->height) break;
            src=image->pixels+(size_t)iy*image->width*image->channels;
            if(image->channels==3) memcpy(dst,src,width*3);
            else {
                unsigned x;
                for(x=0;x<width;++x) {
                    unsigned a=src[x*4+3];
                    dst[x*3]=(unsigned char)((src[x*4]*a+dst[x*3]*(255-a))/255);
                    dst[x*3+1]=(unsigned char)((src[x*4+1]*a+dst[x*3+1]*(255-a))/255);
                    dst[x*3+2]=(unsigned char)((src[x*4+2]*a+dst[x*3+2]*(255-a))/255);
                }
            }
        }
        drawn|=1u<<i;
    }
    return drawn;
}
/* Upload each fast-path image's visible rectangle straight from its pixels. */
static void kitty_blit_fast(struct RastPort *rp,const XTermScreen *s)
{
    unsigned i,w=xfont_width(),h=xfont_height();
    for(i=0;i<XKITTY_MAX_IMAGES;++i) {
        const XKittyImage *image=&kitty.images[i];const KittyShown *k=&kitty_shown[i];
        long top,end;unsigned x0,width,y0,y1;
        if(!(kitty_fast&(1u<<i))) continue;
        top=k->first<0?0:k->first;
        end=k->first+(long)k->count<(long)s->rows?k->first+(long)k->count:(long)s->rows;
        x0=k->column*w;
        if(top>=end || x0>=s->columns*w) continue;
        width=image->width<s->columns*w-x0?image->width:s->columns*w-x0;
        y0=(unsigned)((top-k->first)*h);
        y1=(unsigned)((end-k->first)*h);if(y1>image->height) y1=image->height;
        if(y0>=y1) continue;
        if(WritePixelArray(image->pixels,0,y0,image->width*3,rp,MARGIN+x0,MARGIN+(unsigned)(k->first*(long)h)+y0,
                           width,y1-y0,RECTFMT_RGB)==width*(y1-y0)) {
            kitty_mark_rows(i,top,end);++kitty_fast_blits;
        } else {
            /* Partly drawn: take the row path on the next pass. */
            long r;++kitty_fast_failed;kitty_shown[i].serial=0;
            for(r=top;r<end;++r) paint_damage_forget_row(&paint_damage,(unsigned)r);
        }
    }
}
static void kitty_count_drawn(const XTermScreen *s)
{
    unsigned i;long r;
    for(i=0;i<XKITTY_MAX_IMAGES;++i) {
        const KittyShown *k=&kitty_shown[i];int complete=1,any=0;
        if(!k->visible || k->serial!=kitty.images[i].serial || k->serial==kitty_drawn_serial[i] ||
           kitty_ok_serial[i]!=k->serial) continue;
        for(r=k->first;r<k->first+(long)k->count;++r)
            if(r>=0 && r<(long)s->rows) { any=1;if(!kitty_ok_rows[i][r]) complete=0; }
        if(any && complete) {
            kitty_drawn_serial[i]=k->serial;++kitty_drawn;DateStamp(&kitty_last_drawn);
        }
    }
}

static void draw_screen(struct Window *window)
{
    const XTermScreen *display=display_screen();
    XTermScreen screen=*display;
    struct RastPort *rp = window->RPort;
    unsigned row, col;
    unsigned w = xfont_width(), h = xfont_height();
    unsigned cursor_col=screen.cursor_column;
    int cursor=(!screen.view_offset || display==&command_view) && screen.cursor_visible;
    /* `screen` here is the local copy; the live view is anything but the
       command editor's view, and only while not scrolled into history. */
    int images_live=display!=&command_view && !screen.view_offset;
    if(!paint_damage_geometry(&paint_damage,screen.rows,screen.columns)) return;
    kitty_damage(&screen,images_live);
    if(cursor && cursor_col && !screen_view_cell(&screen,screen.cursor_row,cursor_col).width)
        --cursor_col;
    ++paint_calls;
    colours(rp, theme.background, theme.background);
    /* Never clear the text grid before uploading finished rows: that exposes
     * a blank frame on every keystroke. Only clear unused margins. */
    RectFill(rp,0,0,window->GZZWidth-1,MARGIN-1);
    RectFill(rp,0,MARGIN,MARGIN-1,window->GZZHeight-1);
    if(MARGIN+screen.columns*w<(unsigned)window->GZZWidth)
        RectFill(rp,MARGIN+screen.columns*w,MARGIN,window->GZZWidth-1,window->GZZHeight-1);
    if(MARGIN+screen.rows*h<(unsigned)window->GZZHeight)
        RectFill(rp,MARGIN,MARGIN+screen.rows*h,window->GZZWidth-1,window->GZZHeight-1);
    SetDrMd(rp, JAM2);
    for (row = 0; row < screen.rows; ++row) {
        unsigned pitch=screen.columns*w*3, y,x, images;
        for (col = 0; col < screen.columns; ++col) {
            XTermCell cell = screen_view_cell(&screen, row, col);
            int selected=selection_contains(&selection,&screen,row,col);
            paint_row[col].cell=cell;
            paint_row[col].fg=selected?theme.selection_fg:theme_fg(&theme,&cell);
            paint_row[col].bg=selected?theme.selection_bg:theme_bg(&theme,&cell);
            paint_row[col].cursor_colour=theme.cursor;
            paint_row[col].flags=(cursor && row==screen.cursor_row && col==cursor_col?XPAINT_CURSOR:0)|
                ((cell.colour_flags&XCOLOUR_UNDERLINE ||
                  (link_hovered && hyperlink_contains(&screen,&hovered_link,row,col)))
                 ?XPAINT_UNDERLINE:0);
        }
        if(!paint_damage_changed(&paint_damage,row,paint_row)) continue;
        for (col = 0; col < screen.columns; ++col) {
            uint32_t bg=paint_row[col].bg;
            for(y=0;y<h;++y) for(x=0;x<w;++x) {
                unsigned at=y*pitch+(col*w+x)*3;
                row_pixels[at]=bg>>16; row_pixels[at+1]=bg>>8; row_pixels[at+2]=bg;
            }
        }
        for (col = 0; col < screen.columns; ++col) {
            XTermCell cell=paint_row[col].cell;
            uint32_t scalars[XCLUSTER_MAX]; unsigned i,count;
            unsigned char rgb[XRASTER_MAX*XRASTER_MAX*3]; const XClusterRaster *r;
            if(!cell.width || (!cell.length && cell.ch==' ')) continue;
            count=cell.length?cell.length:1;
            if(count>XCLUSTER_MAX) continue;
            if(count==1 && cell.width==1 && boxdraw_pixel(cell.ch,w,h,0,0)>=0) {
                uint32_t fg=paint_row[col].fg;
                for(y=0;y<h;++y) for(x=0;x<w;++x) if(boxdraw_pixel(cell.ch,w,h,x,y)) {
                    unsigned at=y*pitch+(col*w+x)*3;
                    row_pixels[at]=fg>>16; row_pixels[at+1]=fg>>8; row_pixels[at+2]=fg;
                }
                continue;
            }
            scalars[0]=cell.ch;
            for(i=1;i<count;++i) scalars[i]=cell.suffix[i-1];
            r=xfont_cluster(scalars,count,cell.width);
            if(r->width>XRASTER_MAX || r->height>XRASTER_MAX ||
               r->width>screen.columns*w-col*w || r->height>h) continue;
            xfont_cluster_rgb(r,paint_row[col].fg,paint_row[col].bg,rgb);
            for(y=0;y<r->height;++y)
                memcpy(row_pixels+y*pitch+col*w*3,rgb+y*r->width*3,r->width*3);
        }
        if(h>1) for(col=0;col<screen.columns;++col)
            if(paint_row[col].flags&XPAINT_UNDERLINE) {
                uint32_t fg=paint_row[col].fg;
                for(x=0;x<w;++x) {
                    unsigned at=(h-2)*pitch+(col*w+x)*3;
                    row_pixels[at]=fg>>16; row_pixels[at+1]=fg>>8; row_pixels[at+2]=fg;
                }
            }
        for(col=0;col<screen.columns;++col) if(paint_row[col].flags&XPAINT_CURSOR) {
            unsigned width=paint_row[col].cell.width*w;
            if(width>(screen.columns-col)*w) width=(screen.columns-col)*w;
            for(x=0;x<width;++x) {
                unsigned at=(h-1)*pitch+(col*w+x)*3;
                row_pixels[at]=theme.cursor>>16;
                row_pixels[at+1]=theme.cursor>>8; row_pixels[at+2]=theme.cursor;
            }
        }
        images=kitty_composite(&screen,images_live,row,row_pixels,pitch);
        ++paint_rows;
        /* A clipped/partial upload may already have changed visible pixels. */
        paint_damage_forget_row(&paint_damage,row);
        if(WritePixelArray(row_pixels,0,0,pitch,rp,MARGIN,MARGIN+row*h,
                          screen.columns*w,h,RECTFMT_RGB)==screen.columns*w*h) {
            unsigned i;
            paint_damage_commit(&paint_damage,row,paint_row);
            flood_diag_upload(window,row,1);
            for(i=0;i<XKITTY_MAX_IMAGES;++i) if(images&(1u<<i)) kitty_mark_rows(i,row,row+1);
        } else flood_diag_upload(window,row,0);
    }
    kitty_blit_fast(rp,&screen);
    kitty_count_drawn(&screen);
}

#ifdef XTERM_DIAG_FORWARD
/* DIAGNOSTIC BUILD ONLY - never ship. Tests whether a DOS without mainline's
 * 2f662609da ("serve clients on a port of its own") overwrites fh_Type, so that
 * client packets reach the handler's process port instead of the session port.
 * Forwarding from pr_MsgPort can steal replies to this process's own DoPkt
 * calls; that is acceptable for one measurement and is why this is not a fix. */
static unsigned long diag_forwarded;
static LONG diag_types[4];
static ULONG diag_forward_signal(void)
{
    struct Process *me=(struct Process *)FindTask(NULL);
    return handler_mode && process_session.opened?1UL<<me->pr_MsgPort.mp_SigBit:0;
}
static void diag_forward(void)
{
    struct Process *me=(struct Process *)FindTask(NULL);
    struct Message *m;
    if(!diag_forward_signal()) return;
    while((m=GetMsg(&me->pr_MsgPort))) {
        struct DosPacket *p=(struct DosPacket *)m->mn_Node.ln_Name;
        unsigned i;
        for(i=0;i<4 && diag_types[i] && diag_types[i]!=p->dp_Type;++i) {}
        if(i<4 && !diag_types[i]) diag_types[i]=p->dp_Type;
        ++diag_forwarded; PutMsg(process_session.port,m);
    }
}
#else
/* Macros, not empty functions, so product builds stay byte-identical. */
#define diag_forward_signal() 0
#define diag_forward() ((void)0)
#endif

/* Measurement switches, read once at start (XTERM_TITLE_CACHE=0,
   XTERM_APC_NODRAW=0, XTERM_PUMP_BUDGET=bytes). The defaults are the
   behaviour; the variables exist to compare it with the old one in one
   build (docs/kitty-gui-work-20260929.md). */
static long env_number(const char *name,long fallback)
{
    UBYTE value[16];
    if(GetVar((CONST_STRPTR)name,value,sizeof value,GVF_GLOBAL_ONLY)<=0) return fallback;
    return atol((const char *)value);
}
static void read_switches(void)
{
    title_cache=env_number("XTERM_TITLE_CACHE",1)!=0;
    title_stats=env_number("XTERM_TITLE_STATS",0)!=0;
    apc_nodraw=env_number("XTERM_APC_NODRAW",1)!=0;
#ifdef XTERM_PTY_FRONTEND
    aros_pty_set_receive_budget((size_t)env_number("XTERM_PUMP_BUDGET",8192));
#endif
}

/* Set by the build (Makefile LIMPET_VERSION, LIMPET_DATE); the release
   packager refuses an archive whose name disagrees with this string. */
#ifndef LIMPET_VERSION
#define LIMPET_VERSION "0.0.0-dev"
#endif
#ifndef LIMPET_DATE
#define LIMPET_DATE "01.01.2026"
#endif
static const char version_tag[] __attribute__((used))="$VER: Limpet " LIMPET_VERSION " (" LIMPET_DATE ")";

enum { MENU_ABOUT=1, MENU_CLOSE, MENU_COPY, MENU_PASTE };
static struct NewMenu menu_spec[]={
    {NM_TITLE,(UBYTE *)"Project",NULL,0,0,NULL},
    {NM_ITEM,(UBYTE *)"About Limpet...",NULL,0,0,(APTR)MENU_ABOUT},
    {NM_ITEM,NM_BARLABEL,NULL,0,0,NULL},
    {NM_ITEM,(UBYTE *)"Close window",(UBYTE *)"Q",0,0,(APTR)MENU_CLOSE},
    {NM_TITLE,(UBYTE *)"Edit",NULL,0,0,NULL},
    /* the same RAmiga+C/V that worked before the menu existed */
    {NM_ITEM,(UBYTE *)"Copy",(UBYTE *)"C",0,0,(APTR)MENU_COPY},
    {NM_ITEM,(UBYTE *)"Paste",(UBYTE *)"V",0,0,(APTR)MENU_PASTE},
    {NM_END,NULL,NULL,0,0,NULL}
};

#include "limpet_icon.h"

/* DEC 2048: queue a report of the current size. A change while one is
   half sent is not lost: the newest size follows it. */
static unsigned resize_mode_seen;
static int replies_enabled(void);
static void resize_queue(void)
{
    if(!screen.resize_report || !replies_enabled()) return;
    input_order_resize(&input_order,screen.rows,screen.columns,
                       screen.rows*xfont_height(),screen.columns*xfont_width());
}
static size_t order_sink(void *context,const unsigned char *bytes,size_t length)
{ return aros_session_write(context,bytes,length); }

static const char *const about_lines[]={
    "Limpet " LIMPET_VERSION,
#ifdef XTERM_PTY_FRONTEND
    "PTY preview for mainline AROS",
#endif
    "",
    "A terminal for the AROS Shell, written for AROS:",
    "Unicode, colour, scrollback, copy and paste,",
    "full-screen programs and inline images.",
    "",
    "Tomasz Staniak",
    "https://tomaszstaniak.com",
    "https://github.com/tomaszstaniak/limpet",
    "",
    "MIT licence. Bundled components and fonts:",
    "see the Licenses drawer.",
    "",
    "Keys:",
    "  Drag selects; RAmiga+C / Ctrl+Shift+C copies,",
    "  RAmiga+V / Ctrl+Shift+V pastes (F3 confirms",
    "  several lines, Esc cancels a paste).",
    "  Shift+PgUp/PgDn or the wheel: scrollback.",
    "  Up/Down: command history; Ctrl-R searches it.",
    "  TAB / Shift+TAB: complete a path.",
    "  Ctrl-click a link opens it; Ctrl+Shift-click",
    "  copies it. Ctrl-C interrupts; F5 ends input.",
};

/* Modal: the terminal loop waits while it is open, as with a requester. */
static void show_about(struct Window *parent)
{
    struct Screen *scr=parent->WScreen;
    struct DrawInfo *dri=GetScreenDrawInfo(scr);
    struct RastPort *rp; struct Window *w;
    UWORD bgpen=dri?dri->dri_Pens[BACKGROUNDPEN]:0,txpen=dri?dri->dri_Pens[TEXTPEN]:1;
    ULONG rgb[3]; unsigned char *pix; unsigned i,n=sizeof about_lines/sizeof *about_lines;
    unsigned tw=0,lh,pad=12,width,height;
    /* measure with the font the window will draw in (the screen's font),
       not whatever the screen's own RastPort happens to hold */
    struct TextFont *font=OpenFont(scr->Font); struct RastPort measure;
    InitRastPort(&measure); if(font) SetFont(&measure,font);
    lh=measure.TxHeight+3;
    for(i=0;i<n;++i) {
        unsigned l=TextLength(&measure,(CONST_STRPTR)about_lines[i],strlen(about_lines[i]));
        if(l>tw) tw=l;
    }
    tw+=8;
    width=pad+LIMPET_ICON_SIZE+pad+tw+pad;
    height=pad+(n*lh>LIMPET_ICON_SIZE?n*lh:LIMPET_ICON_SIZE)+pad;
    w=OpenWindowTags(NULL,WA_CustomScreen,(IPTR)scr,WA_Title,(IPTR)"About Limpet",
        WA_InnerWidth,width,WA_InnerHeight,height,
        WA_Left,parent->LeftEdge+40,WA_Top,parent->TopEdge+40,
        WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_MOUSEBUTTONS|IDCMP_VANILLAKEY,
        WA_Flags,WFLG_CLOSEGADGET|WFLG_DRAGBAR|WFLG_DEPTHGADGET|WFLG_ACTIVATE|
                 WFLG_GIMMEZEROZERO|WFLG_RMBTRAP,TAG_DONE);
    if(!w) { if(font) CloseFont(font); if(dri) FreeScreenDrawInfo(scr,dri); return; }
    rp=w->RPort; if(font) SetFont(rp,font); SetRast(rp,bgpen);
    /* blend the icon's alpha against this screen's background pen */
    GetRGB32(scr->ViewPort.ColorMap,bgpen,1,rgb);
    pix=AllocVec(LIMPET_ICON_SIZE*LIMPET_ICON_SIZE*3,MEMF_ANY);
    if(pix) {
        for(i=0;i<LIMPET_ICON_SIZE*LIMPET_ICON_SIZE;++i) {
            const unsigned char *s=limpet_icon_rgba+i*4; unsigned a=s[3],c;
            for(c=0;c<3;++c) pix[i*3+c]=(s[c]*a+(rgb[c]>>24)*(255-a))/255;
        }
        WritePixelArray(pix,0,0,LIMPET_ICON_SIZE*3,rp,pad,pad,
                        LIMPET_ICON_SIZE,LIMPET_ICON_SIZE,RECTFMT_RGB);
        FreeVec(pix);
    }
    SetAPen(rp,txpen); SetDrMd(rp,JAM1);
    for(i=0;i<n;++i) {
        Move(rp,pad+LIMPET_ICON_SIZE+pad,pad+i*lh+rp->TxBaseline);
        Text(rp,(CONST_STRPTR)about_lines[i],strlen(about_lines[i]));
    }
    for(;;) {
        struct IntuiMessage *m; int done=0;
        WaitPort(w->UserPort);
        while((m=(struct IntuiMessage *)GetMsg(w->UserPort))) {
            if(m->Class==IDCMP_CLOSEWINDOW || m->Class==IDCMP_VANILLAKEY ||
               (m->Class==IDCMP_MOUSEBUTTONS && m->Code==SELECTUP)) done=1;
            ReplyMsg((struct Message *)m);
        }
        if(done) break;
    }
    CloseWindow(w);
    if(font) CloseFont(font);
    if(dri) FreeScreenDrawInfo(scr,dri);
}

static struct Menu *menu_strip;
static APTR menu_visual;

static void attach_menu(struct Window *window)
{
    /* A terminal without a menu still works; the shortcuts stay keys. */
    menu_visual=GetVisualInfoA(window->WScreen,NULL);
    if(menu_visual) menu_strip=CreateMenusA(menu_spec,NULL);
    if(menu_strip && LayoutMenusA(menu_strip,menu_visual,NULL)) SetMenuStrip(window,menu_strip);
    else if(menu_strip) { FreeMenus(menu_strip); menu_strip=NULL; }
}

static void close_main_window(struct Window *window)
{
    if(menu_strip) { ClearMenuStrip(window); FreeMenus(menu_strip); menu_strip=NULL; }
    if(menu_visual) { FreeVisualInfo(menu_visual); menu_visual=NULL; }
    CloseWindow(window);
}

/* Title changes are applied at most once per tick (250 ms): a program
   that sends a title with every write must not make the title bar flicker
   or cost a redraw of it per token. The latest text wins. */
static int title_hold,title_pending;

static void update_title(struct Window *window, unsigned last_key)
{
    /* Intuition holds the shown buffer and may draw from it at any time
       (src/title_buffer.h): compose in the other one, hand it over only
       when the text changes. */
    char *next=title_buffers_next(&titles);
    const size_t cap=sizeof titles.text[0];
    XWindowTitle in;
    (void)last_key;
    /* The copy hint describes a live selection; whatever cleared the
       selection (typing, output, resize) retires the hint too. */
    if(clip_status && !selection.active && !strcmp(clip_status,"Selected"))
        clip_status=NULL;
    /* What runs, then what the terminal reports (src/window_title.h). */
    memset(&in,0,sizeof in);
    in.terminal=pty_mode?"Limpet (PTY preview)":"Limpet";
    in.link=link_hovered?hovered_link.url:NULL;
    in.program=program_title_text(&program_title);
    in.command=foreground.running?foreground.name:NULL;
    in.shell=shell_session && echo_mode==2 && !process_session.exited;
    in.status=clip_status;
    in.view_offset=screen.view_offset; in.history_count=screen.history_count;
    window_title_compose(next,cap,&in);
#ifdef XTERM_PKTWAIT_PROBE
    {
        size_t used=strlen(next);
        snprintf(next+used,cap-used," | pktwait %lu fontmem %luK",
                 aros_handler_pktwaits(),(unsigned long)(xfont_resident_bytes()/1024));
    }
#endif
#ifdef XTERM_DIAG_FORWARD
    {
        size_t used=strlen(next);
        snprintf(next+used,cap-used," | diag fwd %lu types %ld,%ld,%ld,%ld",
                 diag_forwarded,(long)diag_types[0],(long)diag_types[1],(long)diag_types[2],(long)diag_types[3]);
    }
#endif
    if(title_stats && kitty.stats.commands) {
        size_t used=strlen(next);
        snprintf(next+used,cap-used," | img rx %lu drawn %lu %luK",
                 kitty.stats.frames_received,kitty_drawn,(unsigned long)(kitty.stats.memory_bytes/1024));
    }
    if(title_cache && !strcmp(next,title_buffers_shown(&titles))) { ++title_skipped;title_pending=0;return; }
    if(title_hold) { title_pending=1;return; }
    title_buffers_commit(&titles);
    ++title_updates;title_hold=1;title_pending=0;
    SetWindowTitles(window, (CONST_STRPTR)title_buffers_shown(&titles), (CONST_STRPTR)-1);
}

static size_t session_bytes(const unsigned char *bytes,size_t count)
{
    size_t used=0,n;
    if(echo_mode==2) {
        if(replies_enabled() && reply_flow.length) return 0;
        if(shell_session && !process_session.line.raw) {
            if(count==1 && bytes[0]==3) {
                if(!aros_session_interrupt(&process_session)) return 0;
                line_editor_search_cancel(&command_editor);
                screen.cluster_active=0;
                screen_write(&screen,(const unsigned char *)command_editor.text,command_editor.length);
                line_editor_action(&command_editor,XLINE_CLEAR);
                selection.active=selection.dragging=0;
                return 1;
            }
            if(!editor_ready()) return 0;
            /* Searching: text goes to the query; Enter takes the match into
             * the line for editing and runs nothing. */
            if(command_editor.searching) {
                while(used<count && bytes[used]!='\r' && bytes[used]!='\n') ++used;
                if(used && !line_editor_search_insert(&command_editor,(const char *)bytes,used)) return 0;
                if(used<count) { line_editor_search_accept(&command_editor); ++used; }
                editor_sync();
                return used;
            }
            /* Paste is fed up to a line boundary. The next command waits for
             * the next cooked Read so it cannot race raw application startup. */
            while(used<count && bytes[used]!='\r' && bytes[used]!='\n') ++used;
            if(used) {
                if(!line_editor_insert(&command_editor,(const char *)bytes,used)) return 0;
                editor_sync();
            }
            if(used<count && editor_submit()) ++used;
            return used;
        }
        /* Keep key/paste batches atomic, including UTF-8 scalar boundaries. */
        if(!(count==1 && bytes[0]==3) && count>XSESSION_CAPACITY-process_session.input.length) return 0;
        used=aros_session_write(&process_session,bytes,count);
        if(used) {
            selection.active=selection.dragging=0;
            screen_scroll_view(&screen,-(int)screen.view_offset);
        }
        return used;
    }
    if(echo_mode!=1) return 0;
    selection.active=selection.dragging=0;
    screen_scroll_view(&screen,-(int)screen.view_offset);
    do {
        used+=echo_session_write(&echo,bytes+used,count-used);
        { unsigned char output[256];
          while((n=echo_session_read(&echo,output,sizeof output)))
              screen_write(&screen,output,n); }
    } while(used<count);
    return used;
}

static int keyboard_interrupt(const XQueuedKey *key)
{
    return echo_mode==2 && shell_session && !process_session.line.raw &&
        key->length==1 && key->bytes[0]==3;
}

static int keyboard_ready(void)
{
    const XQueuedKey *key;
    if(!keyboard.count) return 0;
    key=&keyboard.events[keyboard.head];
    if(!echo_mode || (echo_mode==2 &&
       (!aros_session_active(&process_session) || process_session.eof))) return 1;
    if(keyboard_interrupt(key))
        return !process_session.breaker.registered ||
            (aros_break_ready(&process_session.breaker) && XSESSION_CAPACITY-process_session.output.length>=4);
    if(paste.length || (clip_job.task_id && !clip_job.writing)) return 0;
    if(echo_mode!=2) return 1;
    if(!input_order_others_may_send(&input_order)) return 0;
    if(replies_enabled() && reply_flow.length) return 0;
    if(shell_session && !process_session.line.raw) return editor_ready();
    return key->length-key->offset<=XSESSION_CAPACITY-process_session.input.length;
}

static int keyboard_deliver(void *unused,XQueuedKey *key)
{
    size_t used;
    (void)unused;
    if(!keyboard_ready()) return 0;
    if(!echo_mode || (echo_mode==2 &&
       (!aros_session_active(&process_session) || process_session.eof))) {
        clip_status="Session ended; queued key discarded"; return -1;
    }
    /* Ctrl-R in the native line editor: reverse history search. A raw-mode
       program (editor not ready) gets the byte 0x12 as before. */
    if(!key->offset && key->length==1 && key->bytes[0]==0x12 && editor_ready()) {
        completion_active=0;
        line_editor_search_start(&command_editor); editor_sync(); return 1;
    }
    if(!key->offset && editor_key(key->code,key->qualifiers)) return 1;
    if(!key->length) return 1;
    completion_active=0;
    used=session_bytes(key->bytes+key->offset,key->length-key->offset);
    key->offset+=used;
    if(key->offset==key->length) return 1;
    if(!used && (editor_ready() || (keyboard_interrupt(key) &&
                                  !process_session.breaker.registered))) {
        clip_status="Key rejected: editor limit/unsupported input or interrupt";
        return -1;
    }
    return 0;
}

static void keyboard_submit(UWORD code,UWORD q,const unsigned char *bytes,size_t count)
{
    if(echo_mode==2 && shell_session && !process_session.line.raw && count==1 && bytes[0]==3) {
        /* Interruption cancels preceding typeahead/paste, but raw Ctrl-C is
         * ordinary ordered input. A blocked interrupt stays at the head. */
        key_queue_clear(&keyboard); paste_cancel(&paste); ++session_generation;
    }
    if(!key_queue_add(&keyboard,code,q,bytes,count))
        clip_status="Keyboard queue full; new key rejected";
}

static int clipboard_shortcut(const ArosRawKey *key)
{
    ArosRawKey mapped=*key; unsigned char bytes[8]; UWORD q=key->event.ie_Qualifier;
    if(!(q&IEQUALIFIER_RCOMMAND) &&
       !((q&IEQUALIFIER_CONTROL) && (q&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)))) return 0;
    if(q&(IEQUALIFIER_LALT|IEQUALIFIER_RALT)) return 0;
    mapped.event.ie_Qualifier=0;
    if(aros_input_encode(&mapped,bytes,sizeof bytes)!=1) return 0;
    return bytes[0]=='c'?1:bytes[0]=='v'?2:0;
}

static void clipboard_action(int action)
{
    size_t text_length,raw_length;
    if(keyboard.count) { clip_status="Clipboard waits for typeahead; retry shortcut"; return; }
    if(clip_job.task_id || paste.length || input_order_paste_busy(&input_order)) {
        clip_status="Clipboard busy (Esc cancels queued paste)"; return;
    }
    if(action==1) {
        int result=0;
        if(editor_ready() && command_editor.selecting) {
            size_t a=command_editor.anchor,b=command_editor.cursor;
            if(a>b) { size_t t=a;a=b;b=t; }
            text_length=b-a;
            memcpy(clip_text,command_editor.text+a,text_length);
        } else result=selection_text(&selection,display_screen(),clip_text,sizeof clip_text,&text_length);
        if(result || !text_length) { clip_status=result==-2?"Selection too large":"Select text first"; return; }
        if(clip_encode_utf8(clip_text,text_length,clip_raw,sizeof clip_raw,&raw_length,&copy_loss)) {
            clip_status="Copy exceeds clipboard limit"; return;
        }
        if(aros_clip_job_start(&clip_job,1,0,clip_raw,raw_length)) clip_status="Cannot start clipboard writer";
        else clip_status="Copying...";
    } else {
        if(!echo_mode || (echo_mode==2 && process_session.eof)) {
            clip_status="Open an input session with F2 or F4"; return;
        }
        paste_generation=session_generation;
        if(aros_clip_job_start(&clip_job,0,0,NULL,0)) clip_status="Cannot start clipboard reader";
        else clip_status="Reading clipboard...";
    }
}

static void clipboard_completed(int closing)
{
    size_t length; unsigned format;
    if(closing) return;
    if(clip_job.result) { clip_status="Clipboard I/O failed or clip too large"; return; }
    if(clip_job.writing) {
        clip_status=copy_loss?"Copied UTF-8 (legacy fallback loses characters)":"Copied";
        return;
    }
    if(paste_generation!=session_generation || !echo_mode ||
       (echo_mode==2 && process_session.eof)) { clip_status="Paste cancelled: session changed"; return; }
    if(!clip_job.length) { clip_status="Clipboard empty"; return; }
    if(clip_decode_text(clip_job.bytes,clip_job.length,clip_text,sizeof clip_text,&length,&format)) {
        clip_status="Clipboard is not supported text"; return;
    }
    if(paste_prepare(&paste,clip_text,length)) { clip_status="Paste rejected: invalid text or controls"; return; }
    /* A raw-mode client that turned on DEC 2004 receives the paste framed
       and decides itself what a newline in it means, so the multi-line
       confirmation would only be in its way. */
    if(paste.length && screen.bracketed_paste && process_session.line.raw) {
        /* The previous paste's end marker has not gone out yet. */
        if(input_order_paste_busy(&input_order)) { paste_cancel(&paste); clip_status="Clipboard busy"; return; }
        input_order_begin_bracketed(&input_order);
    }
    clip_status=paste.confirmation?"MULTILINE: F3 paste / Esc cancel":paste.length?"Pasting... (Esc cancels)":"Clipboard empty";
}

static int mouse_cell(struct Window *window,WORD x,WORD y,unsigned *row,unsigned *col,int clamp)
{
    XSelectionGrid grid={window->BorderLeft+MARGIN,window->BorderTop+MARGIN,xfont_width(),xfont_height()};
    return selection_hit(display_screen(),&grid,x,y,clamp,row,col);
}

static void link_action(int action,const char *url)
{
    size_t raw_length;
    if(action==2) {
        if(clip_job.task_id || paste.length) { clip_status="Clipboard busy"; return; }
        if(clip_encode_utf8((const unsigned char *)url,strlen(url),clip_raw,
                           sizeof clip_raw,&raw_length,&copy_loss)) {
            clip_status="Link exceeds clipboard limit"; return;
        }
        clip_status=aros_clip_job_start(&clip_job,1,0,clip_raw,raw_length)?
            "Cannot start clipboard writer":"Copying link...";
    } else if(action==1) {
        if(!hyperlink_open_safe(url)) {
            clip_status="Link cannot be opened safely; Ctrl+Shift-click copies"; return;
        }
        if(link_job.task_id) { clip_status="Link opener busy"; return; }
        clip_status=aros_hyperlink_start(&link_job,url)?
            "Cannot start link opener":"Opening link...";
    }
}

/* A 250 ms tick, armed only while something time-based is pending:
   following a running command, a held title change, a message to expire. */
static struct MsgPort *tick_port;
static struct timerequest *tick_io;
static int tick_ready,tick_armed;
static void tick_open(void)
{
    if(!(tick_port=CreateMsgPort())) return;
    if(!(tick_io=(struct timerequest *)CreateIORequest(tick_port,sizeof *tick_io)) ||
       OpenDevice((CONST_STRPTR)TIMERNAME,UNIT_VBLANK,(struct IORequest *)tick_io,0)) {
        if(tick_io) DeleteIORequest((struct IORequest *)tick_io);
        DeleteMsgPort(tick_port); tick_port=NULL; tick_io=NULL; return;
    }
    tick_ready=1;
}
static void tick_arm(void)
{
    if(!tick_ready || tick_armed) return;
    tick_io->tr_node.io_Command=TR_ADDREQUEST;
    tick_io->tr_time.tv_secs=0; tick_io->tr_time.tv_micro=250000;
    SendIO((struct IORequest *)tick_io); tick_armed=1;
}
static int tick_fired(void)
{
    if(!tick_armed || !CheckIO((struct IORequest *)tick_io)) return 0;
    WaitIO((struct IORequest *)tick_io); tick_armed=0;
    return 1;
}
static ULONG tick_signal(void)
{ return tick_ready?1UL<<tick_port->mp_SigBit:0; }
static void tick_close(void)
{
    if(!tick_ready) return;
    if(tick_armed) { AbortIO((struct IORequest *)tick_io); WaitIO((struct IORequest *)tick_io); }
    CloseDevice((struct IORequest *)tick_io);
    DeleteIORequest((struct IORequest *)tick_io); DeleteMsgPort(tick_port);
    tick_ready=tick_armed=0;
}

/* Read the foreground again; a program's title ends when its command does.
   Returns nonzero when what the title shows may have changed. */
static int foreground_poll(void)
{
    XForeground now;
    int changed;
    if(echo_mode==2 && !process_session.exited) aros_session_foreground(&process_session,&now);
    else memset(&now,0,sizeof now);
    /* An unreadable moment (no reader bound) is no evidence of a change;
       keep what was read last until the reader can be read again. */
    if(!now.known && foreground.known && echo_mode==2 && !process_session.exited) return 0;
    changed=now.known!=foreground.known || now.running!=foreground.running ||
            now.module!=foreground.module || strcmp(now.name,foreground.name);
    foreground=now;
    if(program_title_follow(&program_title,&foreground)) changed=1;
    return changed;
}

/* Messages that report something done go after a few seconds; those that
   ask for an action or describe one in progress stay until it changes. */
static const char *status_seen;
static unsigned status_age;
static int status_lasting(const char *s)
{
    static const char *const lasting[]={"MULTILINE","Pasting","Copying","Reading clipboard",
        "Closing","TAB: searching","Selected","Session ended","Cannot start session",
        "Opening link","Copying link"};
    unsigned i;
    for(i=0;i<sizeof lasting/sizeof *lasting;++i)
        if(!strncmp(s,lasting[i],strlen(lasting[i]))) return 1;
    return 0;
}
static int status_expiring(void)
{ return clip_status && !status_lasting(clip_status); }

int main(int argc,char **argv)
{
    struct Window *window;
    int running = TRUE;
    int closing = FALSE;
    unsigned last_key = 0;
    theme_default(&theme);
    paint_damage_init(&paint_damage,paint_cache,MAX_ROWS*MAX_COLUMNS);
    input_order_init(&input_order,&paste);
#ifdef XTERM_DOS_HANDLER
    XHandlerContext *handler=aros_handler_context();
    (void)argc; (void)argv;
    if(!handler) return RETURN_FAIL;
    shell_session=1;
#else
    /* Theme is a leading option; remaining arguments retain their strict grammar. */
    if(argc>1 && !strcmp(argv[1],"--theme")) {
        int i;
        if(argc<3 || !*argv[2]) {
            puts("--theme requires a file path"); return RETURN_ERROR;
        }
        if(theme_load(&theme,argv[2]))
            puts("Cannot load theme; using built-in dark theme");
        for(i=1;i<argc-2;++i) argv[i]=argv[i+2];
        argc-=2;
    }
    if(!pty_mode && aros_session_peer_command(peer_command,sizeof peer_command)) {
        puts("Cannot resolve session-peer beside this executable");
        return RETURN_ERROR;
    }

    if(pty_mode) {
        /* --shell starts an AROS Shell on the PTY; the PTY does the line
           discipline, so shell_session (this frontend's own line editor for
           DOS-console sessions) stays off. A NULL command selects it. */
        /* no arguments, or a start from Workbench (argc 0): a Shell */
        if(argc<=1 || (argc==2 && !strcmp(argv[1],"--shell"))) process_command=NULL;
        else if(argc==3 && !strcmp(argv[1],"--command") && *argv[2] && strlen(argv[2])<512)
            process_command=argv[2];
        else {
            puts("Usage: LimpetPTY [--theme FILE] [--shell | --command EXECUTABLE]");return RETURN_ERROR;
        }
    }
    /* no arguments, or a start from Workbench (argc 0): a Shell */
    else if(argc<=1 || (argc==2 && !strcmp(argv[1],"--shell"))) shell_session=1;
    else if(argc==3 && !strcmp(argv[1],"--shell") && !strcmp(argv[2],"--no-startup")) {
        shell_session=1; shell_profile="";
    } else if(argc==4 && !strcmp(argv[1],"--shell") && !strcmp(argv[2],"--from") && *argv[3]) {
        shell_session=1; shell_profile=argv[3];
    }
    else if(argc==3 && !strcmp(argv[1],"--command") && *argv[2] && strlen(argv[2])<512)
        process_command=argv[2];
    else {
        puts("Usage: Limpet [--theme FILE] [--shell [--from FILE | --no-startup] | --command COMMAND]");
        return RETURN_ERROR;
    }
#endif
    read_switches();
#ifdef XTERM_PTY_DIAG
    /* DIAGNOSTIC BUILD ONLY: leave early at a chosen stage, to find which
       part of a run keeps memory (docs/release-resources-20260929.md). */
    diag_exit_stage=(int)env_number("XTERM_DIAG_EXIT_STAGE",0);
    if(diag_exit_stage==1) return RETURN_OK;
#endif
    screen_init(&screen, cells, MAX_ROWS * MAX_COLUMNS, 24, 80);
    reset_links();
    if(!handler_mode) replay();
    if (xfont_open()) {
        /* FreeType/Intuition do not establish DOS IoErr for the launcher. */
        if(handler_mode) SetIoErr(ERROR_OBJECT_WRONG_TYPE);
        return RETURN_FAIL;
    }
#ifdef XTERM_PTY_DIAG
    if(diag_exit_stage==2) { xfont_close();return RETURN_OK; }
#endif
    window = OpenWindowTags(NULL,
        WA_Title, (IPTR)"Limpet",
        WA_Left, 40, WA_Top, 60,
        WA_InnerWidth, 80 * xfont_width() + 2 * MARGIN,
        WA_InnerHeight, 24 * xfont_height() + 2 * MARGIN,
        WA_MinWidth, 240, WA_MinHeight, 100,
        WA_MaxWidth, MAX_COLUMNS * xfont_width() + 2 * MARGIN,
        WA_MaxHeight, MAX_ROWS * xfont_height() + 2 * MARGIN,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_NEWSIZE | IDCMP_RAWKEY |
                  IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_INACTIVEWINDOW | IDCMP_MENUPICK,
        WA_Flags, WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET |
                  WFLG_SIZEGADGET | WFLG_SIZEBBOTTOM | WFLG_ACTIVATE |
                  WFLG_GIMMEZEROZERO | WFLG_SIMPLE_REFRESH | WFLG_REPORTMOUSE,
        TAG_DONE);
    if (!window) {
        xfont_close();
        if(handler_mode) SetIoErr(ERROR_NO_FREE_STORE);
        return RETURN_FAIL;
    }
    attach_menu(window);
#ifdef XTERM_PTY_DIAG
    if(diag_exit_stage==3) { close_main_window(window);xfont_close();return RETURN_OK; }
#endif
    if(shell_session || pty_mode) {
#ifdef XTERM_DOS_HANDLER
        if(aros_session_start_endpoint(&process_session) ||
           aros_handler_accept(handler,&process_session)) {
            LONG error=IoErr();
            aros_session_stop(&process_session);
            aros_session_dispose(&process_session);
            close_main_window(window); xfont_close(); SetIoErr(error); return RETURN_FAIL;
        }
#else
        if(pty_mode?aros_session_start(&process_session,process_command):
           aros_session_start_shell_profile(&process_session,shell_profile)) {
            PrintFault(IoErr(),(CONST_STRPTR)(pty_mode?"Cannot start PTY executable":"Cannot start Shell/profile"));
            close_main_window(window); xfont_close(); return RETURN_FAIL;
        }
#endif
        {
            echo_mode=2;
            screen_init(&screen,cells,MAX_ROWS*MAX_COLUMNS,screen.rows,screen.columns);
            screen.amiga_pens=!pty_mode;
            reset_links();
            screen_attach_history(&screen,history,HISTORY_LINES*MAX_COLUMNS);
            screen_attach_reflow(&screen,history_spare,HISTORY_LINES*MAX_COLUMNS);
            screen_attach_alternate(&screen,alternate,MAX_ROWS*MAX_COLUMNS);
            clip_status=NULL;
        }
    }
    tick_open();
    update_title(window, last_key);
    draw_screen(window);
    if(!flood_diag_start(window)) {
        tick_close();
        close_main_window(window);xfont_close();return RETURN_FAIL;
    }
    while (running) {
        struct IntuiMessage *message;
        int dirty=0;
        replies_sync();
        int paste_ready=paste.length && !paste.confirmation &&
            !reply_flow.length &&
            (echo_mode!=2 || (!process_session.eof &&
             XSESSION_CAPACITY-process_session.input.length>=1024 &&
             (!shell_session || process_session.line.raw || editor_ready())));
        if(!paste_ready && !keyboard_ready() && !flood_diag_ready() && !(echo_mode==2 && output_ready()))
            Wait((1UL<<window->UserPort->mp_SigBit)|SIGF_CHILD|tick_signal()|
                 aros_session_signals(&process_session)|diag_forward_signal()|flood_diag_signals());
        diag_forward();
        process_session.columns=screen.columns;
        process_session.rows=screen.rows;
        process_session.presentation_enabled=1;
        process_session.amiga_pens=screen.amiga_pens;
        aros_session_poll(&process_session);
        flood_diag_queues();
        if(echo_mode==2 && !closing) {
            unsigned long seen=screen.visible_bytes;
            XSelectionMark scrolled=selection_mark(&screen);
            /* The selection's text before the output, to see whether the output
               touched it. On the heap and sized to the screen: a selection of a
               whole reply is easily more than a few KB. Also while dragging:
               output does not move the selection's ends, the mouse does. */
            /* Only when output is waiting: no copy on every idle pass. */
            int pending=process_session.output.length || output_offset<output_length;
            size_t kept_cap=selection.active && pending ? (size_t)screen.rows*screen.columns*8+8 : 0, kept_length=0;
            unsigned char *kept=kept_cap ? malloc(kept_cap) : NULL;
            int keep=kept && !selection_text(&selection,&screen,kept,kept_cap,&kept_length);
            size_t n=output_pump();
            /* Bytes that only fed an image still arriving change nothing on
               screen: no redraw, and a selection or link hover stays. A
               finished image (or any Kitty cursor move) sets kitty_shown_change. */
            if(n && apc_nodraw && screen.visible_bytes==seen && !kitty_shown_change) { ++output_quiet;n=0; }
            kitty_shown_change=0;
            if(n) {
                ++output_redraws;
                link_gesture_cancel(&link_gesture); link_hovered=0;
                /* A redraw of other rows (a status line, a clock) leaves the
                   selection; output that changes or scrolls it drops it (L6). */
                if(!(keep && selection_keep(&selection,&screen,kept,kept_length,&scrolled)))
                    selection.active=selection.dragging=0;
                dirty=1;
            }
            free(kept);
            /* A new OSC title belongs to whatever runs right now; read the
               foreground first, it may have started since the last look. */
            if(screen.title_serial!=program_title.serial) {
                foreground_poll();
                program_title_offer(&program_title,screen.title,screen.title_serial,&foreground);
                dirty=1;
            }
            if(foreground_poll()) dirty=1;
            if(process_session.exited && !process_session.output.length &&
               output_offset==output_length && !process_reported) {
                if(handler_mode) closing=TRUE;
                else {
                    char status[80];
                    if(pty_mode) write_text(XTERM_SESSION_RESET);
                    /* In PTY Shell mode the value is the supervisor's (0: the Shell
                       started and ended), not the Shell's own last return code. */
                    if(pty_mode && !process_command)
                        snprintf(status,sizeof status,process_session.result?"\r\n[PTY Shell did not start; status: %ld]\r\n":"\r\n[PTY Shell ended]\r\n",(long)process_session.result);
                    else
                    snprintf(status,sizeof status,pty_mode?"\r\n[PTY session ended; status: %ld]\r\n":"\r\n[Session ended; Shell status: %ld]\r\n",(long)process_session.result);
                    write_text(status);
                    clip_status="Session ended (F4 restarts)";
                }
                process_reported=1;
#ifdef XTERM_PTY_DIAG
                /* Close by itself after the session, so memory can be
                   measured around a group of complete runs. */
                if(diag_exit_stage==4) { closing=TRUE;aros_session_stop(&process_session); }
#endif
                /* The session's images end with it; record them first. */
                kitty_write_stats();xkitty_reset(&kitty);kitty_forget_state();
                program_title_clear(&program_title); memset(&foreground,0,sizeof foreground);
                paste_cancel(&paste); key_queue_clear(&keyboard); ++session_generation;
                dirty=1;
            }
        }
        mouse_flush();
        if(clip_status!=status_seen) { status_seen=clip_status; status_age=0; }
        if(tick_fired()) {
            if(title_pending) dirty=1;
            title_hold=0;
            if(foreground_watch) --foreground_watch;
            if(status_expiring() && ++status_age>=16) { clip_status=status_seen=NULL; dirty=1; }
        }
        if(aros_clip_job_poll(&clip_job)) { clipboard_completed(closing); dirty=1; }
#ifdef XTERM_PROCESS_PORT_CLIENTS
        if(aros_completion_job_poll(&completion_job)) {
            if(stop_after_completion) { stop_after_completion=0; aros_session_stop(&process_session); }
            else if(!closing) completion_finished();
            dirty=1;
        }
#endif
        if(aros_hyperlink_poll(&link_job)) {
            clip_status=link_job.result==0?"Link handed to OpenURL":
                link_job.result==-1?"openurl.library unavailable; Ctrl+Shift-click copies":
                "OpenURL failed; check browser preferences";
            link_hovered=0; dirty=1;
        }
        while ((message = (struct IntuiMessage *)GetMsg(window->UserPort))) {
            ULONG event = message->Class;
            UWORD code = message->Code;
            WORD mx=message->MouseX,my=message->MouseY;
            UWORD mouse_qualifiers=message->Qualifier;
            ArosRawKey key;
            int have_key=aros_input_capture(&key,message);
            ReplyMsg((struct Message *)message);
            flood_diag_event(event);
            if (event == IDCMP_MENUPICK) {
                UWORD pick=code; int close_now=0;
                while(pick!=MENUNULL && menu_strip) {
                    struct MenuItem *item=ItemAddress(menu_strip,pick);
                    if(!item) break;
                    switch((IPTR)GTMENUITEM_USERDATA(item)) {
                    case MENU_ABOUT: show_about(window); break;
                    case MENU_CLOSE: close_now=1; break;
                    case MENU_COPY: if(!closing) clipboard_action(1); break;
                    case MENU_PASTE: if(!closing) clipboard_action(2); break;
                    }
                    pick=item->NextSelect;
                }
                dirty=1;
                if(!close_now) continue;
                event=IDCMP_CLOSEWINDOW;
            }
            if (event == IDCMP_CLOSEWINDOW) {
                output_reset();
                link_gesture_cancel(&link_gesture); link_hovered=0;
                closing=TRUE; paste_cancel(&paste); key_queue_clear(&keyboard); ++session_generation;
#ifdef XTERM_PROCESS_PORT_CLIENTS
                /* Stopping replies the Shell's Read; the TAB child still reads
                 * its dirs. Stop when the child has gone. */
                if(completion_job.task_id) stop_after_completion=1;
                else
#endif
                aros_session_stop(&process_session);
                selection.dragging=0;
                clip_status="Closing: waiting for process / clipboard"; dirty=1;
            }
            else if (event == IDCMP_REFRESHWINDOW) {
                paint_damage_invalidate(&paint_damage);
                BeginRefresh(window); kitty_refreshing=1; draw_screen(window); kitty_refreshing=0; EndRefresh(window, TRUE);
            } else if (closing) continue;
            else if(event==IDCMP_INACTIVEWINDOW) {
                unsigned button;
                link_gesture_cancel(&link_gesture); link_hovered=0; dirty=1;
                selection.dragging=0;
                for(button=0;button<3;++button)
                    mouse_release_button(button,mouse_last_col,mouse_last_row,0);
                mouse_position_valid=0;
            }
            else if(event==IDCMP_MOUSEBUTTONS || event==IDCMP_MOUSEMOVE) {
                unsigned row,col;
                unsigned button=code==SELECTDOWN || code==SELECTUP?0:
                    code==MIDDLEDOWN || code==MIDDLEUP?1:
                    code==MENUDOWN || code==MENUUP?2:99;
                int release=event==IDCMP_MOUSEBUTTONS &&
                    (code==SELECTUP || code==MIDDLEUP || code==MENUUP);
                int captured=button<3 && (mouse_buttons&(1u<<button));
                int ctrl=(mouse_qualifiers&IEQUALIFIER_CONTROL)!=0;
                int shift=(mouse_qualifiers&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT))!=0;
                /* A release outside the inactive window may never arrive. */
                if(event==IDCMP_MOUSEBUTTONS && code==SELECTDOWN)
                    link_gesture_new_press(&link_gesture);
                if(mouse_qualifiers&(IEQUALIFIER_LALT|IEQUALIFIER_RALT)) ctrl=0;
                if(!selection.dragging && !mouse_buttons) {
                    int was_hovered=link_hovered;
                    XHyperlinkHit previous=hovered_link;
                    link_hovered=mouse_cell(window,mx,my,&row,&col,0) &&
                        hyperlink_at(display_screen(),row,col,&hovered_link);
                    if(was_hovered!=link_hovered || (link_hovered &&
                       (previous.token!=hovered_link.token || previous.first!=hovered_link.first ||
                        previous.last!=hovered_link.last || strcmp(previous.url,hovered_link.url)))) dirty=1;
                }
                if(link_gesture.owned) {
                    if(event==IDCMP_MOUSEMOVE) link_gesture_motion(&link_gesture,mx,my);
                    if(event==IDCMP_MOUSEBUTTONS && code==SELECTUP) {
                        int action=link_gesture_finish(&link_gesture,
                            link_hovered?hovered_link.url:NULL,mx,my,ctrl,shift);
                        if(action) link_action(action,hovered_link.url);
                        link_hovered=0; dirty=1;
                        continue;
                    }
                    if(event==IDCMP_MOUSEMOVE) continue;
                    if(event==IDCMP_MOUSEBUTTONS && code!=SELECTDOWN)
                        link_gesture_cancel(&link_gesture);
                }
                if(!process_session.line.raw || !screen.mouse_tracking) {
                    mouse_buttons=0; mouse_position_valid=0;
                }
                if(mouse_releases.count && !release) continue;
                if(event==IDCMP_MOUSEBUTTONS && code==SELECTDOWN &&
                   !mouse_buttons && !selection.dragging && link_hovered &&
                   link_gesture_start(&link_gesture,hovered_link.url,mx,my,ctrl,shift)) {
                    dirty=1; continue;
                }
                if(process_session.console && process_session.line.raw && screen.mouse_tracking &&
                   ((release && captured) || (event==IDCMP_MOUSEMOVE && mouse_buttons) || (!selection.dragging &&
                   !(mouse_qualifiers&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT))))) {
                    if(event==IDCMP_MOUSEMOVE && screen.mouse_tracking==1002 && mouse_buttons &&
                       mouse_cell(window,mx,my,&row,&col,1) &&
                       (!mouse_position_valid || row!=mouse_last_row || col!=mouse_last_col)) {
                        unsigned moving=mouse_buttons&1?32:mouse_buttons&2?33:34;
                        unsigned modifiers=(mouse_qualifiers&IEQUALIFIER_CONTROL?16:0)|
                            (mouse_qualifiers&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)?4:0)|
                            (mouse_qualifiers&(IEQUALIFIER_LALT|IEQUALIFIER_RALT)?8:0);
                        unsigned char report[64];
                        size_t n=screen_mouse_encode(&screen,moving,col,row,modifiers,0,report,sizeof report);
                        if(n && mouse_write(&process_session,report,n)==n) {
                            mouse_last_row=row; mouse_last_col=col; mouse_position_valid=1;
                        }
                    }
                    if(event==IDCMP_MOUSEBUTTONS &&
                       (!release || captured) && mouse_cell(window,mx,my,&row,&col,release)) {
                        unsigned modifiers=(mouse_qualifiers&IEQUALIFIER_CONTROL?16:0)|
                            (mouse_qualifiers&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)?4:0)|
                            (mouse_qualifiers&(IEQUALIFIER_LALT|IEQUALIFIER_RALT)?8:0);
                        unsigned char report[64];
                        size_t n;
                        if(release) {
                            mouse_release_button(button,col,row,modifiers);
                            continue;
                        }
                        n=screen_mouse_encode(&screen,button,col,row,modifiers,0,report,sizeof report);
                        if(n && mouse_write(&process_session,report,n)==n && button<3) {
                            if(release) mouse_buttons&=~(1u<<button);
                            else mouse_buttons|=1u<<button;
                            mouse_last_row=row; mouse_last_col=col; mouse_position_valid=1;
                        }
                    }
                    continue;
                }
                if(event==IDCMP_MOUSEBUTTONS && code==SELECTDOWN) {
                    if(mouse_cell(window,mx,my,&row,&col,0)) {
                        command_editor.selecting=0;
                        selection_start(&selection,display_screen(),row,col);
                        clip_status="Selected";
                    } else selection.active=selection.dragging=0;
                    dirty=1;
                } else if(selection.dragging) {
                    if(event==IDCMP_MOUSEMOVE) {
                        if(my<window->BorderTop+MARGIN) screen_scroll_view(&screen,1);
                        if(my>=(int)(window->BorderTop+MARGIN+screen.rows*xfont_height())) screen_scroll_view(&screen,-1);
                    }
                    mouse_cell(window,mx,my,&row,&col,1);
                    selection_extend(&selection,display_screen(),row,col);
                    if(event==IDCMP_MOUSEBUTTONS && code==SELECTUP) selection.dragging=0;
                    dirty=1;
                }
            } else if (event == IDCMP_NEWSIZE) {
                paint_damage_invalidate(&paint_damage);
                link_gesture_cancel(&link_gesture); link_hovered=0;
                unsigned cols = (window->GZZWidth - 2 * MARGIN) / xfont_width();
                unsigned rows = (window->GZZHeight - 2 * MARGIN) / xfont_height();
                if (cols > MAX_COLUMNS) cols = MAX_COLUMNS;
                if (rows > MAX_ROWS) rows = MAX_ROWS;
                if (!cols) cols = 1;
                if (!rows) rows = 1;
                screen_resize(&screen, rows, cols);
                if(pty_mode) {
                    process_session.columns=cols;process_session.rows=rows;
                    aros_session_poll(&process_session);
                }
                resize_queue();
                selection.active=selection.dragging=0;
                dirty=1;
            } else if (event == IDCMP_RAWKEY && have_key) {
                link_gesture_cancel(&link_gesture); link_hovered=0;
                unsigned char bytes[AROS_INPUT_MAX_BYTES];
                UWORD q=key.event.ie_Qualifier;
                int action=clipboard_shortcut(&key);
                size_t count=aros_input_encode(&key,bytes,sizeof bytes);
                if(action) { clipboard_action(action); dirty=1; continue; }
                if((code==RAWKEY_PAGEUP || code==RAWKEY_PAGEDOWN) &&
                   (q&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT))) {
                    screen_scroll_view(&screen,code==RAWKEY_PAGEUP?(int)screen.rows-1:1-(int)screen.rows);
                    dirty=1; continue;
                }
                if(code==RAWKEY_NM_WHEEL_UP || code==RAWKEY_NM_WHEEL_DOWN) {
                    if(mouse_releases.count) continue;
                    /* Mode 2100 (hardcode L5): wheel steps go to the program,
                       buttons stay with the terminal, so a plain drag still selects. */
                    if(process_session.console && process_session.line.raw &&
                       (screen.mouse_tracking || screen.wheel_report) &&
                       !(q&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT))) {
                        unsigned row,col; unsigned char report[64];
                        if(mouse_cell(window,mx,my,&row,&col,0)) {
                            unsigned modifiers=(q&IEQUALIFIER_CONTROL?16:0)|
                                (q&(IEQUALIFIER_LALT|IEQUALIFIER_RALT)?8:0);
                            size_t n=screen.mouse_tracking ?
                                screen_mouse_encode(&screen,
                                    code==RAWKEY_NM_WHEEL_UP?64:65,col,row,modifiers,0,report,sizeof report) :
                                screen_wheel_encode(&screen,code==RAWKEY_NM_WHEEL_UP,col,row,modifiers,report,sizeof report);
                            if(n) terminal_reply(&process_session,report,n);
                        }
                        continue;
                    }
                    screen_scroll_view(&screen,code==RAWKEY_NM_WHEEL_UP?3:-3);
                    dirty=1; continue;
                }
                if(code==RAWKEY_ESCAPE && (paste.length || (clip_job.task_id && !clip_job.writing))) {
                    /* The same client keeps reading: a bracketed paste still
                       gets its end marker. */
                    input_order_cancel_paste(&input_order); key_queue_clear(&keyboard); ++session_generation;
                    order_session_generation=session_generation;
                    clip_status="Paste cancelled"; dirty=1; continue;
                }
#ifdef XTERM_PROCESS_PORT_CLIENTS
                if(code==RAWKEY_F5 && echo_mode==2 && completion_job.task_id) {
                    clip_status="TAB search running; press F5 again when it ends"; dirty=1; continue;
                }
#endif
                if(code==RAWKEY_F5 && echo_mode==2) {
                    paste_cancel(&paste); key_queue_clear(&keyboard); ++session_generation;
                    if(shell_session && !process_session.line.raw) {
                        screen.cluster_active=0;
                        screen_write(&screen,(const unsigned char *)command_editor.text,command_editor.length);
                        line_editor_action(&command_editor,XLINE_CLEAR);
                    }
                    aros_session_eof(&process_session);
                    clip_status=pty_mode?"PTY hangup requested":"Input EOF requested"; dirty=1; continue;
                }
                if((code==RAWKEY_F1 || code==RAWKEY_F2 || code==RAWKEY_F4) &&
                   (handler_mode || aros_session_active(&process_session))) {
                    clip_status=handler_mode?"DOS console | F5 EOF | demo switching disabled":
                        "Process running: exit or F5 EOF before switching";
                    dirty=1; continue;
                }
                if(paste.length && code!=RAWKEY_F1 && code!=RAWKEY_F2 && code!=RAWKEY_F4) {
                    if(paste.confirmation && code==RAWKEY_F3) {
                        paste_confirm(&paste); clip_status="Pasting... (Esc cancels)";
                        dirty=1; continue;
                    }
                }
                if(echo_mode && code!=RAWKEY_F1 && code!=RAWKEY_F2 && code!=RAWKEY_F4) {
                    if(count || code==RAWKEY_TAB || code==RAWKEY_LEFT || code==RAWKEY_RIGHT ||
                       code==RAWKEY_UP || code==RAWKEY_DOWN || code==RAWKEY_HOME ||
                       code==RAWKEY_END || code==RAWKEY_BACKSPACE || code==RAWKEY_DELETE)
                        keyboard_submit(code,q,bytes,count);
                    dirty=1; continue;
                }
                /* Modifier-only RAWKEY events (notably Shift before TAB)
                 * produce no bytes and must not discard the cycling set. */
                if(count) completion_active=0;
                if(!count && code!=RAWKEY_F1 && code!=RAWKEY_F2 && code!=RAWKEY_F4) continue;
                last_key = code;
                if(code==RAWKEY_F4) {
                    int start_failed;
                    key_queue_clear(&keyboard);
                    paste_cancel(&paste); ++session_generation;
                    start_failed=aros_session_dispose(&process_session);
                    if(!start_failed) {
                        process_session.columns=screen.columns;process_session.rows=screen.rows;
                        start_failed=shell_session?aros_session_start_shell_profile(&process_session,shell_profile):
                            aros_session_start(&process_session,process_command);
                    }
                    if(start_failed) {
                        clip_status="Cannot start session/profile | F4 retries"; dirty=1; continue;
                    }
                    output_reset();
                    program_title_clear(&program_title); memset(&foreground,0,sizeof foreground);
                    echo_session_close(&echo); echo_mode=2; process_reported=0;
                    memset(&command_editor,0,sizeof command_editor);
                    selection.active=selection.dragging=0;
                    screen_init(&screen,cells,MAX_ROWS*MAX_COLUMNS,screen.rows,screen.columns);
                    screen.amiga_pens=!pty_mode;
                    reset_links();
                    screen_attach_history(&screen,history,HISTORY_LINES*MAX_COLUMNS);
                    screen_attach_reflow(&screen,history_spare,HISTORY_LINES*MAX_COLUMNS);
                    screen_attach_alternate(&screen,alternate,MAX_ROWS*MAX_COLUMNS);
                    clip_status=NULL;
                } else if (code==RAWKEY_F1) {
                    output_reset();
                    key_queue_clear(&keyboard);
                    aros_session_dispose(&process_session);
                    paste_cancel(&paste); ++session_generation;
                    program_title_clear(&program_title); memset(&foreground,0,sizeof foreground);
                    echo_session_close(&echo); echo_mode=0; replay();
                } else if(code==RAWKEY_F2) {
                    output_reset();
                    key_queue_clear(&keyboard);
                    aros_session_dispose(&process_session);
                    paste_cancel(&paste); ++session_generation;
                    selection.active=selection.dragging=0;
                    program_title_clear(&program_title); memset(&foreground,0,sizeof foreground);
                    echo_mode=1; echo_session_init(&echo);
                    screen_init(&screen,cells,MAX_ROWS*MAX_COLUMNS,screen.rows,screen.columns);
                    reset_links();
                    screen_attach_history(&screen,history,HISTORY_LINES*MAX_COLUMNS);
                    screen_attach_reflow(&screen,history_spare,HISTORY_LINES*MAX_COLUMNS);
                    screen_attach_alternate(&screen,alternate,MAX_ROWS*MAX_COLUMNS);
                    write_text("\033[1;36mLimpet / byte-stream echo\033[0m\r\n");
                    write_text("Keyboard -> UTF-8/VT -> EchoSession -> parser\r\n");
                    write_text("Drag selects | RAmiga+C/V or Ctrl+Shift+C/V\r\n");
                    write_text("Not a Shell. Backspace edits one display cell.\r\n\r\n");
                    write_text("echo> ");
                } else if(!echo_mode && count==1) {
                    if(bytes[0]=='u') screen_scroll_view(&screen,5);
                    else if(bytes[0]=='d') screen_scroll_view(&screen,-5);
                    else if(bytes[0]=='r') replay();
                }
                dirty=1;
            }
        }
        if(!closing && keyboard_ready()) {
            if(key_queue_drain(&keyboard,16,keyboard_deliver,NULL)) dirty=1;
        }
        /* Another reader, another session or no raw client: a bracketed
           paste and a pending resize report belong to the old owner. */
        if(order_session_generation!=session_generation ||
           order_input_generation!=process_session.input_generation || !replies_enabled()) {
            input_order_drop(&input_order);
            order_session_generation=session_generation;
            order_input_generation=process_session.input_generation;
        }
        /* Native console: back in cooked mode, the program that turned on
           input reports has finished with them. The Shell and the next raw
           program must not inherit them (a PTY session is reset when it ends). */
        if(!pty_mode && raw_seen && !process_session.line.raw) screen_reset_input_modes(&screen);
        raw_seen=process_session.line.raw;
        /* A client that just set 2048 gets the current size at once. */
        if(screen.resize_report && !resize_mode_seen) resize_queue();
        resize_mode_seen=screen.resize_report;
        if(!screen.resize_report && !input_order.resize.offset)
            memset(&input_order.resize,0,sizeof input_order.resize);
        if(!closing && replies_enabled()) {
            int framed=input_order_paste_busy(&input_order);
            if(input_order_pump(&input_order,!reply_flow.length,order_sink,&process_session)) {
                dirty=1;
                if(framed && !input_order_paste_busy(&input_order)) clip_status="Pasted";
                link_gesture_cancel(&link_gesture); link_hovered=0;
            }
        }
        if(!closing && !reply_flow.length && paste.length && !paste.confirmation &&
           !input_order_paste_busy(&input_order) && input_order_plain_paste_may_send(&input_order) &&
           (echo_mode!=2 || !shell_session || process_session.line.raw || editor_ready())) {
            const unsigned char *bytes;
            size_t count=paste_peek(&paste,&bytes,1024);
            size_t accepted=session_bytes(bytes,count);
            link_gesture_cancel(&link_gesture); link_hovered=0;
            paste_consume(&paste,accepted);
            if(!accepted && editor_ready()) {
                paste_cancel(&paste);
                clip_status="Paste stopped: command too long or unsupported input";
            }
            else if(!paste.length) clip_status="Pasted";
            dirty=1;
        }
        if(flood_diag_step(window)) {
            closing=TRUE;aros_session_stop(&process_session);
        }
        if(closing && !clip_job.task_id && !link_job.task_id &&
#ifdef XTERM_PROCESS_PORT_CLIENTS
           !completion_job.task_id && !stop_after_completion &&
#endif
           !aros_session_active(&process_session) &&
           !aros_session_dispose(&process_session))
            running=FALSE;
        if(running && dirty) { update_title(window,last_key); draw_screen(window); }
        if(running && (title_pending || title_hold || status_expiring() || foreground_watch ||
                       (echo_mode==2 && foreground.running))) tick_arm();
    }
    tick_close();
    aros_session_dispose(&process_session);
#ifdef XTERM_PKTWAIT_PROBE
    aros_handler_probe_stop();
#endif
    echo_session_close(&echo); close_main_window(window); xfont_close();
    return RETURN_OK;
}
