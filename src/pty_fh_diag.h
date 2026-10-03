/* Diagnostic only: append the identity and DOS buffer state of a file handle
   to a RAM: log. Used by the XTERM_PTY_DIAG frontend build and by
   tests/pty_system_run.c to trace where PTY output stops, without relying on
   that output. The log is written with plain DOS calls to its own file.

   The SDK's struct FileHandle is FileHandle32 (__DOS64 is not defined). The
   fh_Flags bits are dos.library's private ones (rom/dos/dos_fhflags.h):
   0x80000000 WRITE, 0x1 BUF, 0x2 APPEND, 0x4 LINEBUF, 0x8 NOBUF, 0x10 OWNBUF,
   0x20 FLUSHING. Values are recorded raw; nothing here interprets them. */
#ifndef PTY_FH_DIAG_H
#define PTY_FH_DIAG_H
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

static void pty_fh_diag_write(const char *path, const char *text)
{
    BPTR file = Open((CONST_STRPTR)path, MODE_READWRITE);
    if (!file) return;
    Seek(file, 0, OFFSET_END);
    Write(file, (APTR)text, (LONG)strlen(text));
    Close(file);
}

static void pty_fh_diag(const char *path, const char *tag, BPTR handle)
{
    char line[320];
    struct FileHandle *fh = (struct FileHandle *)BADDR(handle);
    if (!fh)
        snprintf(line, sizeof line, "%s handle=NULL\n", tag);
    else
        snprintf(line, sizeof line,
                 "%s handle=%p flags=0x%08lx interactive=%ld type=%p arg1=%p buf=%p "
                 "pos=%ld end=%ld bufsize=%lu\n",
                 tag, (void *)fh, (unsigned long)fh->fh_Flags, (long)fh->fh_Interactive,
                 (void *)fh->fh_Type, (void *)fh->fh_Arg1, (void *)BADDR(fh->fh_Buf),
                 (long)fh->fh_Pos, (long)fh->fh_End, (unsigned long)fh->fh_BufSize);
    pty_fh_diag_write(path, line);
}
#endif
