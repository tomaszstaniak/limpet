#include "../examples/ssh/sgr_ansi.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void expect(const char *input, const char *wanted)
{
    size_t size = strlen(input), split;
    /* Whole, then cut at every position: the result must not depend on
     * where the network delivered the bytes. */
    for (split = 0; split <= size; ++split) {
        struct xt_sgr_filter f;
        unsigned char out[4096];
        size_t n;
        xt_sgr_filter_init(&f);
        n = xt_sgr_filter(&f, (const unsigned char *)input, split, out);
        n += xt_sgr_filter(&f, (const unsigned char *)input + split, size - split, out + n);
        if (n != strlen(wanted) || memcmp(out, wanted, n)) {
            fprintf(stderr, "split %zu of \"%s\": got %zu bytes\n", split, input + 1, n);
            assert(0);
        }
    }
}

int main(void)
{
    char longer[300], wanted[300];
    expect("plain text\r\n", "plain text\r\n");
    expect("\033[31mred\033[0m", "\033[38;5;1mred\033[0m");
    expect("\033[1;32mx", "\033[1;38;5;2mx");
    expect("\033[44;37m", "\033[48;5;4;38;5;7m");
    expect("\033[m\033[39;49m", "\033[m\033[39;49m");
    expect("\033[;31m", "\033[;38;5;1m");
    expect("\033[31;m", "\033[38;5;1;m");
    /* Arguments of the extended forms are not colours of their own. */
    expect("\033[38;5;31m", "\033[38;5;31m");
    expect("\033[48;2;30;40;37m", "\033[48;2;30;40;37m");
    expect("\033[38;5;31;41m", "\033[38;5;31;48;5;1m");
    expect("\033[38;2;1;2;3;32m", "\033[38;2;1;2;3;38;5;2m");
    /* Bright colours are not pens. */
    expect("\033[91;104m", "\033[91;104m");
    /* Other sequences, private parameters and colon forms pass unchanged. */
    expect("\033[31H\033[2J\033[?1034h", "\033[31H\033[2J\033[?1034h");
    expect("\033[38:5:31m\033[>31m", "\033[38:5:31m\033[>31m");
    expect("\033]0;title 31m\007\033[31m", "\033]0;title 31m\007\033[38;5;1m");
    expect("\033\033[31m\033(B", "\033\033[38;5;1m\033(B");
    /* Longer than the filter holds: left as it came. */
    memset(longer, '1', sizeof longer);
    longer[0] = '\033'; longer[1] = '['; longer[2] = '3';
    longer[200] = 'm'; longer[201] = 0;
    strcpy(wanted, longer);
    expect(longer, wanted);
    puts("ssh sgr: ok");
    return 0;
}
