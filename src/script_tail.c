#include "script_tail.h"
#include <string.h>

void script_stream_init(XScriptStream *s, long file_length, long tail_length)
{
    s->file_length = file_length > 0 ? file_length : 0;
    s->tail_length = tail_length > 0 ? tail_length : 0;
    s->position = 0;
}

long script_stream_file_part(const XScriptStream *s, long want)
{
    long left = s->file_length - s->position;
    if (want <= 0 || left <= 0) return 0;
    return want < left ? want : left;
}

void script_stream_file_read(XScriptStream *s, long got)
{
    if (got > 0) s->position += got;
    else if (s->position < s->file_length) s->file_length = s->position;
}

long script_stream_tail_read(XScriptStream *s, const char *tail, char *buffer, long want)
{
    long at = s->position - s->file_length, n;
    if (want <= 0 || at < 0 || at >= s->tail_length) return 0;
    n = s->tail_length - at;
    if (n > want) n = want;
    memcpy(buffer, tail + at, (size_t)n);
    s->position += n;
    return n;
}

long script_stream_seek(XScriptStream *s, long offset, long mode)
{
    long base = mode < 0 ? 0 : mode == 0 ? s->position : s->file_length + s->tail_length;
    long target = base + offset, old = s->position;
    if (mode < -1 || mode > 1 || target < 0 || target > s->file_length + s->tail_length) return -1;
    s->position = target;
    return old;
}
