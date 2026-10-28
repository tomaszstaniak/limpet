#ifndef XTERM_SCRIPT_TAIL_H
#define XTERM_SCRIPT_TAIL_H
/* The script a Shell session starts with, as one stream: the startup file
 * (S:Shell-Startup) followed by a few bytes held in memory (the line set in
 * Preferences). The Shell reads and seeks it like a file; it is never typed
 * into the console. Positions are those of the whole stream. This model
 * makes no DOS calls: the caller reads and seeks the file where told. */
typedef struct {
    long file_length;   /* bytes of the file; lowered if it ends early */
    long tail_length;
    long position;
} XScriptStream;
void script_stream_init(XScriptStream *, long file_length, long tail_length);
/* Bytes to read from the file now, at position; 0: read the tail instead. */
long script_stream_file_part(const XScriptStream *, long want);
/* After reading the file: got bytes (0: the file ended before its length). */
void script_stream_file_read(XScriptStream *, long got);
/* Copies from the tail at position; returns the bytes copied (0: the end). */
long script_stream_tail_read(XScriptStream *, const char *tail, char *buffer, long want);
/* AmigaDOS Seek (mode -1 beginning, 0 current, 1 end). Returns the old
 * position, or -1 for a position outside the stream (nothing changes).
 * When the new position is inside the file the caller seeks the file there. */
long script_stream_seek(XScriptStream *, long offset, long mode);
#endif
