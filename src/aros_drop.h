#ifndef XTERM_AROS_DROP_H
#define XTERM_AROS_DROP_H
#include <exec/types.h>
#include "file_drop.h"
/* The window as a Workbench AppWindow: icons dropped on it arrive as one
 * AppMessage. Wanderer waits for the reply holding the Workbench
 * semaphore, so a message is answered as soon as its paths are copied
 * (docs/file-drop.md). Nothing from a message is kept past the reply. */
struct Window;
struct MsgPort;
typedef struct {
    struct MsgPort *port;
    void *appwindow;
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
/* Before the window closes. */
void aros_drop_close(ArosDrop *);
#endif
