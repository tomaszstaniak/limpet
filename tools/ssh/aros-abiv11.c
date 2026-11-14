/* ABIv11 only: what libssh expects of the C library and ABIv11 does not
 * have. IPv4 name lookup goes through bsdsocket.library (getaddrinfo() and
 * its companions are miami.library calls there). On mainline the C library
 * provides all of it and this file is empty. Remove it with ABIv11. */
#ifdef XT_SSH_ABIV11
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pwd.h>
#include <errno.h>
#include <proto/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { XT_EAI_NONAME = 8, XT_EAI_MEMORY = 6, XT_EAI_FAMILY = 5 };

int xt_ssh_inet_pton(int family, const char *text, void *address)
{
    unsigned parts[4], count = 0, value = 0, digits = 0;
    const char *p;
    if (family != AF_INET) return 0; /* IPv6 text is not an address here */
    for (p = text;; ++p) {
        if (*p >= '0' && *p <= '9') {
            value = value * 10 + (unsigned)(*p - '0');
            if (++digits > 3 || value > 255) return 0;
        } else if (*p == '.' || *p == 0) {
            if (!digits || count == 4) return 0;
            parts[count++] = value; value = 0; digits = 0;
            if (*p == 0) break;
        } else return 0;
    }
    if (count != 4) return 0;
    ((unsigned char *)address)[0] = (unsigned char)parts[0];
    ((unsigned char *)address)[1] = (unsigned char)parts[1];
    ((unsigned char *)address)[2] = (unsigned char)parts[2];
    ((unsigned char *)address)[3] = (unsigned char)parts[3];
    return 1;
}

void xt_ssh_freeaddrinfo(struct addrinfo *list)
{
    while (list) {
        struct addrinfo *next = list->ai_next;
        free(list);
        list = next;
    }
}

static struct addrinfo *entry(const unsigned char *ip, unsigned port, const struct addrinfo *hints)
{
    struct addrinfo *ai = calloc(1, sizeof *ai + sizeof(struct sockaddr_in));
    struct sockaddr_in *sin;
    if (!ai) return NULL;
    sin = (struct sockaddr_in *)(ai + 1);
    sin->sin_len = sizeof *sin;
    sin->sin_family = AF_INET;
    sin->sin_port = htons((unsigned short)port);
    memcpy(&sin->sin_addr, ip, 4);
    ai->ai_family = AF_INET;
    ai->ai_socktype = hints && hints->ai_socktype ? hints->ai_socktype : SOCK_STREAM;
    ai->ai_protocol = hints ? hints->ai_protocol : 0;
    ai->ai_addrlen = sizeof *sin;
    ai->ai_addr = (struct sockaddr *)sin;
    return ai;
}

int xt_ssh_getaddrinfo(const char *host, const char *service,
                       const struct addrinfo *hints, struct addrinfo **result)
{
    struct addrinfo *first = NULL, **tail = &first;
    unsigned char ip[4];
    unsigned port = 0;
    *result = NULL;
    if (hints && hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET) return XT_EAI_FAMILY;
    if (!host) return XT_EAI_NONAME;
    if (service) {
        const char *p;
        for (p = service; *p; ++p) {
            if (*p < '0' || *p > '9' || (port = port * 10 + (unsigned)(*p - '0')) > 65535) return XT_EAI_NONAME;
        }
    }
    if (xt_ssh_inet_pton(AF_INET, host, ip)) {
        if (!(first = entry(ip, port, hints))) return XT_EAI_MEMORY;
    } else {
        struct hostent *he = gethostbyname((char *)host);
        char **address;
        if (!he || he->h_addrtype != AF_INET || he->h_length != 4) return XT_EAI_NONAME;
        for (address = he->h_addr_list; address && *address; ++address) {
            if (!(*tail = entry((const unsigned char *)*address, port, hints))) {
                xt_ssh_freeaddrinfo(first);
                return XT_EAI_MEMORY;
            }
            tail = &(*tail)->ai_next;
        }
        if (!first) return XT_EAI_NONAME;
    }
    *result = first;
    return 0;
}

const char *xt_ssh_gai_strerror(int code)
{
    switch (code) {
    case 0: return "no error";
    case XT_EAI_NONAME: return "host name not found";
    case XT_EAI_MEMORY: return "out of memory";
    case XT_EAI_FAMILY: return "address family not supported";
    default: return "name lookup failed";
    }
}

/* Numeric form only; nothing here looks a name up from an address. */
int xt_ssh_getnameinfo(const struct sockaddr *address, socklen_t length, char *host,
                       size_t host_size, char *service, size_t service_size, int flags)
{
    const struct sockaddr_in *sin = (const struct sockaddr_in *)address;
    const unsigned char *ip;
    (void)flags;
    if (!address || length < sizeof *sin || sin->sin_family != AF_INET) return XT_EAI_FAMILY;
    ip = (const unsigned char *)&sin->sin_addr;
    if (host && host_size) snprintf(host, host_size, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    if (service && service_size) snprintf(service, service_size, "%u", (unsigned)ntohs(sin->sin_port));
    return 0;
}

/* No account database: the caller falls back to its own default. */
int xt_ssh_getpwuid_r(uid_t uid, struct passwd *entry, char *buffer, size_t size, struct passwd **result)
{
    (void)uid; (void)entry; (void)buffer; (void)size;
    *result = NULL;
    return ENOENT;
}

int xt_ssh_strerror_r(int code, char *buffer, size_t size)
{
    const char *text = strerror(code);
    if (!size) return ERANGE;
    if (!text) text = "unknown error";
    if (strlen(text) >= size) { buffer[0] = 0; return ERANGE; }
    strcpy(buffer, text);
    return 0;
}
#else
typedef int xt_ssh_abiv11_unused;
#endif
