#ifndef AROS_TTY_SERVICE_H
#define AROS_TTY_SERVICE_H
#include "tty_pair.h"

#define TTY_SERVICE_HANDLES 16
#define TTY_SERVICE_REQUESTS 32
#define TTY_SERVICE_LANE_REQUESTS (TTY_SERVICE_REQUESTS / 4)
typedef uint64_t TtyHandle;
typedef enum { TTY_REQUEST_READ, TTY_REQUEST_WRITE, TTY_REQUEST_DRAIN,
    TTY_REQUEST_DRAIN_CONFIG, TTY_REQUEST_DRAIN_CONFIG_DISCARD } TtyOperation;
typedef struct {
    TtyHandle handle;
    TtyOperation operation;
    void *buffer;
    size_t length;
    TtyResult result;
    int cancelled;
} TtyRequest;
typedef struct { TtyHandle id; TtyEnd end; } TtyServiceHandle;
typedef struct {
    TtyRequest *request;
    TtyEnd end;
    uint64_t order;
    int complete;
    int drain_started;
    uint64_t drain_start;
    size_t drain_bytes;
    size_t written; /* slave write progress: DOS Write() expects all bytes */
    TtyConfig config;
    uint64_t read_generation, read_arrived, timer_token;
    uint64_t signal_generation;
    unsigned timer_ds;
    int read_started, timer_expired;
} TtyServiceSlot;
typedef struct {
    TtyPair pair;
    TtyServiceHandle handles[TTY_SERVICE_HANDLES];
    TtyServiceSlot slots[TTY_SERVICE_REQUESTS];
    uint64_t next_handle, next_order;
    uint64_t next_timer;
    /* Serialized owner hook: must not reenter the service. Context outlives
       registration; remove it before destroying owner resources. */
    void (*interrupt)(void *);
    void *interrupt_context;
    uint64_t signal_generation; /* Monotonic route identity; never wrap/reuse. */
} TtyService;

/* Internal adapter API, not DOS/libc ABI. Caller serializes all operations.
   Fresh storage only. Initial handles are master=1, slave=2.
   Accepted requests AND their buffers remain caller-owned but must not be
   modified/released/resubmitted until collected by tty_service_take(). */
void tty_service_init(TtyService *);
TtyStatus tty_service_duplicate(TtyService *, TtyHandle, TtyHandle *);
TtyStatus tty_service_close(TtyService *, TtyHandle);
TtyStatus tty_service_submit(TtyService *, TtyRequest *);
/* Immediate internal READ/WRITE attempt, serialized by the same owner.
   Never retains buffer/request storage or allocates a service slot. Pending
   older operations in the same lane take precedence, including write drains.
   Raw reads ignore VMIN/VTIME; empty live input returns AGAIN, not timed zero.
   Canonical record boundaries, partial transfers and EOF remain pair semantics.
   Zero-length I/O validates the handle but does not pump or consume signals.
   Not a public nonblocking fd API; caller translates status at its boundary. */
TtyResult tty_service_try_io(TtyService *,TtyHandle,TtyOperation,void *,size_t);
/* DRAIN: slave-only, NULL buffer/zero length. Shares the slave-write FIFO and
   slot quota. Captures queued output after earlier write requests complete;
   later echo does not extend it. Output flush also retires captured bytes.
   Completion means master consumption/discard, not pixels rendered. */
/* DRAIN_CONFIG[_DISCARD]: buffer points to a TtyConfig, length == sizeof it.
   Validate and copy at admission; apply after drain, before later writes.
   Without DISCARD, accepted input records and any partial line are preserved. */
TtyStatus tty_service_cancel(TtyService *, TtyRequest *);
/* Cancel every pending request atomically, without pumping between requests. */
size_t tty_service_cancel_all(TtyService *);
void tty_service_pump(TtyService *);
/* Owner arms/rearms a relative timer for this generation. Only the FIFO head
   can request one. Expiry is ignored after completion/cancel/reconfiguration. */
unsigned tty_service_read_timer(TtyService *, TtyRequest **, uint64_t *);
void tty_service_read_timeout(TtyService *, TtyRequest *, uint64_t);
TtyRequest *tty_service_take(TtyService *);
/* Safe storage retirement requires no handles and no uncollected requests. */
int tty_service_retired(const TtyService *);
#endif
