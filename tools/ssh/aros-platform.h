#ifndef XTERM_SSH_AROS_PLATFORM_H
#define XTERM_SSH_AROS_PLATFORM_H
#define __CONFIG_ROADSHOW__ 1
/* Mbed TLS comes from the aros-crypto kit, which has one configuration. */
#define MBEDTLS_CONFIG_FILE <aros_crypto_config.h>
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
#ifdef XT_SSH_ABIV11
/* Its C library does not know socket descriptors (mainline joins the two
 * through fd.library): libssh closes and switches sockets with bsdsocket
 * calls (tools/ssh/libssh-aros.patch). */
#define XT_SSH_BSDSOCKET_DIRECT 1
#include <sys/ioctl.h>
/* select() for sockets is WaitSelect(); mainline's SDK header says so itself. */
#undef select
#define select(count, readable, writable, exceptional, timeout) \
    WaitSelect((count), (readable), (writable), (exceptional), (timeout), NULL)
/* ABIv11: name lookup functions are miami.library calls and its bsdsocket
 * has no inet_pton vector. The client resolves IPv4 names through bsdsocket
 * (tools/ssh/aros-interface.c). */
#undef inet_pton
#define AI_NUMERICHOST 0
int xt_ssh_inet_pton(int family, const char *text, void *address);
int xt_ssh_getaddrinfo(const char *host, const char *service,
                       const struct addrinfo *hints, struct addrinfo **result);
void xt_ssh_freeaddrinfo(struct addrinfo *list);
const char *xt_ssh_gai_strerror(int code);
int xt_ssh_getnameinfo(const struct sockaddr *address, socklen_t length, char *host,
                       size_t host_size, char *service, size_t service_size, int flags);
#define inet_pton xt_ssh_inet_pton
#define getaddrinfo xt_ssh_getaddrinfo
#define freeaddrinfo xt_ssh_freeaddrinfo
#define gai_strerror xt_ssh_gai_strerror
#define getnameinfo xt_ssh_getnameinfo
/* Its C library has neither a reentrant account lookup nor strerror_r(). */
#include <pwd.h>
#include <string.h>
int xt_ssh_getpwuid_r(uid_t uid, struct passwd *entry, char *buffer, size_t size, struct passwd **result);
int xt_ssh_strerror_r(int code, char *buffer, size_t size);
#define getpwuid_r xt_ssh_getpwuid_r
#define strerror_r xt_ssh_strerror_r
#endif
unsigned int xt_ssh_if_nametoindex(const char *name);
/* Asked by libssh's wait for the network every quarter of a second, with
 * the time waited so far: nonzero ends the wait with an error (Ctrl-C). */
int xt_ssh_break_wanted(int waited_ms);
char *xt_ssh_home_dir(void);
#define if_nametoindex xt_ssh_if_nametoindex
/* proto/socket.h supplies select -> WaitSelect. Only sockets reach libssh's
 * poll fallback; the unrelated POSIX select implementation must not be used. */
#endif
