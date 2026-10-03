/* Force-included boundary doubles for compiling the real fcntl.c on host. */
#define ___FDESC_H
#define LIBC_TERMINAL_SHIM_H
#include <stdint.h>
#include <stddef.h>
#include <fcntl.h>
typedef void *APTR;
typedef intptr_t BPTR;
typedef intptr_t IPTR;
typedef long LONG;
#define FIONBIO 1
#define CHANGE_FH 1
#define DOSTRUE (-1)
typedef struct { BPTR handle; int flags; } fcb;
typedef struct { fcb *fcb; int fdflags; } fdesc;
struct fd_hooks { LONG (*fdh_ioctl)(APTR,IPTR,APTR,LONG *); };
fdesc *__getfdesc(int);
const struct fd_hooks *__getfdhooks(int,APTR *);
int __getfirstfd(int);
int __oflags2amode(int);
int ChangeMode(int,BPTR,int);
LONG IoErr(void);
int __stdc_ioerr2errno(LONG);
void __fcb_lock(fcb *);
void __fcb_unlock(fcb *);
