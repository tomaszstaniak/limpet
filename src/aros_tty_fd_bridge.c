/* Explicit link-time experiment: --wrap=tcgetattr --wrap=tcsetattr.
   Not a public libc ABI. Requires terminal-safety libc and serialized fd use.
   No concurrent close/replacement; the legacy fd-to-DOS helper does not pin. */
#include "aros_tty_termios.h"
#include "aros_tty_fd_bridge.h"
#include "tty_control.h"
#include <proto/dos.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

int __real_tcgetattr(int,struct termios *);
int __real_tcsetattr(int,int,const struct termios *);

/* 1 private PTY, 0 legacy/non-terminal, -1 failed private negotiation. */
int aros_tty_fd_resolve(int fd,BPTR *out)
{
    struct termios probe;
    union { long abi;BPTR handle; } storage;
    struct FileHandle *fh;XtyControl caps;SIPTR result;LONG error;
    /* The safety-fixed libc rejects directory locks before IsInteractive.
       Never call __get_default_file first and treat an arbitrary fd as a FH. */
    memset(&probe,0,sizeof probe);
    if(__real_tcgetattr(fd,&probe)) return 0;
    memset(&storage,0,sizeof storage);
    if(__get_default_file(fd,&storage.abi) || !storage.handle) { errno=EBADF;return -1; }
    fh=BADDR(storage.handle);
    if(!fh->fh_Type) { errno=ENOTTY;return -1; }
    memset(&caps,0,sizeof caps);caps.version=XTY_CONTROL_VERSION;caps.operation=XTY_CAPS;
    result=DoPkt(fh->fh_Type,XTY_ACTION_CONTROL,fh->fh_Arg1,(SIPTR)&caps,
                 sizeof caps,XTY_PACKET_MAGIC,0);
    error=IoErr();
    if(result==DOSFALSE && error==ERROR_ACTION_NOT_KNOWN) return 0;
    if(result!=DOSTRUE || error || caps.version!=XTY_CONTROL_VERSION ||
       caps.operation!=XTY_CAPS || caps.reserved[0]!=XTY_PACKET_MAGIC || caps.reserved[1]) {
        errno=error==ERROR_BREAK?EINTR:error==ERROR_INVALID_LOCK?EBADF:EIO;return -1;
    }
    *out=storage.handle;return 1;
}
int __wrap_tcgetattr(int fd,struct termios *out)
{
    BPTR handle;int kind;
    if(!out) return __real_tcgetattr(fd,out);
    kind=aros_tty_fd_resolve(fd,&handle);
    if(kind<0) return -1;
    return kind?aros_tty_tcgetattr(handle,out):__real_tcgetattr(fd,out);
}
int __wrap_tcsetattr(int fd,int action,const struct termios *settings)
{
    BPTR handle;int kind;
    if(!settings || (action!=TCSANOW && action!=TCSADRAIN && action!=TCSAFLUSH))
        return __real_tcsetattr(fd,action,settings);
    kind=aros_tty_fd_resolve(fd,&handle);
    if(kind<0) return -1;
    return kind?aros_tty_tcsetattr(handle,action,settings):__real_tcsetattr(fd,action,settings);
}
