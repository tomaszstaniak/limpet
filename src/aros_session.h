#ifndef XTERM_AROS_SESSION_H
#define XTERM_AROS_SESSION_H
#include "session_queue.h"
#include "console_input.h"
#include "aros_break.h"
#include "script_tail.h"
#include <exec/ports.h>
#include <dos/dosextens.h>
#include <exec/io.h>
#include <devices/conunit.h>

/* Owned foreground command or externally owned endpoint; reads, writes and
 * WaitForChar calls from several clients served in order. All functions
 * except the internal child entry run on the owning GUI/test process. No locks
 * are needed for queues: the child accesses them only through DOS packets.
 * Zero-initialize before first start. Owned mode does not support background
 * children; external mode retains explicit handles/leases without owning its
 * clients. Caller must stop all packet producers before freeing the endpoint. */
typedef struct {
    struct MsgPort *port;
    /* pending_read is the read being served; others wait in read_wait, in
     * order, as CON: queues them. Waiters (WaitForChar) each have their own
     * timer; input answers all of them. */
    struct DosPacket *pending_read,*pending_write;
#define XSESSION_READERS 16
#define XSESSION_WAITERS 16
    struct DosPacket *read_wait[XSESSION_READERS];
    unsigned read_waiting;
    struct { struct DosPacket *packet; struct timerequest *timer; int active; } waits[XSESSION_WAITERS];
    unsigned wait_count;
    struct MsgPort *timer_port;
    XSessionQueue input,output;
    XConsoleInput line;
    int console,shell;
    unsigned input_generation;
    unsigned columns,rows; /* Optional display geometry, zero without a GUI. */
    unsigned pixel_width,pixel_height; /* Complete cells only, excluding margins. */
    unsigned (*presentation_features)(void); /* Renderer-owned live font evidence. */
    int presentation_enabled; /* Set by an attached XTerm renderer only. */
    int amiga_pens; /* The renderer reads SGR 30-37/40-47 as pens (op 13 says so). */
    int external,opened;
    BPTR startup_input,startup_file;
    unsigned startup_closes;
    /* The startup script as the Shell reads it: the file, then
       startup_tail (aros_session_start_shell_run), one stream. */
#define XSESSION_TAIL_MAX 512
    XScriptStream script;
    char startup_tail[XSESSION_TAIL_MAX];
    int script_proxy;
    IPTR signal_target;
    APTR window; /* The console's window for ACTION_DISK_INFO, or NULL. */
    /* What ACTION_DISK_INFO hands out as CON: does: an IOStdReq whose unit
     * is a ConUnit with the grid's size and the window. Read-only: posixc's
     * TIOCGWINSZ reads cu_XMax/cu_YMax/cu_Window through id_InUse and read
     * through a null pointer before. It is no console.device: io_Device is
     * NULL, and a program sending it I/O fails. */
    struct IOStdReq info_request;
    struct ConUnit info_unit;
    ArosBreak breaker;
    size_t write_offset;
    /* Writes from other clients while pending_write waits for room, in
     * order; CON: makes every writer wait too, it never refuses one. */
#define XSESSION_WRITERS 16
    struct DosPacket *write_wait[XSESSION_WRITERS];
    unsigned write_waiting;
    ULONG task_id,handles;
    unsigned references[3];
    struct FileLock locks[16];
    unsigned lock_mask;
    LONG result;
    LONG unsupported_action;
    int eof,stopping,exited;
    /* Ctrl-\ in cooked mode: the next Read returns 0 once, then reads wait
       again, as CON: does. eof (F5, close) stays for good. */
    int eof_once;
    char command[512];
#ifdef XTERM_PROCESS_PORT_CLIENTS
    int borrowed_port; /* port is this process's pr_MsgPort; never deleted */
#endif
} ArosSession;
int aros_session_start(ArosSession *,const char *command);
int aros_session_start_console(ArosSession *,const char *command);
int aros_session_start_shell(ArosSession *);
/* Caller owns initial admission and must stop all packet producers before
 * disposing. No DOS-list registration and no owned child are created.
 * With XTERM_PROCESS_PORT_CLIENTS the endpoint serves the calling process's
 * own pr_MsgPort. An ABIv11 DOS (deadwood2, including ABIv11_2026.09) always
 * sets a handle's fh_Type to the port the FIND went to, so clients reach the
 * handler process port whatever the FIND reply said. That process must then
 * make no synchronous DOS call while the endpoint is open: DoPkt waits for
 * its reply on the same port and would take a client packet instead. */
int aros_session_start_endpoint(ArosSession *);
int aros_session_active(const ArosSession *);
/* NULL: optional S:Shell-Startup; "": clean session; otherwise required file. */
int aros_session_start_shell_profile(ArosSession *,const char *profile);
/* The same, and the Shell then runs line once as the startup script's last
   line: after the startup file, in this session's Shell, before its first
   prompt. Ctrl-C ends it as any command; if it fails the Shell carries on.
   Sub-Shells (NewShell, Execute) read their own startup and never see it.
   NULL or "" runs nothing. At most XSESSION_TAIL_MAX-3 bytes, no line end. */
int aros_session_start_shell_run(ArosSession *,const char *profile,const char *line);
int aros_session_interrupt(ArosSession *);
/* Ctrl-D, Ctrl-E, Ctrl-F in cooked mode, as CON: does them: the break
   signal to the console's reader, nothing into the line. Raw clients get
   the bytes instead. Returns whether a signal went out. */
int aros_session_break_signal(ArosSession *,ULONG signals);
/* Ctrl-\ in cooked mode: what was typed goes to the reader without a line
   end, then one Read returns 0 (end of input). Returns 0 when it cannot
   (raw mode, no room, no console). */
int aros_session_end_input(ArosSession *);
/* What the console's current reader runs (aros_break_foreground); known
   is 0 when the session is not a console or no reader is bound. */
int aros_session_foreground(ArosSession *,XForeground *);
/* Resolve before entering SystemTags, whose Shell has its own PROGDIR. */
int aros_session_peer_command(char *command,size_t capacity);
ULONG aros_session_signals(const ArosSession *);
void aros_session_poll(ArosSession *);
size_t aros_session_write(ArosSession *,const unsigned char *,size_t);
size_t aros_session_read(ArosSession *,unsigned char *,size_t);
void aros_session_eof(ArosSession *);
void aros_session_stop(ArosSession *);
/* Refuses to free a live child, open endpoint or borrowed packet. */
int aros_session_dispose(ArosSession *);
#endif
