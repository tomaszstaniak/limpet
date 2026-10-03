#include "pump.h"

#include <string.h>

void ssh_pump_init(struct ssh_pump *pump, int command_mode)
{
    memset(pump, 0, sizeof(*pump));
    pump->local_eof = command_mode != 0;
}

static int fail(struct ssh_pump *pump)
{
    pump->terminal = SSH_PUMP_ERROR;
    return SSH_PUMP_ERROR;
}

static void consume(struct ssh_pump_buffer *buffer, size_t count)
{
    buffer->offset += count;
    buffer->length -= count;
    if (!buffer->length) buffer->offset = 0;
}

int ssh_pump_step(struct ssh_pump *pump, const struct ssh_pump_ops *ops,
                  void *context, int *progress)
{
    int remote_done, i, moved = 0, empty_streams = 0;
    int result;
    struct ssh_pump_buffer *buffer;

    if (progress) *progress = 0;
    if (pump->terminal) return pump->terminal;
    remote_done = ops->remote_done(context);
    if (remote_done < 0) return fail(pump);

    /* Each stream gets a bounded turn, even when its peer is continuously
     * readable or has a long unwritten output tail. Alternate first service. */
    for (i = 0; i < 2; ++i) {
        int stream = (pump->next_stream + i) % 2;
        buffer = &pump->output[stream];
        if (!buffer->length) {
            result = ops->remote_read(context, stream, buffer->data,
                                      sizeof(buffer->data));
            if (result < 0 || (size_t)result > sizeof(buffer->data))
                return fail(pump);
            buffer->length = (size_t)result;
            if (result) moved = 1;
            else ++empty_streams;
        }
        if (buffer->length) {
            result = ops->local_write(context, stream,
                                      buffer->data + buffer->offset,
                                      buffer->length);
            if (result <= 0 || (size_t)result > buffer->length)
                return fail(pump);
            consume(buffer, (size_t)result);
            moved = 1;
        }
    }
    pump->next_stream ^= 1;

    /* A completed peer cannot consume more input. Read both streams until
     * each reports empty, including a fresh read after any saved tail. */
    if (remote_done) {
        if (empty_streams == 2) pump->terminal = SSH_PUMP_DONE;
        if (progress) *progress = moved;
        return pump->terminal;
    }

    /* Reading can process a network CLOSE. Recheck before touching input,
     * and make a fresh drain pass next time: an earlier empty stream may
     * have acquired buffered bytes while the other stream was serviced. */
    remote_done = ops->remote_done(context);
    if (remote_done < 0) return fail(pump);
    if (remote_done) {
        if (progress) *progress = moved;
        return SSH_PUMP_ACTIVE;
    }

    buffer = &pump->input;
    if (!pump->local_eof && !buffer->length) {
        result = ops->local_read(context, buffer->data, sizeof(buffer->data));
        if (result == SSH_PUMP_INPUT_EOF) {
            pump->local_eof = 1;
        } else {
            if (result < 0 || (size_t)result > sizeof(buffer->data))
                return fail(pump);
            buffer->length = (size_t)result;
            if (result) moved = 1;
        }
    }
    if (buffer->length) {
        result = ops->remote_write(context, buffer->data + buffer->offset,
                                   buffer->length);
        if (result < 0 || (size_t)result > buffer->length)
            return fail(pump);
        if (result) {
            consume(buffer, (size_t)result);
            moved = 1;
        }
    }
    if (pump->local_eof && !buffer->length && !pump->eof_sent) {
        result = ops->send_eof(context);
        if (result < 0) return fail(pump);
        if (result) {
            pump->eof_sent = 1;
            moved = 1;
        }
    }
    if (progress) *progress = moved;
    return SSH_PUMP_ACTIVE;
}
