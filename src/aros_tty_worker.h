#ifndef AROS_TTY_WORKER_H
#define AROS_TTY_WORKER_H
#include <dos/dos.h>
#include "tty_pair.h"
typedef struct ArosTtyWorker ArosTtyWorker;
struct Task;
/* Creator-only synchronous bind. Non-NULL target must remain launch-gated/live
   until return. NULL revokes; no old event is transferred to a new target. */
TtyStatus aros_tty_worker_signal_target(ArosTtyWorker *,struct Task *);
/* Creator-Process API; calls are serialized. No mounted/public PTY ABI.
   Start returns two caller-owned DOS FileHandles, closable with normal Close.
   Outputs are unchanged on failure. The creator/code must outlive the worker. */
ArosTtyWorker *aros_tty_worker_start(BPTR *master,BPTR *slave);
/* Source must be live, belong to this worker and stay open until return. */
TtyStatus aros_tty_worker_duplicate(ArosTtyWorker *,BPTR source,BPTR *copy);
TtyStatus aros_tty_worker_configure(ArosTtyWorker *,const TtyConfig *);
TtyStatus aros_tty_worker_get_config(ArosTtyWorker *,uint32_t version,TtyConfig *);
TtyStatus aros_tty_worker_set_geometry(ArosTtyWorker *,const TtyGeometry *);
TtyStatus aros_tty_worker_get_geometry(ArosTtyWorker *,uint32_t version,TtyGeometry *);
/* Cancels I/O but retains ports until every exported handle has been closed. */
int aros_tty_worker_stop(ArosTtyWorker *);
/* Busy => -1, worker remains usable. Success => 0, worker is reaped and freed.
   Caller must quiesce its clients and close every export, including duplicates. */
int aros_tty_worker_finish(ArosTtyWorker *);
#endif
