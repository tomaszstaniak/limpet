#ifndef XTERM_SSH_PUMP_H
#define XTERM_SSH_PUMP_H

#include <stddef.h>

#define SSH_PUMP_BUFFER_SIZE 4096
#define SSH_PUMP_INPUT_EOF (-2)
#define SSH_PUMP_ERROR (-1)
#define SSH_PUMP_ACTIVE 0
#define SSH_PUMP_DONE 1

/* All callbacks must return promptly. Byte callbacks return a positive count,
 * zero for would-block, or a negative error. local_read alone may return
 * SSH_PUMP_INPUT_EOF. local_write returning zero is an error, not a retry.
 * stream is 0 for stdout and 1 for stderr.
 * send_eof returns positive on success, zero to retry, negative on error.
 * remote_done returns positive when no further network bytes can arrive,
 * zero while active, negative on error. Buffered remote_read data must remain
 * readable after remote_done becomes positive; zero then means drained.
 */
struct ssh_pump_ops {
    int (*remote_read)(void *, int, unsigned char *, size_t);
    int (*remote_write)(void *, const unsigned char *, size_t);
    int (*local_read)(void *, unsigned char *, size_t);
    int (*local_write)(void *, int, const unsigned char *, size_t);
    int (*send_eof)(void *);
    int (*remote_done)(void *);
};

struct ssh_pump_buffer {
    unsigned char data[SSH_PUMP_BUFFER_SIZE];
    size_t offset;
    size_t length;
};

struct ssh_pump {
    struct ssh_pump_buffer input;
    struct ssh_pump_buffer output[2];
    int local_eof;
    int eof_sent;
    int next_stream;
    int terminal;
};

/* command_mode starts at local EOF and never invokes local_read. */
void ssh_pump_init(struct ssh_pump *pump, int command_mode);

/* One bounded pass, with at most one read/write per stream and one input
 * read/write. Returns ACTIVE, DONE, or ERROR; terminal results are sticky.
 * Optional progress is set when bytes move or EOF is accepted. A completed
 * peer's buffered output is drained before DONE; local input is not polled
 * once completion is observed, and unsent input is discarded at completion.
 */
int ssh_pump_step(struct ssh_pump *pump, const struct ssh_pump_ops *ops,
                  void *context, int *progress);

#endif
