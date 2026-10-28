#include "../src/script_tail.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char file[] = "Prompt \"%N> \"\nPath C: ADD\n";
static const char tail[] = "\nfastfetch -c aros-compact\n";
/* What the Shell would read, in pieces of n bytes, with a file of length
   claimed (the file has strlen(file) bytes). */
static void read_all(long n, long claimed, char *out)
{
    XScriptStream s; char piece[64]; long k, used = 0, pos;
    script_stream_init(&s, claimed, (long)strlen(tail));
    for (;;) {
        long f = script_stream_file_part(&s, n);
        if (f) {
            pos = s.position;
            k = pos >= (long)strlen(file) ? 0 : (long)strlen(file) - pos < f ? (long)strlen(file) - pos : f;
            memcpy(piece, file + pos, (size_t)k);
            script_stream_file_read(&s, k);
            if (!k) continue;
        } else if (!(k = script_stream_tail_read(&s, tail, piece, n))) break;
        memcpy(out + used, piece, (size_t)k); used += k;
    }
    out[used] = 0;
}
int main(void)
{
    char all[256], want[256];
    XScriptStream s;
    snprintf(want, sizeof want, "%s%s", file, tail);
    read_all(1, (long)strlen(file), all); assert(!strcmp(all, want));
    read_all(7, (long)strlen(file), all); assert(!strcmp(all, want));
    read_all(64, (long)strlen(file), all); assert(!strcmp(all, want));
    /* A file that ends before its size said: the tail follows what was there. */
    read_all(5, (long)strlen(file) + 10, all); assert(!strcmp(all, want));
    /* No startup file: the tail alone. */
    script_stream_init(&s, 0, (long)strlen(tail));
    assert(!script_stream_file_part(&s, 10) && script_stream_tail_read(&s, tail, all, 100) == (long)strlen(tail));
    assert(!script_stream_tail_read(&s, tail, all, 100));
    /* Seek: old position back, whole-stream positions, refusals change nothing. */
    script_stream_init(&s, 10, 5);
    assert(script_stream_seek(&s, 12, -1) == 0 && s.position == 12);
    assert(script_stream_seek(&s, 0, 0) == 12);
    assert(script_stream_tail_read(&s, "ABCDE", all, 10) == 3 && !memcmp(all, "CDE", 3));
    assert(script_stream_seek(&s, -15, 1) == 15 && s.position == 0 && script_stream_file_part(&s, 99) == 10);
    assert(script_stream_seek(&s, 16, -1) == -1 && s.position == 0);
    assert(script_stream_seek(&s, -1, 0) == -1 && s.position == 0);
    assert(script_stream_seek(&s, 0, 2) == -1);
    puts("script tail PASS: file then tail in any piece size, short file, tail alone, seek over both");
    return 0;
}
