#ifndef AROS_XTERM_CLIPBOARD_H
#define AROS_XTERM_CLIPBOARD_H
#include <stddef.h>
#define AROS_CLIP_LIMIT 65536u
/* Synchronous diagnostic adapter. Reads drain/release the device transaction.
 * Return 0 on success, -1 on device/I/O failure, -2 on capacity overflow. */
int aros_clip_read(unsigned unit,unsigned char *bytes,size_t cap,size_t *length);
int aros_clip_write(unsigned unit,const unsigned char *bytes,size_t length);
#endif
