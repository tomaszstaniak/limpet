#include "../examples/ssh/pump.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

struct fixture {
    const char *input;
    size_t input_at;
    const char *remote[2];
    size_t remote_at[2];
    char sent[20000];
    size_t sent_size;
    char output[2][20000];
    size_t output_size[2];
    int input_calls;
    int read_calls[2];
    int write_calls;
    int eof_calls;
    int eof_accepted;
    int eof_retry;
    int remote_closed;
    int close_during_read;
    int close_after_eof;
    int input_idle;
    int read_again[2];
    int write_again;
    size_t remote_limit;
    size_t local_limit;
    int fail;
};

static int rr(void *context, int stream, unsigned char *data, size_t size)
{
    struct fixture *f = context;
    size_t n = strlen(f->remote[stream]) - f->remote_at[stream];
    ++f->read_calls[stream];
    if (f->close_during_read) f->remote_closed = 1;
    if (f->fail == 1 + stream) return -1;
    if (f->read_again[stream]) { --f->read_again[stream]; return 0; }
    if (n > size) n = size;
    memcpy(data, f->remote[stream] + f->remote_at[stream], n);
    f->remote_at[stream] += n;
    return (int)n;
}

static int rw(void *context, const unsigned char *data, size_t size)
{
    struct fixture *f = context;
    ++f->write_calls;
    if (f->fail == 3) return -1;
    if (f->write_again) { --f->write_again; return 0; }
    if (size > f->remote_limit) size = f->remote_limit;
    assert(f->sent_size + size < sizeof(f->sent));
    memcpy(f->sent + f->sent_size, data, size);
    f->sent_size += size;
    return (int)size;
}

static int lr(void *context, unsigned char *data, size_t size)
{
    struct fixture *f = context;
    size_t n = strlen(f->input) - f->input_at;
    ++f->input_calls;
    if (f->fail == 4) return -1;
    if (f->input_idle) return 0;
    if (!n) return SSH_PUMP_INPUT_EOF;
    if (n > size) n = size;
    memcpy(data, f->input + f->input_at, n);
    f->input_at += n;
    return (int)n;
}

static int lw(void *context, int stream, const unsigned char *data, size_t size)
{
    struct fixture *f = context;
    if (f->fail == 5) return -1;
    if (f->fail == 6) return 0;
    if (size > f->local_limit) size = f->local_limit;
    assert(f->output_size[stream] + size < sizeof(f->output[stream]));
    memcpy(f->output[stream] + f->output_size[stream], data, size);
    f->output_size[stream] += size;
    return (int)size;
}

static int eof(void *context)
{
    struct fixture *f = context;
    ++f->eof_calls;
    if (f->fail == 7) return -1;
    if (f->eof_retry) { --f->eof_retry; return 0; }
    /* An EOF emitted before an unwritten input tail is a protocol bug. */
    assert(f->sent_size == strlen(f->input));
    ++f->eof_accepted;
    return 1;
}

static int done(void *context)
{
    struct fixture *f = context;
    if (f->fail == 8) return -1;
    return f->remote_closed || (f->close_after_eof && f->eof_accepted);
}

static const struct ssh_pump_ops ops = {rr, rw, lr, lw, eof, done};

static void setup(struct fixture *f)
{
    memset(f, 0, sizeof(*f));
    f->input = "";
    f->remote[0] = "";
    f->remote[1] = "";
    f->remote_limit = 20000;
    f->local_limit = 20000;
}

static void finish(struct ssh_pump *p, struct fixture *f)
{
    int i, result = SSH_PUMP_ACTIVE;
    for (i = 0; i < 20000 && result == SSH_PUMP_ACTIVE; ++i)
        result = ssh_pump_step(p, &ops, f, NULL);
    assert(result == SSH_PUMP_DONE);
}

static void test_partial_writes_and_retry(void)
{
    struct ssh_pump p;
    struct fixture f;
    setup(&f);
    f.input = "input survives short writes";
    f.remote[0] = "standard output survives";
    f.remote[1] = "standard error survives";
    f.remote_limit = 2;
    f.local_limit = 3;
    f.write_again = 3;
    f.read_again[0] = 2;
    f.read_again[1] = 1;
    f.eof_retry = 2;
    f.close_after_eof = 1;
    ssh_pump_init(&p, 0);
    finish(&p, &f);
    assert(!strcmp(f.sent, "input survives short writes"));
    assert(!strcmp(f.output[0], "standard output survives"));
    assert(!strcmp(f.output[1], "standard error survives"));
    assert(f.eof_calls == 3 && f.eof_accepted == 1);
    assert(ssh_pump_step(&p, &ops, &f, NULL) == SSH_PUMP_DONE);
    assert(f.eof_calls == 3);
}

static void test_closed_peer_drains_and_never_waits_for_input(void)
{
    struct ssh_pump p;
    struct fixture f;
    char payload[SSH_PUMP_BUFFER_SIZE * 2 + 8];
    memset(payload, 'x', sizeof(payload) - 1);
    payload[sizeof(payload) - 1] = 0;
    setup(&f);
    f.remote_closed = 1;
    f.input_idle = 1;
    f.remote[0] = payload;
    f.remote[1] = "stderr";
    f.local_limit = 2;
    ssh_pump_init(&p, 0);
    assert(ssh_pump_step(&p, &ops, &f, NULL) == SSH_PUMP_ACTIVE);
    assert(f.output_size[0] > 0 && f.output_size[1] > 0);
    finish(&p, &f);
    assert(!strcmp(f.output[0], payload));
    assert(!strcmp(f.output[1], "stderr"));
    assert(f.input_calls == 0 && f.eof_calls == 0);
}

static void test_command_mode_and_idle_progress(void)
{
    struct ssh_pump p;
    struct fixture f;
    int progress = 99;
    setup(&f);
    ssh_pump_init(&p, 1);
    assert(ssh_pump_step(&p, &ops, &f, &progress) == SSH_PUMP_ACTIVE);
    assert(progress && f.eof_accepted == 1 && f.input_calls == 0);
    progress = 99;
    assert(ssh_pump_step(&p, &ops, &f, &progress) == SSH_PUMP_ACTIVE);
    assert(progress == 0 && f.eof_calls == 1 && f.input_calls == 0);
    f.remote_closed = 1;
    finish(&p, &f);
}

static void test_io_errors_are_explicit_and_sticky(void)
{
    int fail;
    for (fail = 1; fail <= 8; ++fail) {
        struct ssh_pump p;
        struct fixture f;
        setup(&f);
        f.fail = fail;
        if (fail == 3 || fail == 4) f.input = "input";
        if (fail == 5 || fail == 6) f.remote[0] = "output";
        ssh_pump_init(&p, 0);
        assert(ssh_pump_step(&p, &ops, &f, NULL) == SSH_PUMP_ERROR);
        f.fail = 0;
        assert(ssh_pump_step(&p, &ops, &f, NULL) == SSH_PUMP_ERROR);
    }
}

static void test_input_larger_than_buffer(void)
{
    struct ssh_pump p;
    struct fixture f;
    char payload[SSH_PUMP_BUFFER_SIZE * 2 + 8];
    memset(payload, 'a', sizeof(payload) - 1);
    payload[sizeof(payload) - 1] = 0;
    setup(&f);
    f.input = payload;
    f.remote_limit = 3;
    f.close_after_eof = 1;
    ssh_pump_init(&p, 0);
    finish(&p, &f);
    assert(!strcmp(f.sent, payload));
    assert(f.eof_accepted == 1);
}

static void test_remote_close_abandons_pending_input(void)
{
    struct ssh_pump p;
    struct fixture f;
    int input_calls;
    setup(&f);
    f.input = "never accepted";
    f.write_again = 1;
    ssh_pump_init(&p, 0);
    assert(ssh_pump_step(&p, &ops, &f, NULL) == SSH_PUMP_ACTIVE);
    input_calls = f.input_calls;
    f.remote_closed = 1;
    f.fail = 3;
    f.remote[1] = "final diagnostic";
    finish(&p, &f);
    assert(!strcmp(f.output[1], "final diagnostic"));
    assert(f.input_calls == input_calls);
    assert(f.sent_size == 0 && f.eof_calls == 0);
}

static void test_close_during_remote_read_skips_input_and_eof(void)
{
    int mode;
    for (mode = 0; mode < 3; ++mode) {
        struct ssh_pump p;
        struct fixture f;
        int input_calls, write_calls;
        setup(&f);
        ssh_pump_init(&p, mode == 2);
        if (mode == 1) {
            f.input = "pending input";
            f.write_again = 1;
            assert(ssh_pump_step(&p, &ops, &f, NULL) == SSH_PUMP_ACTIVE);
        }
        input_calls = f.input_calls;
        write_calls = f.write_calls;
        f.remote[0] = "final output";
        f.remote[1] = "final diagnostic";
        f.local_limit = 2;
        f.close_during_read = 1;
        f.fail = mode == 0 ? 4 : (mode == 1 ? 3 : 7);
        /* Network processing while reading observes CLOSE. No subsequent
         * local read, pending remote write, or EOF send is valid. */
        assert(ssh_pump_step(&p, &ops, &f, NULL) == SSH_PUMP_ACTIVE);
        assert(f.input_calls == input_calls && f.write_calls == write_calls);
        assert(f.eof_calls == 0);
        finish(&p, &f);
        assert(!strcmp(f.output[0], "final output"));
        assert(!strcmp(f.output[1], "final diagnostic"));
    }
}

int main(int argc, char **argv)
{
    static const struct {
        const char *name;
        void (*run)(void);
    } cases[] = {
        {"partial", test_partial_writes_and_retry},
        {"drain", test_closed_peer_drains_and_never_waits_for_input},
        {"command", test_command_mode_and_idle_progress},
        {"errors", test_io_errors_are_explicit_and_sticky},
        {"large", test_input_larger_than_buffer},
        {"close", test_remote_close_abandons_pending_input},
        {"close-during-read", test_close_during_remote_read_skips_input_and_eof}
    };
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        if (argc == 1 || !strcmp(argv[1], cases[i].name)) cases[i].run();
    puts("ssh pump tests passed");
    return 0;
}
