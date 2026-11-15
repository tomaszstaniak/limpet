#include "scp_spec.h"
#include <string.h>

static int put(char *out, size_t size, const char *text, size_t length)
{
    if (length >= size) return -1;
    memcpy(out, text, length);
    out[length] = 0;
    return 0;
}

int xt_scp_split(const char *argument, int (*is_device)(const char *name, size_t length),
                 struct xt_scp_spec *spec)
{
    const char *colon = strchr(argument, ':');
    const char *at, *path;
    size_t before;

    memset(spec, 0, sizeof *spec);
    if (!colon) return put(spec->path, sizeof spec->path, argument, strlen(argument));
    before = (size_t)(colon - argument);
    at = memchr(argument, '@', before);
    if (!at && (before == 0 || memchr(argument, '/', before) || (is_device && is_device(argument, before))))
        return put(spec->path, sizeof spec->path, argument, strlen(argument));

    spec->remote = 1;
    if (at)
    {
        /* The last '@' before the colon: a user name may hold one. */
        const char *next;
        while ((next = memchr(at + 1, '@', (size_t)(colon - at - 1)))) at = next;
        if (put(spec->user, sizeof spec->user, argument, (size_t)(at - argument))) return -1;
        ++at;
    }
    else at = argument;
    if (colon == at || put(spec->host, sizeof spec->host, at, (size_t)(colon - at))) return -1;
    path = colon + 1;
    if (path[0] == '~' && (path[1] == '/' || !path[1])) path += path[1] ? 2 : 1;
    if (!*path) path = ".";
    return put(spec->path, sizeof spec->path, path, strlen(path));
}

const char *xt_scp_remote_name(const char *path, size_t *length)
{
    size_t end = strlen(path), start;
    while (end && path[end - 1] == '/') --end;
    start = end;
    while (start && path[start - 1] != '/') --start;
    *length = end - start;
    if ((*length == 1 && path[start] == '.') || (*length == 2 && path[start] == '.' && path[start + 1] == '.'))
        *length = 0;
    return path + start;
}

int xt_scp_remote_join(char *out, size_t size, const char *directory, const char *name)
{
    size_t d = strlen(directory), n = strlen(name);
    int slash = d && directory[d - 1] != '/';
    if (d + (size_t)slash + n >= size) return -1;
    memcpy(out, directory, d);
    if (slash) out[d++] = '/';
    memcpy(out + d, name, n + 1);
    return 0;
}

int xt_scp_local_name(char *out, size_t size, const char *name, size_t length)
{
    size_t i;
    if (!length || length >= size) return -1;
    if (name[0] == '.' && (length == 1 || (length == 2 && name[1] == '.'))) return -1;
    for (i = 0; i < length; ++i)
    {
        if (name[i] == '/' || !name[i]) return -1;
        out[i] = name[i] == ':' ? '_' : name[i];
    }
    out[length] = 0;
    return 0;
}
