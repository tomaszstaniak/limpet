#ifndef AROS_TTY_PACKETS_H
#define AROS_TTY_PACKETS_H
#include <dos/dosextens.h>
#include <devices/timer.h>
#include "tty_service.h"
#include "tty_control.h"

typedef struct {
    struct DosPacket *packet;
    TtyRequest request;
    XtyControl control;
    TtyConfig config;
} ArosTtyPacketSlot;
typedef struct {
    struct DosPacket *packet;
    struct timerequest *timer;
} ArosTtyWait;
typedef struct {
    TtyService service;
    struct MsgPort *ports[2]; /* Endpoint identity for handle-less DOS controls. */
    struct MsgPort *timer_port;
    ArosTtyWait waits[2]; /* One non-consuming waiter per endpoint. */
    struct timerequest *read_timer;
    TtyRequest *timed_read;
    uint64_t read_timer_token;
    ArosTtyPacketSlot slots[TTY_SERVICE_REQUESTS];
    TtyHandle exports[TTY_SERVICE_HANDLES]; /* Survive stop until ACTION_END. */
    TtyEnd export_ends[TTY_SERVICE_HANDLES];
    int stopping;
    int presentation_enabled; /* Renderer owner opt-in; generic PTYs stay zero. */
    /* Optional owner hook for ACTION_CHANGE_SIGNAL, the DOS console contract
       for the Ctrl-C target (con-handler's breaktask). Set after init. With
       no hook the packet keeps its ERROR_ACTION_NOT_KNOWN reply. sender is
       the task that sent the packet and is waiting for the reply; target is
       the requested task or NULL (query only); *old receives the previous
       target for the reply's Res2, as con-handler returns it. handle is the
       PTY handle the packet was sent for, or 0 for the implicit move when
       the sender reads (a foreground reader takes the break); that move may
       only change an existing route, never create one or change PTY flags. */
    TtyStatus (*change_signal)(void *context,struct Task *sender,struct Task *target,struct Task **old,TtyHandle handle);
    void *change_signal_context;
} ArosTtyPackets;

/* All operations run in the owning Process. No concurrent calls.
   FileHandles are fresh AllocDosObject(DOS_FILEHANDLE) objects owned by caller;
   normal DOS Close frees them. Initialization binds exactly two raw endpoints. */
int aros_tty_packets_init(ArosTtyPackets *,struct FileHandle *,struct FileHandle *);
TtyStatus aros_tty_packets_duplicate(ArosTtyPackets *,TtyHandle,struct FileHandle *);
void aros_tty_packets_poll(ArosTtyPackets *);
/* Owner-only cancellation; reply must still be collected before freeing packet. */
TtyStatus aros_tty_packets_cancel(ArosTtyPackets *,struct DosPacket *);
void aros_tty_packets_stop(ArosTtyPackets *);
/* Returns -1 while busy. Caller must quiesce producers before disposing. */
int aros_tty_packets_dispose(ArosTtyPackets *);
#endif
