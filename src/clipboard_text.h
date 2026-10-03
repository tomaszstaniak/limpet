#ifndef XTERM_CLIPBOARD_TEXT_H
#define XTERM_CLIPBOARD_TEXT_H
#include <stddef.h>

#define CLIP_RAW_LIMIT 65536u
#define CLIP_TEXT_LIMIT 65536u
enum ClipStatus {
    CLIP_OK, CLIP_ERR_ARGUMENT, CLIP_ERR_FORMAT, CLIP_ERR_UTF8, CLIP_ERR_NUL,
    CLIP_ERR_LIMIT, CLIP_ERR_CAPACITY, CLIP_ERR_CHARSET, CLIP_ERR_MEMORY
};
enum ClipFormat {
    CLIP_FORMAT_EMPTY, CLIP_FORMAT_UTF8, CLIP_FORMAT_LATIN1,
    CLIP_FORMAT_CSET_UTF8, CLIP_FORMAT_MIXED
};
/* No trailing NUL is written. Embedded NUL is rejected; other controls and
 * newlines are preserved (paste policy belongs to the caller). Buffers must
 * not overlap. written and lossy/format are required and reset to zero on
 * error; output remains untouched on any failure. NULL input is valid only
 * for zero input length; NULL output only when capacity is zero.
 * Both input text and complete FORM output are bounded, so the duplicated
 * UTF8 + CHRS representation can reach the raw limit before the text limit.
 * lossy counts scalars replaced by '?' in the Latin-1 CHRS fallback only. */
int clip_encode_utf8(const unsigned char *text, size_t length,
                     unsigned char *out, size_t capacity, size_t *written,
                     unsigned *lossy);
/* Exactly one FORM FTXT, exact FORM length, complete even-byte chunk padding.
 * Concatenate all UTF8 chunks if any are present, otherwise CHRS chunks.
 * UTF8 may be split across chunks. Default CHRS charset is Latin-1; CSET's
 * big-endian first ULONG supports 0 (legacy Latin-1), IANA 4 (Latin-1),
 * and 106 (UTF-8), applying
 * to subsequent CHRS chunks. Unknown CSET is rejected when decoding CHRS;
 * it does not affect independent UTF8 chunks. Unknown chunks are skipped.
 * Empty FTXT is valid empty text; non-FTXT/raw-empty data is a format error.
 * Decoding uses at most CLIP_TEXT_LIMIT temporary heap bytes. */
int clip_decode_text(const unsigned char *raw, size_t length,
                     unsigned char *out, size_t capacity, size_t *written,
                     unsigned *format);
#endif
