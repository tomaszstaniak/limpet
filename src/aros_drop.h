#ifndef XTERM_AROS_DROP_H
#define XTERM_AROS_DROP_H
#include <exec/types.h>
#include "file_drop.h"
/* The window as a Workbench AppWindow: icons dropped on it arrive as one
 * AppMessage. Wanderer waits for the reply holding the Workbench
 * semaphore, so a message is answered as soon as its paths are copied
 * (docs/file-drop.md). Nothing from a message is kept past the reply.
 *
 * RemoveAppWindow takes that semaphore exclusively, so while it waits a
 * drop must be answered by someone other than this task: closing starts a
 * small process that replies to every drop until the AppWindow is gone,
 * and the port is freed only once that process has exited. */
struct Window;
struct MsgPort;
typedef struct {
    struct MsgPort *port;
    void *appwindow;
    ULONG answerer;             /* the answering process's task id, while closing */
    volatile int stop;          /* the AppWindow is gone: the answerer drains and ends */
    volatile ULONG answered;    /* drops answered unread while closing */
} ArosDrop;
typedef enum { ARDROP_OK, ARDROP_GONE, ARDROP_TOO_LONG, ARDROP_TOO_MANY } ArosDropError;
typedef struct {
    size_t count;
    const char *paths[XDROP_MAX_FILES];
    char pool[2 * XLINE_CAPACITY];
    ArosDropError error;
    char name[64];              /* ARDROP_GONE: the object that is not there */
} ArosDropped;
/* 1 the window takes drops; 0 it does not (no workbench.library, no memory). */
int aros_drop_open(ArosDrop *, struct Window *);
ULONG aros_drop_signal(const ArosDrop *);
/* One waiting drop: its paths copied to *out, then replied. Returns 1 when
   a drop was taken (out->error says whether its paths are usable), 0 when
   none waits. */
int aros_drop_take(ArosDrop *, ArosDropped *out);
/* Replies every waiting drop unread; returns how many there were. */
int aros_drop_refuse(ArosDrop *);
/* Before the window closes: begin, then end. From begin on every drop is
   answered unread without the task; end returns how many were, those the
   task had not taken yet included. Only senders that hold the Workbench
   semaphore while they wait (SendAppWindowMessage, Wanderer) are covered;
   one that kept the port and sent later would break the AppWindow
   contract anyway. */
void aros_drop_close_begin(ArosDrop *);
ULONG aros_drop_close_end(ArosDrop *);
void aros_drop_close(ArosDrop *);
#endif
