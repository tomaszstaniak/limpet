#include "eza_icon_contract.h"
#include "screen_model.h"
#include "theme.h"
#include "margin_fill.h"
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
#include "aros_drop.h"
#include "output_search.h"
#include "settings.h"
#include "console_name.h"
#include "terminal_input.h"
#include <dos/var.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <proto/gadtools.h>
#include <libraries/gadtools.h>
#include <cybergraphx/cybergraphics.h>
#include "perf_diag.h"
#include "redraw_settle.h"
#include <devices/rawkeycodes.h>
#include <devices/timer.h>
#include <proto/timer.h>
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
#define MARGIN 12

static XTermScreen screen;
/* Scrollback search (src/output_search.h). While it is open it holds the
   keys and the view; typeahead and pastes wait until it closes. */
static XOutputSearch output_search;
static char search_label[XSCREEN_TITLE_MAX];
static const char *search_note;
static unsigned char search_marks[MAX_ROWS*MAX_COLUMNS];
/* XTERM:'s window as the opening program named it, CON: syntax
   (src/console_name.h); all fields unset elsewhere. */
static XConsoleName console_spec={-1,-1,-1,-1,"","",0,0,-1};
static int wait_reported;
static XTermCell cells[MAX_ROWS * MAX_COLUMNS];
static XTermCell alternate[MAX_ROWS * MAX_COLUMNS];
/* Scrollback, sized by the settings at start (src/settings.h): lines at the
   widest window, so history_cells is lines * MAX_COLUMNS. Reflow writes the
   rewrapped history into the spare and swaps the two. */
static XTermCell *history, *history_spare;
static size_t history_cells;
static char (*command_ring)[XLINE_CAPACITY];
static size_t command_ring_size;
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
static ArosDrop file_drop;
static ArosDropped dropped;
static char drop_status[96];
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
    output_search.open=0;
    screen_init(&screen, cells, MAX_ROWS * MAX_COLUMNS,
                screen.rows, screen.columns);
    reset_links();
    screen_attach_history(&screen, history, history_cells);
    screen_attach_reflow(&screen, history_spare, history_cells);
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
        (!process_session.external || (process_session.handles&7));
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
/* Bytes of p up to and including the first end of a synchronized frame
 * (CSI ? 2026 l), or n. Parsing stops there so the finished frame is drawn
 * before the next one starts to arrive in the same read: once any of the
 * next frame is parsed, the finished one is gone from the model. A marker
 * split across two reads is not found; the frame is then drawn with the
 * next one, as without the marks. */
static size_t sync_frame_end(const unsigned char *p,size_t n)
{
    static const unsigned char end[]="\033[?2026l";
    size_t i;
    if(n<sizeof end-1) return n;
    for(i=0;i+sizeof end-1<=n;++i)
        if(p[i]==0x1b && !memcmp(p+i,end,sizeof end-1)) return i+sizeof end-1;
    return n;
}
static size_t output_pump(void)
{
    size_t n,take;
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
    PERF_MAX(output_backlog_max,process_session.output.length+(output_length-output_offset));
#ifdef XTERM_PERF_DIAG
    uint64_t parse_started=PERF_NOW();
#endif
    take=sync_frame_end(pending_output+output_offset,output_length-output_offset);
    if(replies_enabled() && output_replies && output_generation==process_session.input_generation)
        n=reply_flow_write(&reply_flow,&screen,pending_output+output_offset,
                          take,reply_send,&process_session);
    else {
        n=take;
        screen_write(&screen,pending_output+output_offset,n);
    }
    flood_diag_output(pending_output+output_offset,n);
    PERF_DO(if(n) { ++perf_diag.pumps; perf_diag.pump_bytes+=n;
                    perf_diag.parse_us+=(unsigned long)(PERF_NOW()-parse_started); });
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

#ifdef XTERM_PERF_DIAG
static RedrawSettle settle;
static int sync_open;
/* Why the next draw happens: 1 settle quiet, 2 settle limit, 0 other. */
static int perf_draw_reason;
/* Line numbers in the first 8 columns (Helix's gutter) must rise from row
 * to row; a repeat or a step back means the model holds part of an old and
 * part of a new frame. Rows without a number (wrapped lines, status) are
 * skipped. Returns -1 when fewer than 4 rows have a number (not Helix). */
static int perf_model_torn(const XTermScreen *s)
{
    unsigned row,col,numbered=0; long prev=-1; int torn=0;
    /* The last two rows are Helix's status and command lines (":28"). */
    for(row=0;row+2<s->rows;++row) {
        long n=-1;
        for(col=0;col<8 && col<s->columns;++col) {
            uint32_t ch=screen_view_cell(s,row,col).ch;
            if(ch>='0' && ch<='9') n=(n<0?0:n*10)+(long)(ch-'0');
            else if(n>=0) break;
        }
        if(n<0) continue;
        ++numbered;
        if(prev>=0 && n<=prev) torn=1;
        prev=n;
    }
    if(torn && numbered>=4) {
        /* r<reason> o<frame open> k<key> then the numbers, for the log. */
        char *out=perf_diag.torn_sample; size_t room=sizeof perf_diag.torn_sample,used;
        snprintf(out,room,"r%d,o%d,h%d:",perf_draw_reason,sync_open,settle.held);
        for(row=0;row<s->rows;++row) {
            long n=-1;
            for(col=0;col<8 && col<s->columns;++col) {
                uint32_t ch=screen_view_cell(s,row,col).ch;
                if(ch>='0' && ch<='9') n=(n<0?0:n*10)+(long)(ch-'0');
                else if(n>=0) break;
            }
            used=strlen(out);
            if(used+8>=room) break;
            if(n>=0) snprintf(out+used,room-used,"%ld,",n); else snprintf(out+used,room-used,"_,");
        }
    }
    return numbered<4?-1:torn;
}
#endif

static unsigned char *frame_pixels;
static size_t frame_size;
/* Rows [first,end) of frame_pixels, already committed to paint_damage. */
static void upload_run(struct Window *window,struct RastPort *rp,const XTermScreen *screen,
                       unsigned w,unsigned h,unsigned pitch,unsigned first,unsigned end,
                       const unsigned *images,uint64_t *upload_us)
{
    unsigned row,i,width=screen->columns*w,height=(end-first)*h;
    ULONG uploaded;
    (void)window;
#ifdef XTERM_PERF_DIAG
    uint64_t started=PERF_NOW();
#endif
    uploaded=WritePixelArray(frame_pixels+(size_t)first*h*pitch,0,0,pitch,rp,MARGIN,MARGIN+first*h,
                             width,height,RECTFMT_RGB);
    PERF_DO(*upload_us+=PERF_NOW()-started; perf_diag.rows+=end-first;
            perf_diag.pixels+=(unsigned long)width*height);
    (void)upload_us;
    if(uploaded!=width*height) {
        paint_damage_invalidate(&paint_damage);
        for(row=first;row<end;++row) flood_diag_upload(window,row,0);
        return;
    }
    for(row=first;row<end;++row) {
        flood_diag_upload(window,row,1);
        for(i=0;i<XKITTY_MAX_IMAGES;++i) if(images[row]&(1u<<i)) kitty_mark_rows(i,row,row+1);
    }
}

static void draw_screen(struct Window *window)
{
    const XTermScreen *display=display_screen();
    /* Marks come from the terminal's text, not the command line drawn over it. */
    const int finding=output_search.open;
    if(finding) output_search_marks(&output_search,&screen,search_marks,sizeof search_marks);
    XTermScreen screen=*display;
    struct RastPort *rp = window->RPort;
    unsigned row, col;
    unsigned w = xfont_width(), h = xfont_height(), pitch;
    int run_first=-1;
    static unsigned run_images[MAX_ROWS];
    unsigned cursor_col=screen.cursor_column;
    int cursor=(!screen.view_offset || display==&command_view) && screen.cursor_visible;
    /* `screen` here is the local copy; the live view is anything but the
       command editor's view, and only while not scrolled into history. */
    int images_live=display!=&command_view && !screen.view_offset;
    if(!paint_damage_geometry(&paint_damage,screen.rows,screen.columns)) return;
#ifdef XTERM_PERF_DIAG
    { int t=perf_model_torn(&screen);
      if(t>=0) { ++perf_diag.model_checked;
                 if(t) { if(perf_draw_reason==1) ++perf_diag.model_torn_quiet;
                         else if(perf_draw_reason==2) ++perf_diag.model_torn_limit;
                         else ++perf_diag.model_torn_other; } }
      perf_draw_reason=0; }
#endif
    kitty_damage(&screen,images_live);
    if(cursor && cursor_col && !screen_view_cell(&screen,screen.cursor_row,cursor_col).width)
        --cursor_col;
    ++paint_calls;
    uint64_t upload_us=0;
#ifdef XTERM_PERF_DIAG
    uint64_t draw_started=PERF_NOW(); unsigned long rows_before=perf_diag.rows;
#endif
    /* Never clear the text grid before uploading finished rows: that exposes
     * a blank frame on every keystroke. Only fill the unused margins, in the
     * edge cells' colours on the alternate screen (margin_fill.h). */
    {
        static MarginRect margins[MARGIN_RECTS_MAX(MAX_ROWS,MAX_COLUMNS)];
        MarginGeometry geometry={w,h,MARGIN,(unsigned)window->GZZWidth,(unsigned)window->GZZHeight};
        size_t i,n=margin_rects(&screen,&theme,&geometry,margins,sizeof margins/sizeof *margins);
        for(i=0;i<n;++i) {
            colours(rp,margins[i].rgb,margins[i].rgb);
            RectFill(rp,margins[i].x0,margins[i].y0,margins[i].x1,margins[i].y1);
        }
    }
    SetDrMd(rp, JAM2);
    pitch=screen.columns*w*3;
    /* Changed rows are rasterised into one buffer and uploaded as runs of
     * adjacent rows, one WritePixelArray each. Uploading every row as soon
     * as it was rasterised showed a scrolled screen half old and half new
     * for most of a redraw (a line doubled where the two halves met, read
     * as glyphs growing at the bottom). The run still lands row by row in
     * the window, but within the upload, not across the rasterising.
     * Without the buffer (no memory) rows go up one at a time as before. */
    if((size_t)screen.rows*h*pitch>frame_size) {
        if(frame_pixels) FreeVec(frame_pixels);
        frame_size=(size_t)screen.rows*h*pitch;
        if(!(frame_pixels=AllocVec(frame_size,MEMF_ANY))) frame_size=0;
    }
    for (row = 0; row < screen.rows; ++row) {
        unsigned y,x, images;
        unsigned char *pixels=frame_pixels?frame_pixels+(size_t)row*h*pitch:row_pixels;
        for (col = 0; col < screen.columns; ++col) {
            XTermCell cell = screen_view_cell(&screen, row, col);
            int found=finding?search_marks[row*screen.columns+col]:0;
            int selected=found==2 || selection_contains(&selection,&screen,row,col);
            paint_row[col].cell=cell;
            paint_row[col].fg=selected?theme.selection_fg:theme_fg(&theme,&cell);
            paint_row[col].bg=selected?theme.selection_bg:theme_bg(&theme,&cell);
            paint_row[col].cursor_colour=theme.cursor;
            paint_row[col].flags=(cursor && row==screen.cursor_row && col==cursor_col?XPAINT_CURSOR:0)|
                ((cell.colour_flags&XCOLOUR_UNDERLINE || found==1 ||
                  (link_hovered && hyperlink_contains(&screen,&hovered_link,row,col)))
                 ?XPAINT_UNDERLINE:0);
        }
        if(!paint_damage_changed(&paint_damage,row,paint_row)) {
            if(run_first>=0) upload_run(window,rp,&screen,w,h,pitch,(unsigned)run_first,row,
                                        run_images,&upload_us);
            run_first=-1;
            continue;
        }
        for (col = 0; col < screen.columns; ++col) {
            uint32_t bg=paint_row[col].bg;
            for(y=0;y<h;++y) for(x=0;x<w;++x) {
                unsigned at=y*pitch+(col*w+x)*3;
                pixels[at]=bg>>16; pixels[at+1]=bg>>8; pixels[at+2]=bg;
            }
        }
#ifdef XTERM_PERF_DIAG
        uint64_t glyphs_started=PERF_NOW();
#endif
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
                    pixels[at]=fg>>16; pixels[at+1]=fg>>8; pixels[at+2]=fg;
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
                memcpy(pixels+y*pitch+col*w*3,rgb+y*r->width*3,r->width*3);
        }
        PERF_DO(perf_diag.glyph_us+=(unsigned long)(PERF_NOW()-glyphs_started));
        if(h>1) for(col=0;col<screen.columns;++col)
            if(paint_row[col].flags&XPAINT_UNDERLINE) {
                uint32_t fg=paint_row[col].fg;
                for(x=0;x<w;++x) {
                    unsigned at=(h-2)*pitch+(col*w+x)*3;
                    pixels[at]=fg>>16; pixels[at+1]=fg>>8; pixels[at+2]=fg;
                }
            }
        for(col=0;col<screen.columns;++col) if(paint_row[col].flags&XPAINT_CURSOR) {
            unsigned width=paint_row[col].cell.width*w;
            if(width>(screen.columns-col)*w) width=(screen.columns-col)*w;
            for(x=0;x<width;++x) {
                unsigned at=(h-1)*pitch+(col*w+x)*3;
                pixels[at]=theme.cursor>>16;
                pixels[at+1]=theme.cursor>>8; pixels[at+2]=theme.cursor;
            }
        }
        images=kitty_composite(&screen,images_live,row,pixels,pitch);
        ++paint_rows;
        if(frame_pixels) {
            /* Committed now; a failed upload of the run invalidates all. */
            paint_damage_commit(&paint_damage,row,paint_row);
            run_images[row]=images;
            if(run_first<0) run_first=(int)row;
            continue;
        }
        /* A clipped/partial upload may already have changed visible pixels. */
        paint_damage_forget_row(&paint_damage,row);
#ifdef XTERM_PERF_DIAG
        uint64_t upload_started=PERF_NOW();
#endif
        ULONG uploaded=WritePixelArray(pixels,0,0,pitch,rp,MARGIN,MARGIN+row*h,
                          screen.columns*w,h,RECTFMT_RGB);
        PERF_DO(upload_us+=PERF_NOW()-upload_started; ++perf_diag.rows;
                perf_diag.pixels+=(unsigned long)screen.columns*w*h);
        if(uploaded==screen.columns*w*h) {
            unsigned i;
            paint_damage_commit(&paint_damage,row,paint_row);
            flood_diag_upload(window,row,1);
            for(i=0;i<XKITTY_MAX_IMAGES;++i) if(images&(1u<<i)) kitty_mark_rows(i,row,row+1);
        } else flood_diag_upload(window,row,0);
    }
    if(run_first>=0) upload_run(window,rp,&screen,w,h,pitch,(unsigned)run_first,screen.rows,
                                run_images,&upload_us);
    kitty_blit_fast(rp,&screen);
    kitty_count_drawn(&screen);
    PERF_DO(perf_draw_delay());
    PERF_DO(uint64_t spent=PERF_NOW()-draw_started; ++perf_diag.draws;
            if(perf_diag.rows!=rows_before) ++perf_diag.draws_with_rows;
            perf_diag.upload_us+=(unsigned long)upload_us;
            perf_diag.raster_us+=(unsigned long)(spent-upload_us);
            PERF_MAX(draw_max_us,spent));
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

enum { MENU_ABOUT=1, MENU_CLOSE, MENU_COPY, MENU_PASTE, MENU_FIND, MENU_RELOAD };
static struct NewMenu menu_spec[]={
    {NM_TITLE,(UBYTE *)"Project",NULL,0,0,NULL},
    {NM_ITEM,(UBYTE *)"About Limpet...",NULL,0,0,(APTR)MENU_ABOUT},
#ifndef XTERM_DOS_HANDLER
    /* Not in the XTERM: worker, which may make no DOS call once it serves
       its port; it takes the settings when its window opens. */
    {NM_ITEM,(UBYTE *)"Reload settings",NULL,0,0,(APTR)MENU_RELOAD},
#endif
    {NM_ITEM,NM_BARLABEL,NULL,0,0,NULL},
    {NM_ITEM,(UBYTE *)"Close window",(UBYTE *)"Q",0,0,(APTR)MENU_CLOSE},
    {NM_TITLE,(UBYTE *)"Edit",NULL,0,0,NULL},
    /* the same RAmiga+C/V that worked before the menu existed */
    {NM_ITEM,(UBYTE *)"Copy",(UBYTE *)"C",0,0,(APTR)MENU_COPY},
    {NM_ITEM,(UBYTE *)"Paste",(UBYTE *)"V",0,0,(APTR)MENU_PASTE},
    {NM_ITEM,NM_BARLABEL,NULL,0,0,NULL},
    {NM_ITEM,(UBYTE *)"Find in output...",(UBYTE *)"F",0,0,(APTR)MENU_FIND},
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
    "  RAmiga+F / Ctrl+Shift+F finds text in it",
    "  (Up/Down: older/newer, Esc: back).",
    "  Up/Down: command history; Ctrl-R searches it.",
    "  TAB / Shift+TAB: complete a path; icons",
    "  dropped from Workbench give theirs.",
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
        /* Wanderer waits for a drop's reply: answer it here too. */
        Wait((1UL<<w->UserPort->mp_SigBit)|aros_drop_signal(&file_drop));
        if(aros_drop_refuse(&file_drop)) clip_status="Drop: close the About window first";
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
    aros_drop_close(&file_drop);
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
    if(console_spec.title[0]) in.terminal=console_spec.title;
    in.link=link_hovered?hovered_link.url:NULL;
    in.program=program_title_text(&program_title);
    in.command=foreground.running?foreground.name:NULL;
    in.shell=shell_session && echo_mode==2 && !process_session.exited;
    in.status=clip_status;
    if(output_search.open) {
        size_t n=output_search_label(&output_search,search_label,sizeof search_label);
        if(search_note && n+strlen(search_note)+3<sizeof search_label)
            snprintf(search_label+n,sizeof search_label-n," (%s)",search_note);
        in.status=search_label;
    }
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
            /* Ctrl-\: end of input once, as in CON:. What was typed goes
               to the reader without a line end; the next Read returns 0.
               F5 still ends the session's input for good. */
            if(count==1 && bytes[0]==0x1c) {
                line_editor_search_cancel(&command_editor);
                editor_sync();
                if(!aros_session_end_input(&process_session)) return 0;
                screen.cluster_active=0; screen.cluster_state=0;
                screen_write(&screen,(const unsigned char *)command_editor.text,command_editor.length);
                line_editor_action(&command_editor,XLINE_CLEAR);
                selection.active=selection.dragging=0;
                return 1;
            }
            /* Ctrl-D/E/F: a signal to the reader, as in CON:; the line
               stays as it is. */
            if(count==1 && bytes[0]>=4 && bytes[0]<=6) {
                if(!aros_session_break_signal(&process_session,SIGBREAKF_CTRL_D<<(bytes[0]-4)))
                    clip_status="No program reads this window yet; the key was not sent";
                return 1;
            }
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

/* Why the command line cannot take a drop now, or NULL. Only the native
   editor of a cooked Shell session takes one: a raw-mode program, a PTY
   session or a running command never receives paths it did not ask for. */
static const char *drop_refusal(int closing)
{
    if(closing) return "Drop: the window is closing";
    if(!shell_session) return "Drop: only a Shell's command line takes files";
    if(echo_mode!=2 || !aros_session_active(&process_session) || process_session.eof)
        return "Drop: the Shell has ended";
    if(process_session.line.raw) return "Drop: the program here reads keys itself";
    if(keyboard.count || paste.length || clip_job.task_id) return "Drop: typing or a paste is still going in";
    if(!editor_ready()) return "Drop: a command is running; drop at the prompt";
    return NULL;
}

/* Paths dropped from Workbench go in at the cursor; nothing runs. Every
   drop is replied before this looks at the line (aros_drop_take). */
static int drop_files(int closing)
{
    int any=0;
    while(aros_drop_take(&file_drop,&dropped)) {
        const char *why=drop_refusal(closing);
        size_t failed;
        any=1;
        if(why) { clip_status=why; continue; }
        switch(dropped.error) {
        case ARDROP_GONE:
            snprintf(drop_status,sizeof drop_status,"Drop: %.48s is not there",dropped.name);
            clip_status=drop_status; continue;
        case ARDROP_TOO_LONG: clip_status="Drop: a path is too long for the command line"; continue;
        case ARDROP_TOO_MANY: clip_status="Drop: more than 64 objects"; continue;
        case ARDROP_OK: break;
        }
        switch(file_drop_insert(&command_editor,dropped.paths,dropped.count,&failed)) {
        case XDROP_OK:
            editor_sync();
            snprintf(drop_status,sizeof drop_status,"Dropped %u path%s; Enter runs the line",
                     (unsigned)dropped.count,dropped.count==1?"":"s");
            clip_status=drop_status; break;
        case XDROP_BAD_NAME:
            snprintf(drop_status,sizeof drop_status,"Drop: %.40s is not UTF-8 text or has a control",
                     failed<dropped.count?dropped.paths[failed]:"a name");
            clip_status=drop_status; break;
        case XDROP_TOO_LONG: clip_status="Drop: the line would be over 1023 bytes"; break;
        case XDROP_TOO_MANY: clip_status="Drop: more than 64 objects"; break;
        case XDROP_SEARCHING: clip_status="Drop: end the history search first (Enter or Esc)"; break;
        case XDROP_SHELL_CUT: clip_status="Drop: the Shell would cut this line at a ; (a name has a \")"; break;
        }
    }
    return any;
}

static int keyboard_interrupt(const XQueuedKey *key)
{
    return echo_mode==2 && shell_session && !process_session.line.raw &&
        key->length==1 && key->bytes[0]==3;
}

/* Ctrl-D/E/F in a cooked Shell session only signal, so they need no Read
   waiting and do not queue behind a running command. Ctrl-\ is input (the
   end of it) and keeps its place behind typeahead and pastes like any key. */
static int keyboard_console_key(const XQueuedKey *key)
{
    return echo_mode==2 && shell_session && !process_session.line.raw &&
        key->length==1 && key->bytes[0]>=4 && key->bytes[0]<=6;
}

static int keyboard_ready(void)
{
    const XQueuedKey *key;
    if(!keyboard.count) return 0;
    key=&keyboard.events[keyboard.head];
    if(!echo_mode || (echo_mode==2 &&
       (!aros_session_active(&process_session) || process_session.eof))) return 1;
    if(keyboard_console_key(key)) return 1;
#ifdef XTERM_PROCESS_PORT_CLIENTS
    /* The TAB child reads the Shell's dirs while the Shell waits in its
       unanswered Read: no interrupt may answer that Read until it is gone. */
    if(keyboard_interrupt(key) && completion_job.task_id) return 0;
#endif
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
    return bytes[0]=='c'?1:bytes[0]=='v'?2:bytes[0]=='f'?3:0;
}

/* RAmiga+F (the menu) or Ctrl+Shift+F opens the search; while open, the
   same key goes to the next older result, as Ctrl-R does in its list. */
static void find_start(void)
{
    search_note=NULL;
    if(output_search.open) {
        if(!output_search_step(&output_search,&screen,1)) search_note="oldest";
        return;
    }
    switch(output_search_open(&output_search,&screen)) {
    case XSEARCH_OPENED: selection.dragging=0; break;
    case XSEARCH_ALTERNATE:
        clip_status="Find: a full-screen program has the screen; use its own search"; break;
    case XSEARCH_NO_HISTORY: clip_status="Find: no scrollback in this window"; break;
    }
}

/* Keys while the search is open edit it, and none reaches the program.
   Shift+PgUp/PgDn and the wheel still scroll; Ctrl-C closes the search
   and then interrupts as usual. Returns 1 when the key was taken. */
static int find_key(UWORD code,UWORD q,const unsigned char *bytes,size_t count)
{
    if(!output_search.open) return 0;
    if(code==RAWKEY_NM_WHEEL_UP || code==RAWKEY_NM_WHEEL_DOWN) return 0;
    if((code==RAWKEY_PAGEUP || code==RAWKEY_PAGEDOWN) && (q&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)))
        return 0;
    if(code&IECODE_UP_PREFIX) return 1;
    search_note=NULL;
    switch(code) {
    case RAWKEY_ESCAPE: output_search_close(&output_search,&screen,1); return 1;
    case RAWKEY_RETURN: case RAWKEY_KP_ENTER: output_search_close(&output_search,&screen,0); return 1;
    case RAWKEY_UP: if(!output_search_step(&output_search,&screen,1)) search_note="oldest"; return 1;
    case RAWKEY_DOWN: if(!output_search_step(&output_search,&screen,0)) search_note="newest"; return 1;
    case RAWKEY_BACKSPACE: output_search_backspace(&output_search,&screen); return 1;
    default: break;
    }
    if(count==1 && bytes[0]==3) { output_search_close(&output_search,&screen,1); return 0; }
    if(count && bytes[0]>=0x20 && bytes[0]!=0x7f && bytes[0]!=0x9b &&
       !(q&(IEQUALIFIER_CONTROL|IEQUALIFIER_RCOMMAND)) &&
       !output_search_insert(&output_search,&screen,(const char *)bytes,count))
        search_note="query full or not text";
    return 1;
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
    if(output_search.open) {
        /* The first line goes into the query, nothing to the program. */
        size_t n=0;
        while(n<length && clip_text[n]!='\r' && clip_text[n]!='\n') ++n;
        search_note=n && output_search_insert(&output_search,&screen,(const char *)clip_text,n)?
            NULL:"paste not taken";
        clip_status=NULL;   /* "Reading clipboard..." would otherwise stay */
        return;
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

/* Program output is drawn once it pauses, not after every write. A
 * full-screen program sends one frame as many writes (Helix: about nine
 * 1 KB writes per scrolled screen), and drawing after each one painted the
 * frame in parts and made the program wait for every partial paint. The
 * schedule is in redraw_settle.c: draw when output paused for a moment, and
 * at the latest a fixed time after the first change not yet shown. Keys,
 * resizes and anything else that sets `dirty` draw at once. Times come from
 * the E-clock, which is monotonic; the timer only wakes the loop, and a late
 * wake-up draws at once. The request shares the tick port: no extra signal. */
struct Device *TimerBase;
static RedrawSettle settle;
static struct timerequest *settle_io;
static int settle_armed;
static void settle_open(void)
{
    redraw_settle_init(&settle,0,REDRAW_SETTLE_QUIET_US,REDRAW_SETTLE_LIMIT_US,
                       REDRAW_SETTLE_STRETCH_MAX_US);
    if(!tick_ready) return;
    if(!(settle_io=(struct timerequest *)CreateIORequest(tick_port,sizeof *settle_io))) return;
    if(OpenDevice((CONST_STRPTR)TIMERNAME,UNIT_MICROHZ,(struct IORequest *)settle_io,0)) {
        DeleteIORequest((struct IORequest *)settle_io); settle_io=NULL; return;
    }
    if(!TimerBase) TimerBase=settle_io->tr_node.io_Device;
    redraw_settle_init(&settle,1,REDRAW_SETTLE_QUIET_US,REDRAW_SETTLE_LIMIT_US,
                       REDRAW_SETTLE_STRETCH_MAX_US);
}
static uint64_t settle_now(void)
{
    struct EClockVal v; uint64_t ticks,hz=ReadEClock(&v);
    ticks=((uint64_t)v.ev_hi<<32)|v.ev_lo;
    return ticks/hz*1000000u+ticks%hz*1000000u/hz;
}
static void settle_cancel(void)
{
    if(!settle_armed) return;
    if(!CheckIO((struct IORequest *)settle_io)) AbortIO((struct IORequest *)settle_io);
    WaitIO((struct IORequest *)settle_io); settle_armed=0;
}
static void settle_arm(uint64_t micros)
{
    if(settle_armed) return;
    if(micros<1000) micros=1000;
    settle_io->tr_node.io_Command=TR_ADDREQUEST;
    settle_io->tr_time.tv_secs=micros/1000000u; settle_io->tr_time.tv_micro=micros%1000000u;
    SendIO((struct IORequest *)settle_io); settle_armed=1;
}
/* Output that would redraw: 0 when it is drawn at once (no timer). */
static int settle_hold(void)
{
    uint64_t now;
    if(!settle.timed) return 0;
    now=settle_now();
    redraw_settle_output(&settle,now);
    settle_arm(redraw_settle_wake(&settle,now));
    PERF_DO(++perf_diag.held);
    return 1;
}
/* Synchronized output (DEC 2026): a program that marks the start and end of
 * each frame is drawn once per frame, never half-written. Without the marks
 * a terminal can only guess from pauses in the output, and a program that
 * writes one frame in pieces pauses mid-frame too (Helix in this window:
 * about half of all redraws while a key was held showed lines of two
 * frames; a key, the title's timer or a pause in the output each started one). A frame left open longer than SYNC_LIMIT_US is drawn anyway, so
 * a program that never ends one cannot freeze the window. */
#define SYNC_LIMIT_US 250000u
static int sync_open;
static uint64_t sync_began;
static unsigned long sync_seen_ends;
/* 1 while drawing waits for the end of a frame. */
static int sync_holding(void)
{
    return sync_open && settle.timed && settle_now()-sync_began<SYNC_LIMIT_US;
}
/* After output: track begin and end. Returns 1 when a frame just ended
 * and held output (or a deferred key) should be drawn now. */
static int sync_track(void)
{
    int ended=screen.sync_ends!=sync_seen_ends;
    sync_seen_ends=screen.sync_ends;
    if(screen.sync_update && (!sync_open || ended) && settle.timed) { sync_open=1; sync_began=settle_now(); }
    else if(!screen.sync_update) sync_open=0;
    return ended && !sync_open;
}
/* Every pass of the loop: 1 when held output is due to be drawn now. */
static int settle_poll(void)
{
    uint64_t now;
    if(settle_armed && CheckIO((struct IORequest *)settle_io)) {
        WaitIO((struct IORequest *)settle_io); settle_armed=0;
    }
    if(!settle.held) return 0;
    now=settle_now();
    if(sync_holding()) { settle_arm(sync_began+SYNC_LIMIT_US-now); return 0; }
    if(redraw_settle_due(&settle,now)) {
        PERF_DO(uint32_t limit=redraw_settle_limit(&settle);
                if(now>=settle.first+limit) { ++perf_diag.draws_limit; perf_draw_reason=2; }
                else { ++perf_diag.draws_quiet; perf_draw_reason=1; }
                PERF_MAX(limit_max_ms,limit/1000));
        return 1;
    }
    settle_arm(redraw_settle_wake(&settle,now));
    return 0;
}
/* The window was drawn, starting at `started` (settle_now): nothing is
 * held, and no old wake-up stays queued. */
static void settle_drawn(uint64_t started)
{
    uint64_t cost;
    if(!settle.timed) return;
    cost=settle_now()-started;
    redraw_settle_drawn(&settle,cost>UINT32_MAX?UINT32_MAX:(uint32_t)cost);
    settle_cancel();
}
static void settle_close(void)
{
    if(!settle_io) return;
    settle_cancel();
    if(TimerBase==settle_io->tr_node.io_Device) TimerBase=NULL;
    CloseDevice((struct IORequest *)settle_io); DeleteIORequest((struct IORequest *)settle_io);
    settle_io=NULL; settle.timed=0;
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
    /* "Settings: " is what went wrong with Limpet.conf: it stays until
       another message takes its place (success messages do not start so). */
    static const char *const lasting[]={"Settings: ","MULTILINE","Ended (WAIT)","Pasting","Copying","Reading clipboard",
        "Closing","TAB: searching","Selected","Session ended","Cannot start session",
        "Opening link","Copying link"};
    unsigned i;
    for(i=0;i<sizeof lasting/sizeof *lasting;++i)
        if(!strncmp(s,lasting[i],strlen(lasting[i]))) return 1;
    return 0;
}
static int status_expiring(void)
{ return clip_status && !status_lasting(clip_status); }

/* Settings (src/settings.h). Font, size, scrollback and history are taken
   when a window opens; the theme also on Project > Reload settings. */
static XSettings settings;
static int theme_from_command_line;
static char settings_message[160];

/* 1 with the file's text, 0 when there is none, -1 when it is too large. */
static int settings_read(char *text,size_t *length)
{
    static const char *const places[]={"ENV:Limpet/Limpet.conf","ENVARC:Limpet/Limpet.conf"};
    struct Process *self=(struct Process *)FindTask(NULL);
    APTR window=self->pr_WindowPtr;
    unsigned i; int result=0;
    /* A missing ENVARC: is no reason for a requester. */
    self->pr_WindowPtr=(APTR)-1;
    for(i=0;i<2 && !result;++i) {
        BPTR f=Open((CONST_STRPTR)places[i],MODE_OLDFILE);
        LONG n;
        if(!f) continue;
        n=Read(f,text,XSETTINGS_FILE_MAX+1);
        Close(f);
        if(n<0) continue;
        *length=(size_t)n;
        result=n>XSETTINGS_FILE_MAX?-1:1;
    }
    self->pr_WindowPtr=window;
    return result;
}

/* Read and check the file; on any error keep `into` and say why. */
static int settings_load(XSettings *into)
{
    static char text[XSETTINGS_FILE_MAX+1];
    size_t length=0; const char *why=NULL; int line;
    switch(settings_read(text,&length)) {
    case 0: return 0;
    case -1:
        snprintf(settings_message,sizeof settings_message,
                 "Settings: Limpet.conf is over 4096 bytes; not used"); return -1;
    }
    line=settings_parse(into,text,length,&why);
    if(line) {
        snprintf(settings_message,sizeof settings_message,
                 "Settings: Limpet.conf line %d: %s; built-in settings used",line,why);
        return -1;
    }
    return 0;
}

static void settings_theme(void)
{
    if(theme_from_command_line) return;
    theme_default(&theme);
    if(settings.theme[0] && theme_load(&theme,settings.theme)) {
        theme_default(&theme);
        snprintf(settings_message,sizeof settings_message,
                 "Settings: theme %.80s cannot be read; built-in dark theme",settings.theme);
    }
}

/* Main font at the chosen size; if that fails, Vera Mono at that size, then
   Vera Mono at 13 px, which is what Limpet always had. */
static int settings_font(void)
{
    xfont_set_size(settings.font_size);
    if(!xfont_open_main(settings.font[0]?settings.font:NULL)) return 0;
    if(settings.font[0]) {
        snprintf(settings_message,sizeof settings_message,
                 "Settings: font %.80s is not a usable monospace font; Vera Mono",settings.font);
        if(!xfont_open_main(NULL)) return 0;
    }
    xfont_set_size(XSETTINGS_FONT_SIZE_DEFAULT);
    if(settings.font_size!=XSETTINGS_FONT_SIZE_DEFAULT)
        snprintf(settings_message,sizeof settings_message,
                 "Settings: font size %u does not fit a cell; 13",settings.font_size);
    return xfont_open_main(NULL);
}

static void settings_free(void)
{
    free(history); free(history_spare); free(command_ring);
    history=history_spare=NULL; command_ring=NULL;
}

/* Scrollback and command history as set; the defaults if that much memory
   is not there. */
static int settings_buffers(void)
{
    unsigned lines=settings.scrollback, commands=settings.history;
    for(;;) {
        history_cells=(size_t)lines*MAX_COLUMNS;
        history=malloc(history_cells*sizeof *history);
        history_spare=malloc(history_cells*sizeof *history_spare);
        command_ring=malloc((size_t)commands*sizeof *command_ring);
        if(history && history_spare && command_ring) break;
        settings_free();
        if(lines==XSETTINGS_SCROLLBACK_DEFAULT && commands==XSETTINGS_HISTORY_DEFAULT) return -1;
        snprintf(settings_message,sizeof settings_message,
                 "Settings: not enough memory for %u lines and %u commands; defaults used",lines,commands);
        lines=XSETTINGS_SCROLLBACK_DEFAULT; commands=XSETTINGS_HISTORY_DEFAULT;
    }
    command_ring_size=commands;
    line_editor_attach_history(&command_editor,command_ring,command_ring_size);
    atexit(settings_free);
    return 0;
}

#ifndef XTERM_DOS_HANDLER
/* Project > Reload settings: the theme now, the rest in new windows. */
static void settings_reload(void)
{
    XSettings next; unsigned later;
    settings_default(&next);
    settings_message[0]=0;
    if(settings_load(&next)) {
        /* At start a wrong file means the built-in settings; here the
           window keeps what it has. */
        char *tail=strstr(settings_message,"; built-in settings used");
        if(tail) snprintf(tail,sizeof settings_message-(size_t)(tail-settings_message),"; nothing changed");
        clip_status=settings_message; return;
    }
    later=strcmp(next.font,settings.font) || next.font_size!=settings.font_size ||
          next.scrollback!=settings.scrollback || next.history!=settings.history;
    strcpy(settings.theme,next.theme);
    settings_theme();
    paint_damage_invalidate(&paint_damage);
    if(settings_message[0]) clip_status=settings_message;
    else clip_status=theme_from_command_line?
        (later?"Reloaded; --theme keeps its theme; font, scrollback and history apply to new windows":
               "Reloaded; --theme keeps its theme"):
        later?"Reloaded: theme applied; font, scrollback and history apply to new windows":
              "Settings reloaded";
}
#endif


/* The grid that fits the window's inner size. */
static void fit_grid(struct Window *window)
{
    /* Signed: OpenWindow does not hold a name's small size to the minimum,
       so the inner size can be under the margins. */
    int w=window->GZZWidth-2*MARGIN, h=window->GZZHeight-2*MARGIN;
    unsigned cols=w>0?(unsigned)w/xfont_width():0;
    unsigned rows=h>0?(unsigned)h/xfont_height():0;
    if(cols>MAX_COLUMNS) cols=MAX_COLUMNS;
    if(rows>MAX_ROWS) rows=MAX_ROWS;
    if(!cols) cols=1;
    if(!rows) rows=1;
    if(cols!=screen.columns || rows!=screen.rows) screen_resize(&screen,rows,cols);
}

static unsigned font_presentation_features(void)
{
    static unsigned generation,features;
    unsigned current=xfont_generation();
    if(current!=generation) {
        features=eza_icon_features(xfont_glyph,screen_scalar_width,xfont_width());
        generation=current;
    }
    return features;
}

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
    {
        /* The name the program opened, as CON: reads it. WINDOW (a window
           of the caller's) is not supported: this console draws its own. */
        const char *name=handler->startup->dp_Arg1?
            (const char *)AROS_BSTR_ADDR((BSTR)handler->startup->dp_Arg1):"";
        if(console_name_parse(&console_spec,name) || (console_spec.options&XCON_WINDOW)) {
            SetIoErr(ERROR_BAD_STREAM_NAME); return RETURN_FAIL;
        }
    }
#else
    /* Theme is a leading option; remaining arguments retain their strict grammar. */
    if(argc>1 && !strcmp(argv[1],"--theme")) {
        int i;
        if(argc<3 || !*argv[2]) {
            puts("--theme requires a file path"); return RETURN_ERROR;
        }
        if(theme_load(&theme,argv[2]))
            puts("Cannot load theme; using built-in dark theme");
        theme_from_command_line=1;
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
    settings_default(&settings);
    settings_load(&settings);
    settings_theme();
    if(settings_buffers()) {
        if(handler_mode) SetIoErr(ERROR_NO_FREE_STORE);
        else puts("Not enough memory for the scrollback");
        return RETURN_FAIL;
    }
    output_search.open=0;
    screen_init(&screen, cells, MAX_ROWS * MAX_COLUMNS, 24, 80);
    reset_links();
    if(!handler_mode) replay();
    if (settings_font()) {
        /* FreeType/Intuition do not establish DOS IoErr for the launcher. */
        if(handler_mode) SetIoErr(ERROR_OBJECT_WRONG_TYPE);
        return RETURN_FAIL;
    }
#ifdef XTERM_PTY_DIAG
    if(diag_exit_stage==2) { xfont_close();return RETURN_OK; }
#endif
    {
        ULONG flags=WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET |
                  WFLG_SIZEGADGET | WFLG_SIZEBBOTTOM | WFLG_ACTIVATE |
                  WFLG_GIMMEZEROZERO | WFLG_SIMPLE_REFRESH | WFLG_REPORTMOUSE;
        unsigned o=console_spec.options;
        /* The CON: options that say what the window looks like. */
        if(!console_spec.close_gadget) flags&=~WFLG_CLOSEGADGET;
        if(o&XCON_INACTIVE) flags&=~WFLG_ACTIVATE;
        if(o&XCON_NODEPTH) flags&=~WFLG_DEPTHGADGET;
        if(o&XCON_NOSIZE) flags&=~(WFLG_SIZEGADGET|WFLG_SIZEBBOTTOM);
        if(o&XCON_NODRAG) flags&=~WFLG_DRAGBAR;
        if(o&XCON_NOBORDER) flags|=WFLG_BORDERLESS;
        if(o&XCON_BACKDROP) { flags|=WFLG_BACKDROP; flags&=~(WFLG_DRAGBAR|WFLG_SIZEGADGET|WFLG_SIZEBBOTTOM); }
    window = OpenWindowTags(NULL,
        WA_Title, (IPTR)(console_spec.title[0]?console_spec.title:"Limpet"),
        WA_Left, console_spec.left>=0?console_spec.left:40,
        WA_Top, console_spec.top>=0?console_spec.top:60,
        console_spec.width>0?WA_Width:WA_InnerWidth,
            console_spec.width>0?console_spec.width:(long)(80 * xfont_width() + 2 * MARGIN),
        console_spec.height>0?WA_Height:WA_InnerHeight,
            console_spec.height>0?console_spec.height:(long)(24 * xfont_height() + 2 * MARGIN),
        console_spec.screen[0]?WA_PubScreenName:TAG_IGNORE,(IPTR)console_spec.screen,
        WA_PubScreenFallBack, TRUE,
        WA_MinWidth, 240, WA_MinHeight, 100,
        WA_MaxWidth, MAX_COLUMNS * xfont_width() + 2 * MARGIN,
        WA_MaxHeight, MAX_ROWS * xfont_height() + 2 * MARGIN,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_NEWSIZE | IDCMP_RAWKEY |
                  IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_INACTIVEWINDOW | IDCMP_MENUPICK,
        WA_Flags, flags,
        TAG_DONE);
    }
    if (!window) {
        xfont_close();
        if(handler_mode) SetIoErr(ERROR_NO_FREE_STORE);
        return RETURN_FAIL;
    }
    attach_menu(window);
    fit_grid(window);
#ifndef XTERM_PROCESS_PORT_CLIENTS
    /* Not in the XTERM: worker: a drop's paths take DOS calls (NameFromLock,
       Lock), which that process may not make while it serves its own port. */
    aros_drop_open(&file_drop,window);
#endif
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
            output_search.open=0;
            screen_init(&screen,cells,MAX_ROWS*MAX_COLUMNS,screen.rows,screen.columns);
            screen.amiga_pens=!pty_mode;
            reset_links();
            screen_attach_history(&screen,history,history_cells);
            screen_attach_reflow(&screen,history_spare,history_cells);
            screen_attach_alternate(&screen,alternate,MAX_ROWS*MAX_COLUMNS);
            clip_status=NULL;
        }
        /* Starting zeroes the session: the window goes in after. */
        process_session.window=window;
    }
    if(settings_message[0]) clip_status=settings_message;
    tick_open();
    settle_open();
    update_title(window, last_key);
    draw_screen(window);
    if(!flood_diag_start(window)) {
        settle_close();
        tick_close();
        if(frame_pixels) { FreeVec(frame_pixels); frame_pixels=NULL; frame_size=0; }
        close_main_window(window);xfont_close();return RETURN_FAIL;
    }
    PERF_DO(perf_open());
    while (running) {
        struct IntuiMessage *message;
        int dirty=0;
        /* A redraw waiting for the end of an open frame. */
        static int draw_deferred;
#ifdef XTERM_PERF_DIAG
        uint64_t loop_started;
#endif
        replies_sync();
        int paste_ready=paste.length && !paste.confirmation &&
            !reply_flow.length &&
            (echo_mode!=2 || (!process_session.eof &&
             XSESSION_CAPACITY-process_session.input.length>=1024 &&
             (!shell_session || process_session.line.raw || editor_ready())));
        if(!paste_ready && !keyboard_ready() && !flood_diag_ready() && !(echo_mode==2 && output_ready()))
            Wait((1UL<<window->UserPort->mp_SigBit)|SIGF_CHILD|tick_signal()|aros_drop_signal(&file_drop)|
                 aros_session_signals(&process_session)|diag_forward_signal()|flood_diag_signals());
        PERF_DO(loop_started=PERF_NOW(); ++perf_diag.loops);
        if(settle_poll()) { dirty=1; PERF_DO(++perf_diag.settle_draws); }
        diag_forward();
        process_session.columns=screen.columns;
        process_session.rows=screen.rows;
        process_session.presentation_enabled=1;
        process_session.presentation_features=font_presentation_features;
        process_session.amiga_pens=screen.amiga_pens;
        aros_session_poll(&process_session);
        if(drop_files(closing)) dirty=1;
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
                if(!settle_hold()) dirty=1;
            }
            free(kept);
            if(sync_track() && (settle.held || draw_deferred)) { dirty=1; draw_deferred=0; }
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
                /* WAIT: the window stays until the user closes it. */
                if(handler_mode && (console_spec.options&XCON_WAIT)) {
                    /* Once: later messages (Copied, Find) must show. */
                    if(!wait_reported) clip_status="Ended (WAIT): close the window";
                    wait_reported=1;
                }
                else if(handler_mode) closing=TRUE;
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
#ifdef XTERM_PERF_DIAG
            if(event==IDCMP_RAWKEY && !(code&IECODE_UP_PREFIX)) {
                uint64_t stamp=(uint64_t)message->Seconds*1000000u+message->Micros,now=PERF_NOW();
                ++perf_diag.keys;
                if(code==0x4D) ++perf_diag.downs;
                if(mouse_qualifiers&IEQUALIFIER_REPEAT) ++perf_diag.repeats;
                if(now>stamp) PERF_MAX(key_latency_max_us,now-stamp);
            }
#endif
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
                    case MENU_FIND: if(!closing) find_start(); break;
#ifndef XTERM_DOS_HANDLER
                    case MENU_RELOAD: if(!closing) settings_reload(); break;
#endif
                    }
                    pick=item->NextSelect;
                }
                dirty=1;
                if(!close_now) continue;
                event=IDCMP_CLOSEWINDOW;
            }
            if (event == IDCMP_CLOSEWINDOW) {
                output_search.open=0;
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
                /* A raw-mode client that asked (CSI > 4 ; 1 m) tells Shift+Enter
                   from Enter: a new line in its editor rather than a send. */
                if(screen.modify_enter && process_session.line.raw && count==1 && bytes[0]=='\r' &&
                   (code==RAWKEY_RETURN || code==RAWKEY_KP_ENTER) &&
                   (q&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)) &&
                   !(q&(IEQUALIFIER_CONTROL|IEQUALIFIER_LALT|IEQUALIFIER_RALT|IEQUALIFIER_LCOMMAND|IEQUALIFIER_RCOMMAND))) {
                    size_t n=terminal_encode_modified_enter(TERMINAL_SHIFT,bytes,sizeof bytes);
                    if(n) count=n;
                }
                if(action==3) { find_start(); dirty=1; continue; }
                /* Copy and paste work during Find too (a paste goes into
                   the search text, see clipboard_completed). */
                if(action) { clipboard_action(action); dirty=1; continue; }
                if(find_key(code,q,bytes,count)) { dirty=1; continue; }
                if((code==RAWKEY_PAGEUP || code==RAWKEY_PAGEDOWN) &&
                   (q&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT))) {
                    screen_scroll_view(&screen,code==RAWKEY_PAGEUP?(int)screen.rows-1:1-(int)screen.rows);
                    dirty=1; continue;
                }
                if(code==RAWKEY_NM_WHEEL_UP || code==RAWKEY_NM_WHEEL_DOWN) {
                    if(mouse_releases.count) continue;
                    /* Mode 2100 (hardcode L5): wheel steps go to the program,
                       buttons stay with the terminal, so a plain drag still selects. */
                    if(process_session.console && process_session.line.raw && !output_search.open &&
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
                    process_session.window=window;
                    output_reset();
                    program_title_clear(&program_title); memset(&foreground,0,sizeof foreground);
                    echo_session_close(&echo); echo_mode=2; process_reported=0;
                    memset(&command_editor,0,sizeof command_editor);
                    line_editor_attach_history(&command_editor,command_ring,command_ring_size);
                    selection.active=selection.dragging=0;
                    output_search.open=0;
                    screen_init(&screen,cells,MAX_ROWS*MAX_COLUMNS,screen.rows,screen.columns);
                    screen.amiga_pens=!pty_mode;
                    reset_links();
                    screen_attach_history(&screen,history,history_cells);
                    screen_attach_reflow(&screen,history_spare,history_cells);
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
                    output_search.open=0;
                    screen_init(&screen,cells,MAX_ROWS*MAX_COLUMNS,screen.rows,screen.columns);
                    reset_links();
                    screen_attach_history(&screen,history,history_cells);
                    screen_attach_reflow(&screen,history_spare,history_cells);
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
        if(!closing && !output_search.open && keyboard_ready()) {
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
        if(!closing && !output_search.open && !reply_flow.length && paste.length && !paste.confirmation &&
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
        if(output_search.open) {
            unsigned offset=screen.view_offset;
            unsigned long index=output_search.index,total=output_search.total;
            if(output_search_follow(&output_search,&screen)) {
                clip_status="Find closed: a full-screen program took the screen"; dirty=1;
            } else if(offset!=screen.view_offset || index!=output_search.index ||
                      total!=output_search.total) dirty=1;
        }
        /* Anything drawn while a frame is open (a key, the title's timer,
         * a status message) would paint the half of it that has arrived;
         * it is drawn with the frame's end, or at the frame's time limit.
         * Closing draws at once. */
        if(dirty && !closing && sync_holding()) { draw_deferred=1; dirty=0; }
        else if(draw_deferred && !sync_holding()) { dirty=1; draw_deferred=0; }
        if(running && dirty) {
            uint64_t started=settle.timed?settle_now():0;
            update_title(window,last_key); draw_screen(window); settle_drawn(started);
        }
        PERF_MAX(key_queue_max,keyboard.count);
        PERF_MAX(input_backlog_max,process_session.input.length);
        PERF_DO(PERF_MAX(loop_gap_max_us,PERF_NOW()-loop_started); perf_tick());
        if(running && (title_pending || title_hold || status_expiring() || foreground_watch ||
                       (echo_mode==2 && foreground.running))) tick_arm();
    }
    settle_close();
    tick_close();
    if(frame_pixels) { FreeVec(frame_pixels); frame_pixels=NULL; frame_size=0; }
    PERF_DO(perf_close());
    aros_session_dispose(&process_session);
#ifdef XTERM_PKTWAIT_PROBE
    aros_handler_probe_stop();
#endif
    echo_session_close(&echo); close_main_window(window); xfont_close();
    return RETURN_OK;
}
