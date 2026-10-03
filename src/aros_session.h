#ifndef XTERM_AROS_SESSION_H
#define XTERM_AROS_SESSION_H
#include "session_queue.h"
#include "console_input.h"
#include "aros_break.h"
#include <exec/ports.h>
#include <dos/dosextens.h>

/* Owned foreground command or externally owned endpoint; one outstanding
 * read and write. All functions
 * except the internal child entry run on the owning GUI/test process. No locks
 * are needed for queues: the child accesses them only through DOS packets.
 * Zero-initialize before first start. Owned mode does not support background
 * children; external mode retains explicit handles/leases without owning its
 * clients. Caller must stop all packet producers before freeing the endpoint. */
typedef struct {
    struct MsgPort *port;
    struct DosPacket *pending_read,*pending_write;
    struct DosPacket *pending_wait;
    struct MsgPort *timer_port;
    struct timerequest *timer;
    int timer_active;
    XSessionQueue input,output;
    XConsoleInput line;
    int console,shell;
    unsigned input_generation;
    unsigned columns,rows; /* Optional display geometry, zero without a GUI. */
    int presentation_enabled; /* Set by an attached XTerm renderer only. */
    int amiga_pens; /* The renderer reads SGR 30-37/40-47 as pens (op 13 says so). */
    int external,opened;
    BPTR startup_input,startup_file;
    unsigned startup_closes;
    IPTR signal_target;
    ArosBreak breaker;
    size_t write_offset;
    ULONG task_id,handles;
    unsigned references[3];
    struct FileLock locks[16];
    unsigned lock_mask;
    LONG result;
    LONG unsupported_action;
    int eof,stopping,exited;
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
int aros_session_interrupt(ArosSession *);
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
