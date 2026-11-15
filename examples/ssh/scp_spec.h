#ifndef XTERM_SSH_SCP_SPEC_H
#define XTERM_SSH_SCP_SPEC_H
#include <stddef.h>
/* One scp argument: a local AROS path, or [user@]host:path.
 *
 * AROS paths have a colon too (Work:file). The text before the first colon
 * names a remote host when it holds an '@'; otherwise it is a host unless it
 * is empty, holds a '/', or is_device() knows it as a volume, device or
 * assign. A remote path may be empty or start with "~/": both mean the
 * home directory there. */
struct xt_scp_spec {
    int remote;
    char user[128]; /* "" when not given */
    char host[256];
    char path[1024];
};
/* 0, or -1 when a part does not fit. */
int xt_scp_split(const char *argument, int (*is_device)(const char *name, size_t length),
                 struct xt_scp_spec *spec);
/* The last part of a remote path, without trailing slashes; "" for "/", "."
 * and "..". Returns a pointer into path and the length. */
const char *xt_scp_remote_name(const char *path, size_t *length);
/* Joins a remote directory and a name. 0, or -1 when it does not fit. */
int xt_scp_remote_join(char *out, size_t size, const char *directory, const char *name);
/* A name from a server as a local file name: 0 when it is usable as it is or
 * after ':' became '_'; -1 for "", ".", ".." and names with '/'. */
int xt_scp_local_name(char *out, size_t size, const char *name, size_t length);
/* The names already given out in one target directory. claim() returns 0
 * for a new name, 1 when the name is taken (with fold: also when it differs
 * only in the case of letters, as on AROS file systems) and -1 without
 * memory. */
struct xt_scp_names {
    char **items;
    size_t count, room;
};
int xt_scp_names_claim(struct xt_scp_names *names, const char *name, int fold);
void xt_scp_names_free(struct xt_scp_names *names);
#endif
