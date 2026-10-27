/* The DOS side of the SDK: the only file that sends packets. */
#include <limpet/limpet.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <string.h>

LONG limpet_control(BPTR handle, XtyControl *control)
{
    struct FileHandle *fh;
    XtyControl copy;
    SIPTR result, error;

    if (!handle || !control) { SetIoErr(ERROR_BAD_NUMBER); return DOSFALSE; }
    fh = BADDR(handle);
    /* NIL: and closed handles have no handler port to ask. */
    if (!fh->fh_Type) { SetIoErr(ERROR_INVALID_LOCK); return DOSFALSE; }
    /* Work on a copy: the caller's block must stay untouched on failure,
       and a handler that does not know the action may scribble on it. */
    copy = *control;
    copy.reserved[0] = 0;
    result = DoPkt(fh->fh_Type, XTY_ACTION_CONTROL, fh->fh_Arg1, (SIPTR)&copy,
                   sizeof copy, XTY_PACKET_MAGIC, 0);
    error = IoErr();
    /* Validate in packet width before narrowing: on x86_64 0xffffffff is
       not DOSTRUE, and a success that carries an error code is malformed. */
    if (error < 0 || error > INT32_MAX ||
        (result != DOSFALSE && result != DOSTRUE) || (result == DOSTRUE && error)) {
        SetIoErr(ERROR_BAD_NUMBER);
        return DOSFALSE;
    }
    if (result != DOSTRUE) {
        /* Some handlers refuse an unknown action without an error code. */
        if (!error) SetIoErr(ERROR_ACTION_NOT_KNOWN);
        return DOSFALSE;
    }
    /* reserved[0] is zero on the way out and the magic on the way back:
       a handler that merely returned the block unchanged did not answer. */
    if (copy.version != control->version || copy.operation != control->operation ||
        copy.reserved[0] != XTY_PACKET_MAGIC || copy.reserved[1]) {
        SetIoErr(ERROR_ACTION_NOT_KNOWN);
        return DOSFALSE;
    }
    *control = copy;
    SetIoErr(0);
    return DOSTRUE;
}

LimpetStatus limpet_query_presentation(BPTR output, LimpetPresentation *out,
                                       LONG *ioerr)
{
    XtyControl request;
    LONG error;
    LimpetStatus status;

    if (ioerr) *ioerr = 0;
    if (!out) return LIMPET_BAD_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!output) { if (ioerr) *ioerr = ERROR_INVALID_LOCK; return LIMPET_BAD_ARGUMENT; }
    /* Files and pipes never get the packet: a filesystem has no reason to
       know the action, and the answer would not matter anyway. */
    if (!IsInteractive(output)) return LIMPET_NOT_INTERACTIVE;
    limpet_presentation_request(&request);
    if (limpet_control(output, &request) != DOSTRUE) {
        error = (LONG)IoErr();
        if (ioerr) *ioerr = error;
        return error == ERROR_ACTION_NOT_KNOWN ? LIMPET_UNSUPPORTED :
               error == ERROR_BAD_NUMBER ? LIMPET_INVALID_REPLY : LIMPET_FAILED;
    }
    status = limpet_presentation_decode(&request, out);
    if (status != LIMPET_OK && ioerr) *ioerr = ERROR_BAD_NUMBER;
    return status;
}

LimpetStatus limpet_query_identity(BPTR output, LimpetIdentity *out, LONG *ioerr)
{
    XtyControl request;
    LONG error;
    LimpetStatus status;

    if (ioerr) *ioerr = 0;
    if (!out) return LIMPET_BAD_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!output) { if (ioerr) *ioerr = ERROR_INVALID_LOCK; return LIMPET_BAD_ARGUMENT; }
    if (!IsInteractive(output)) return LIMPET_NOT_INTERACTIVE;
    limpet_identity_request(&request);
    if (limpet_control(output, &request) != DOSTRUE) {
        error = (LONG)IoErr();
        if (ioerr) *ioerr = error;
        return error == ERROR_ACTION_NOT_KNOWN ? LIMPET_UNSUPPORTED :
               error == ERROR_BAD_NUMBER ? LIMPET_INVALID_REPLY : LIMPET_FAILED;
    }
    status = limpet_identity_decode(&request, out);
    if (status != LIMPET_OK && ioerr) *ioerr = ERROR_BAD_NUMBER;
    return status;
}

/* dos/consoleinfo.h (PR #1763) is not in every SDK; the wire layout is
   fixed, so it is spelled out here. */
#define LIMPET_ACTION_CONSOLE_GETGEOMETRY 0x434f4e47L
#define LIMPET_CONSOLE_GEOMETRY_VERSION 1u

LimpetStatus limpet_query_geometry(BPTR output, LimpetGeometry *out, LONG *ioerr)
{
    struct { uint32_t size, version, columns, rows, width, height; } reply;
    struct FileHandle *fh;
    SIPTR result, error;

    if (ioerr) *ioerr = 0;
    if (!out) return LIMPET_BAD_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!output) { if (ioerr) *ioerr = ERROR_INVALID_LOCK; return LIMPET_BAD_ARGUMENT; }
    if (!IsInteractive(output)) return LIMPET_NOT_INTERACTIVE;
    fh = BADDR(output);
    if (!fh->fh_Type) { if (ioerr) *ioerr = ERROR_INVALID_LOCK; return LIMPET_FAILED; }
    memset(&reply, 0, sizeof reply);
    /* Arg1 the handle's fh_Arg1, Arg2 the block, Arg3 its capacity,
       Arg4 the version asked for. */
    result = DoPkt(fh->fh_Type, LIMPET_ACTION_CONSOLE_GETGEOMETRY, fh->fh_Arg1,
                   (SIPTR)&reply, sizeof reply, LIMPET_CONSOLE_GEOMETRY_VERSION, 0);
    error = IoErr();
    if (result != DOSTRUE) {
        /* Only a refusal of the action itself means "not supported";
           a handler that refuses without a code is taken the same way. */
        if (result == DOSFALSE && (error == ERROR_ACTION_NOT_KNOWN || !error)) {
            if (ioerr) *ioerr = ERROR_ACTION_NOT_KNOWN;
            return LIMPET_UNSUPPORTED;
        }
        if (ioerr) *ioerr = result == DOSFALSE && error > 0 && error <= INT32_MAX ? (LONG)error : ERROR_BAD_NUMBER;
        return result == DOSFALSE ? LIMPET_FAILED : LIMPET_INVALID_REPLY;
    }
    if (error || reply.size != sizeof reply || reply.version != LIMPET_CONSOLE_GEOMETRY_VERSION ||
        !reply.columns || !reply.rows) {
        if (ioerr) *ioerr = ERROR_BAD_NUMBER;
        return LIMPET_INVALID_REPLY;
    }
    out->columns = reply.columns; out->rows = reply.rows;
    out->width = reply.width; out->height = reply.height;
    return LIMPET_OK;
}

int limpet_write_dos(void *context, const char *bytes, size_t length)
{
    BPTR handle = (BPTR)context;
    /* Write() may accept fewer bytes than asked; resending the whole
       buffer would repeat the prefix that did arrive. */
    while (length) {
        LONG chunk = length > 0x7fffffff ? 0x7fffffff : (LONG)length;
        LONG n = Write(handle, (APTR)bytes, chunk);
        if (n <= 0) return 1;
        bytes += n;
        length -= (size_t)n;
    }
    return 0;
}
