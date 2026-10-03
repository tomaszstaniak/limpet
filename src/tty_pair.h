#ifndef AROS_TTY_PAIR_H
#define AROS_TTY_PAIR_H
#include <stddef.h>
#include <stdint.h>

/* Internal service core, NOT a public DOS/libc ABI. Caller serializes calls. */
#define TTY_PAIR_VERSION 1
#define TTY_QUEUE_CAPACITY 1024
#define TTY_LINE_CAPACITY 256
#define TTY_CANON 1u
#define TTY_ECHO 2u
#define TTY_ECHOE 4u
#define TTY_ECHOK 8u
#define TTY_ECHONL 16u
#define TTY_NOEOF 32u
#define TTY_ISIG 64u
#define TTY_NOFLSH 128u
#define TTY_LOCAL_MASK 255u
#define TTY_ICRNL 1u
#define TTY_IGNCR 2u
#define TTY_INLCR 4u
#define TTY_OPOST 1u
#define TTY_ONLCR 2u
#define TTY_FLUSH_INPUT 1u
#define TTY_FLUSH_OUTPUT 2u
typedef enum { TTY_MASTER, TTY_SLAVE } TtyEnd;
typedef enum { TTY_OK, TTY_AGAIN, TTY_EOF, TTY_CLOSED, TTY_INVALID,
               TTY_UNSUPPORTED, TTY_BUSY } TtyStatus;
typedef struct { TtyStatus status; size_t count; } TtyResult;
typedef struct {
    uint32_t version, input_flags, output_flags, local_flags;
    unsigned char eof;
    /* Zero disables; 1..256 encode byte + 1, including a literal NUL. */
    uint16_t erase, kill, eol, intr;
    unsigned char vmin, vtime; /* Read policy belongs to the service, not pair I/O. */
} TtyConfig;
typedef struct { uint32_t version, rows, columns, xpixel, ypixel; } TtyGeometry;
typedef struct {
    uint16_t data[TTY_QUEUE_CAPACITY];
    size_t head, count;
} TtyRing;
typedef struct {
    TtyRing input, output;
    uint64_t output_retired; /* Master-consumed or explicitly discarded bytes. */
    uint64_t input_arrived, config_generation;
    unsigned char line[TTY_LINE_CAPACITY];
    size_t line_count;
    int line_ready; /* Frozen pre-switch tail, ordered after the input ring. */
    int output_stopped;
    int interrupt_pending;
    unsigned refs[2];
    TtyConfig config;
    TtyGeometry geometry;
} TtyPair;

/* Initialize fresh/unreferenced storage. Initially raw, no echo, 80x24. */
void tty_pair_init(TtyPair *);
TtyStatus tty_pair_retain(TtyPair *, TtyEnd);
TtyStatus tty_pair_release(TtyPair *, TtyEnd);
TtyResult tty_pair_write(TtyPair *, TtyEnd, const void *, size_t);
TtyResult tty_pair_read(TtyPair *, TtyEnd, void *, size_t);
/* Serialized owner collects before dispatch/pump; no task pointer is stored.
   A pending event blocks nonempty master writes, preserving their suffix. */
int tty_pair_take_interrupt(TtyPair *);
int tty_pair_readable(const TtyPair *, TtyEnd);
/* Readable input bytes through the first preserved record/EOF boundary. */
size_t tty_pair_input_extent(const TtyPair *, int *boundary);
/* Non-consuming next-read outcome: OK (data), EOF, AGAIN, CLOSED or INVALID. */
TtyStatus tty_pair_read_status(const TtyPair *, TtyEnd);
TtyStatus tty_config_validate(const TtyConfig *);
/* Preserve accepted records/EOF; entering raw also releases a partial line.
   Flags apply only to future input. Pump the owner after a successful change. */
TtyStatus tty_pair_configure(TtyPair *, const TtyConfig *);
/* Validate completely, then discard accepted input and install config within
   the serialized owner. Output is preserved; this does not wait for drain. */
TtyStatus tty_pair_configure_discard_input(TtyPair *, const TtyConfig *);
/* Slave-oriented discard of already accepted bytes, not pending requests.
   Caller must pump the service after success. Peer closure is not reopened. */
TtyStatus tty_pair_flush(TtyPair *, unsigned mask);
/* Explicit output suspension; master reads wait, queued output is retained.
   Final slave close releases the remaining output even while stopped. */
TtyStatus tty_pair_output_flow(TtyPair *, int stopped);
TtyStatus tty_pair_get_config(const TtyPair *, uint32_t, TtyConfig *);
TtyStatus tty_pair_set_geometry(TtyPair *, const TtyGeometry *);
TtyStatus tty_pair_get_geometry(const TtyPair *, uint32_t, TtyGeometry *);
#endif
