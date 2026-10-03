#ifndef XTERM_SSH_AROS_PLATFORM_H
#define XTERM_SSH_AROS_PLATFORM_H
#define __CONFIG_ROADSHOW__ 1
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
/* Native networking uses library vectors, not Unix link symbols. Include the
 * system types before libssh's headers (net/if.h is not self-contained). */
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <unistd.h>
#include <proto/socket.h>
/* The SDK declares Roadshow vector 101 but omits its public call macro. */
#ifndef inet_pton
#define inet_pton(af,src,dst) AROS_LC3(LONG, RS_inet_pton, \
    AROS_LCA(LONG,(af),D0), AROS_LCA(const char *,(src),A0), \
    AROS_LCA(void *,(dst),A1), struct Library *, SocketBase,101,BSDSocket)
#endif
unsigned int xt_ssh_if_nametoindex(const char *name);
char *xt_ssh_home_dir(void);
#define if_nametoindex xt_ssh_if_nametoindex
/* proto/socket.h supplies select -> WaitSelect. Only sockets reach libssh's
 * poll fallback; the unrelated POSIX select implementation must not be used. */
#endif
