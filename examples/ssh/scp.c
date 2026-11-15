/*
 * scp for Limpet: copies files between AROS and a server over the SFTP
 * service of SSH. MIT, like the rest of Limpet.
 *
 * Usage: scp [-r] [-q] [-v] [-i keyfile] [-P port] source... target
 *
 * One side is a server ([user@]host:path), the other is AROS. Local sources
 * may be AmigaDOS patterns (#?.txt). See scp_spec.h for how a server is
 * told from a volume.
 */

#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosasl.h>
#include <dos/dosextens.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <aros_crypto.h>
#include "common.h"
#include "scp_spec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifndef LIMPET_SSH_VERSION
#define LIMPET_SSH_VERSION "0.0"
#endif
#ifndef LIMPET_SSH_DATE
#define LIMPET_SSH_DATE "1.1.2026"
#endif
static const char __attribute__((used)) verstag[] = "\0$VER: scp " LIMPET_SSH_VERSION " (" LIMPET_SSH_DATE ")\r\n";

#define PATH_SIZE 1024
#define CHUNK 32768

static ssh_session session;
static sftp_session sftp;
static char *chunk;
static int recursive, quiet;
static int failures;        /* files or drawers not copied */
static int stopped;         /* Ctrl-C or a lost connection: copy nothing more */
static int broken;          /* it was Ctrl-C */
static unsigned long files;
static unsigned long long bytes;

static int is_device(const char *name, size_t length)
{
    char text[108];
    struct DosList *list;
    int found;

    if (length >= sizeof text) return 0;
    memcpy(text, name, length);
    text[length] = 0;
    if (!strcasecmp(text, "PROGDIR") || !strcasecmp(text, "NIL") || !strcasecmp(text, "CONSOLE")) return 1;
    list = LockDosList(LDF_ALL | LDF_READ);
    found = FindDosEntry(list, (CONST_STRPTR)text, LDF_ALL) != NULL;
    UnLockDosList(LDF_ALL | LDF_READ);
    return found;
}

static int interrupted(void)
{
    if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) stopped = broken = 1;
    return stopped;
}

/* Why the server refused the last SFTP request. */
static const char *remote_reason(void)
{
    if (!ssh_is_connected(session))
    {
        stopped = 1;
        return "the connection was lost";
    }
    switch (sftp_get_error(sftp))
    {
    case SSH_FX_NO_SUCH_FILE:
    case SSH_FX_NO_SUCH_PATH: return "no such file or directory";
    case SSH_FX_PERMISSION_DENIED: return "permission denied";
    case SSH_FX_FILE_ALREADY_EXISTS: return "exists already";
    case SSH_FX_WRITE_PROTECT: return "write protected";
    case SSH_FX_OP_UNSUPPORTED: return "not supported by the server";
    case SSH_FX_NO_CONNECTION:
    case SSH_FX_CONNECTION_LOST: stopped = 1; return "the connection was lost";
    case SSH_FX_FAILURE: return "the server refused";
    default:
        {
            /* No SFTP status and no error text: the answer did not come. */
            const char *text = ssh_get_error(session);
            return text && *text ? text : "no answer from the server in time";
        }
    }
}

static void remote_failure(const char *path)
{
    Printf("scp: %s: %s\n", (IPTR)path, (IPTR)remote_reason());
    ++failures;
}

static void local_failure(const char *path)
{
    char text[100];
    Fault(IoErr(), NULL, (STRPTR)text, sizeof text);
    Printf("scp: %s: %s\n", (IPTR)(*path ? path : "the current drawer"), (IPTR)text);
    ++failures;
}

static void failure(const char *path, const char *text)
{
    Printf("scp: %s: %s\n", (IPTR)path, (IPTR)text);
    ++failures;
}

static unsigned long seconds_now(void)
{
    struct DateStamp now;
    DateStamp(&now);
    return (unsigned long)now.ds_Days * 86400UL + (unsigned long)now.ds_Minute * 60UL
        + (unsigned long)now.ds_Tick / TICKS_PER_SECOND;
}

/* One line per file. In a window the line shows the percentage while a
 * large file is copied; into a file only the finished line is written. */
struct progress {
    const char *name;
    unsigned long long total, done;
    unsigned long shown;
    int interactive, drawn;
};

static void progress_start(struct progress *p, const char *name, unsigned long long total)
{
    p->name = name;
    p->total = total;
    p->done = 0;
    p->shown = seconds_now();
    p->interactive = !quiet && IsInteractive(Output());
    p->drawn = 0;
}

static void progress_step(struct progress *p, unsigned long count)
{
    unsigned long now;
    p->done += count;
    if (!p->interactive || !p->total) return;
    now = seconds_now();
    if (now == p->shown) return;
    p->shown = now;
    Printf("\r%s  %ld%%\033[K", (IPTR)p->name, (long)(p->done * 100 / p->total));
    Flush(Output());
    p->drawn = 1;
}

static void progress_end(struct progress *p, int ok)
{
    char size[32];
    if (p->drawn) Printf("\r\033[K");
    if (ok)
    {
        ++files;
        bytes += p->done;
        if (!quiet)
        {
            snprintf(size, sizeof size, "%llu", p->done);
            Printf("%s  %s bytes\n", (IPTR)p->name, (IPTR)size);
        }
    }
    Flush(Output());
}

/* --- AROS to the server --- */

static void upload_file(const char *local, const char *remote)
{
    struct FileInfoBlock *info;
    struct progress p;
    unsigned long long total = 0;
    sftp_file out;
    BPTR in;
    LONG got = 0;
    int ok = 1;

    if (interrupted()) return;
    in = Open((CONST_STRPTR)local, MODE_OLDFILE);
    if (!in) { local_failure(local); return; }
    if ((info = AllocDosObject(DOS_FIB, NULL)))
    {
        if (ExamineFH(in, info) && info->fib_Size > 0) total = (unsigned long long)info->fib_Size;
        FreeDosObject(DOS_FIB, info);
    }
    out = sftp_open(sftp, remote, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!out)
    {
        remote_failure(remote);
        Close(in);
        return;
    }
    progress_start(&p, local, total);
    while (ok && (got = Read(in, chunk, CHUNK)) > 0)
    {
        LONG at = 0;
        while (at < got)
        {
            ssize_t sent = sftp_write(out, chunk + at, (size_t)(got - at));
            if (sent <= 0) { ok = 0; break; }
            at += (LONG)sent;
        }
        if (ok) progress_step(&p, (unsigned long)got);
        if (interrupted()) break;
    }
    if (got < 0)
    {
        progress_end(&p, 0);
        local_failure(local);
        sftp_close(out);
        Close(in);
        return;
    }
    Close(in);
    if (!ok)
    {
        progress_end(&p, 0);
        remote_failure(remote);
        sftp_close(out);
        return;
    }
    if (sftp_close(out) != SSH_OK)
    {
        progress_end(&p, 0);
        remote_failure(remote);
        return;
    }
    if (stopped)
    {
        progress_end(&p, 0);
        Printf("scp: %s is incomplete on the server\n", (IPTR)remote);
        ++failures;
        return;
    }
    progress_end(&p, 1);
}

static int remote_is_directory(const char *path)
{
    sftp_attributes attributes = sftp_stat(sftp, path);
    int result = attributes && attributes->type == SSH_FILEXFER_TYPE_DIRECTORY;
    if (attributes) sftp_attributes_free(attributes);
    return result;
}

static void upload_directory(const char *local, const char *remote)
{
    struct FileInfoBlock *info = AllocDosObject(DOS_FIB, NULL);
    char *local_child = malloc(PATH_SIZE), *remote_child = malloc(PATH_SIZE);
    BPTR lock = Lock((CONST_STRPTR)local, SHARED_LOCK);

    if (!info || !local_child || !remote_child) { failure(local, "out of memory"); goto done; }
    if (!lock || !Examine(lock, info)) { local_failure(local); goto done; }
    if (sftp_mkdir(sftp, remote, 0755) != SSH_OK && !remote_is_directory(remote))
    {
        failure(remote, "cannot make this directory on the server");
        goto done;
    }
    while (!interrupted() && ExNext(lock, info))
    {
        size_t length = strlen(local);
        if (length >= PATH_SIZE || xt_scp_remote_join(remote_child, PATH_SIZE, remote, (const char *)info->fib_FileName))
        {
            failure((const char *)info->fib_FileName, "the path is too long");
            continue;
        }
        memcpy(local_child, local, length + 1);
        if (!AddPart((STRPTR)local_child, info->fib_FileName, PATH_SIZE))
        {
            failure((const char *)info->fib_FileName, "the path is too long");
            continue;
        }
        if (info->fib_DirEntryType == ST_SOFTLINK)
            Printf("scp: %s is a link; not copied\n", (IPTR)local_child);
        else if (info->fib_DirEntryType > 0)
            upload_directory(local_child, remote_child);
        else
            upload_file(local_child, remote_child);
    }
    if (!stopped && IoErr() != ERROR_NO_MORE_ENTRIES) local_failure(local);
done:
    if (lock) UnLock(lock);
    if (info) FreeDosObject(DOS_FIB, info);
    free(local_child);
    free(remote_child);
}

/* All local sources to the server path target. */
static void upload(char **sources, int count, const char *target)
{
    int target_is_directory = remote_is_directory(target);
    struct AnchorPath *anchor = AllocVec(sizeof *anchor + PATH_SIZE, MEMF_CLEAR);
    char *remote = malloc(PATH_SIZE);
    int items = 0, i;

    if (!anchor || !remote) { failure(target, "out of memory"); goto done; }
    for (i = 0; i < count && !stopped; ++i)
    {
        LONG error;
        int matched = 0;
        memset(anchor, 0, sizeof *anchor);
        anchor->ap_Strlen = PATH_SIZE;
        anchor->ap_BreakBits = SIGBREAKF_CTRL_C;
        for (error = MatchFirst((CONST_STRPTR)sources[i], anchor); !error; error = MatchNext(anchor))
        {
            const char *local = (const char *)anchor->ap_Buf;
            const char *name = (const char *)FilePart((CONST_STRPTR)local);
            int is_directory = anchor->ap_Info.fib_DirEntryType > 0 && anchor->ap_Info.fib_DirEntryType != ST_SOFTLINK;
            matched = 1;
            if (is_directory && !recursive)
            {
                failure(local, "is a drawer; use -r to copy drawers");
                continue;
            }
            if (!target_is_directory && items++ > 0)
            {
                failure(target, "is not a directory on the server; it cannot take several files");
                stopped = 1;
                break;
            }
            if (target_is_directory && *name)
            {
                if (xt_scp_remote_join(remote, PATH_SIZE, target, name)) { failure(local, "the path is too long"); continue; }
            }
            else
            {
                snprintf(remote, PATH_SIZE, "%s", target);
            }
            if (is_directory) upload_directory(local, remote);
            else upload_file(local, remote);
            if (stopped) break;
        }
        MatchEnd(anchor);
        if (error == ERROR_BREAK) stopped = broken = 1;
        else if (!matched && !stopped)
        {
            SetIoErr(error == ERROR_NO_MORE_ENTRIES ? ERROR_OBJECT_NOT_FOUND : error);
            local_failure(sources[i]);
        }
    }
done:
    if (anchor) FreeVec(anchor);
    free(remote);
}

/* --- The server to AROS --- */

static void download_file(const char *remote, const char *local, unsigned long long size)
{
    struct progress p;
    sftp_file in;
    BPTR out;
    int ok = 1, local_error = 0;

    if (interrupted()) return;
    in = sftp_open(sftp, remote, O_RDONLY, 0);
    if (!in) { remote_failure(remote); return; }
    out = Open((CONST_STRPTR)local, MODE_NEWFILE);
    if (!out)
    {
        local_failure(local);
        sftp_close(in);
        return;
    }
    progress_start(&p, local, size);
    for (;;)
    {
        ssize_t got = sftp_read(in, chunk, CHUNK);
        if (got == 0) break;
        if (got < 0) { ok = 0; break; }
        if (Write(out, chunk, (LONG)got) != (LONG)got) { ok = 0; local_error = 1; break; }
        progress_step(&p, (unsigned long)got);
        if (interrupted()) { ok = 0; break; }
    }
    progress_end(&p, ok);
    if (!ok)
    {
        if (local_error) local_failure(local);
        else if (!stopped || !ssh_is_connected(session)) remote_failure(remote);
        else ++failures;
    }
    Close(out);
    sftp_close(in);
    /* Half a file is worse than none. */
    if (!ok) DeleteFile((CONST_STRPTR)local);
}

/* 1 for a drawer, 0 for a file, -1 when nothing has this name. */
static int local_kind(const char *path)
{
    struct FileInfoBlock *info = AllocDosObject(DOS_FIB, NULL);
    BPTR lock = Lock((CONST_STRPTR)path, SHARED_LOCK);
    int kind = -1;
    if (lock && info && Examine(lock, info)) kind = info->fib_DirEntryType > 0;
    if (lock) UnLock(lock);
    if (info) FreeDosObject(DOS_FIB, info);
    return kind;
}

static void download_directory(const char *remote, const char *local)
{
    char *local_child = malloc(PATH_SIZE), *remote_child = malloc(PATH_SIZE);
    char name[256];
    sftp_dir directory = NULL;
    sftp_attributes entry;
    int kind = local_kind(local);

    if (!local_child || !remote_child) { failure(local, "out of memory"); goto done; }
    if (kind == 0) { failure(local, "is a file; a drawer is needed here"); goto done; }
    if (kind < 0)
    {
        BPTR lock = CreateDir((CONST_STRPTR)local);
        if (!lock) { local_failure(local); goto done; }
        UnLock(lock);
    }
    directory = sftp_opendir(sftp, remote);
    if (!directory) { remote_failure(remote); goto done; }
    while (!interrupted() && (entry = sftp_readdir(sftp, directory)))
    {
        const char *given = entry->name ? entry->name : "";
        size_t length = strlen(local);
        int type = entry->type;
        unsigned long long size = entry->size;

        if (!strcmp(given, ".") || !strcmp(given, "..")) { sftp_attributes_free(entry); continue; }
        if (xt_scp_local_name(name, sizeof name, given, strlen(given)) || length >= PATH_SIZE
            || xt_scp_remote_join(remote_child, PATH_SIZE, remote, given))
        {
            failure(given, "this name cannot be used on AROS; not copied");
            sftp_attributes_free(entry);
            continue;
        }
        memcpy(local_child, local, length + 1);
        if (!AddPart((STRPTR)local_child, (CONST_STRPTR)name, PATH_SIZE))
        {
            failure(given, "the path is too long");
            sftp_attributes_free(entry);
            continue;
        }
        sftp_attributes_free(entry);
        if (type == SSH_FILEXFER_TYPE_SYMLINK)
        {
            /* A link to a file is copied as that file. A link to a directory
             * is not followed: it can lead back to where the copy started. */
            sftp_attributes aim = sftp_stat(sftp, remote_child);
            int aim_type = aim ? aim->type : SSH_FILEXFER_TYPE_UNKNOWN;
            if (aim) { size = aim->size; sftp_attributes_free(aim); }
            if (aim_type == SSH_FILEXFER_TYPE_DIRECTORY)
            {
                Printf("scp: %s is a link to a directory; not followed\n", (IPTR)remote_child);
                continue;
            }
            type = aim_type == SSH_FILEXFER_TYPE_REGULAR ? SSH_FILEXFER_TYPE_REGULAR : SSH_FILEXFER_TYPE_SPECIAL;
        }
        if (type == SSH_FILEXFER_TYPE_DIRECTORY) download_directory(remote_child, local_child);
        else if (type == SSH_FILEXFER_TYPE_REGULAR) download_file(remote_child, local_child, size);
        else Printf("scp: %s is not a file or a directory; not copied\n", (IPTR)remote_child);
    }
    if (!stopped && !sftp_dir_eof(directory)) remote_failure(remote);
done:
    if (directory) sftp_closedir(directory);
    free(local_child);
    free(remote_child);
}

/* All server paths to the local path target. */
static void download(struct xt_scp_spec *sources, int count, const char *target)
{
    int target_is_directory = local_kind(target) == 1;
    char *local = malloc(PATH_SIZE);
    char name[256];
    int items = 0, i;

    if (!local) { failure(target, "out of memory"); return; }
    for (i = 0; i < count && !stopped; ++i)
    {
        const char *remote = sources[i].path;
        sftp_attributes attributes = sftp_stat(sftp, remote);
        int is_directory;
        unsigned long long size;

        if (!attributes) { remote_failure(remote); continue; }
        is_directory = attributes->type == SSH_FILEXFER_TYPE_DIRECTORY;
        size = attributes->size;
        if (!is_directory && attributes->type != SSH_FILEXFER_TYPE_REGULAR)
        {
            sftp_attributes_free(attributes);
            failure(remote, "is not a file or a directory");
            continue;
        }
        sftp_attributes_free(attributes);
        if (is_directory && !recursive)
        {
            failure(remote, "is a directory; use -r to copy directories");
            continue;
        }
        if (!target_is_directory && items++ > 0)
        {
            failure(target, "is not a drawer; it cannot take several files");
            break;
        }
        snprintf(local, PATH_SIZE, "%s", target);
        if (target_is_directory)
        {
            /* The name the server has for it; for "." and "/", ask. */
            size_t length;
            const char *last = xt_scp_remote_name(remote, &length);
            char *full = NULL;
            if (!length && (full = sftp_canonicalize_path(sftp, remote))) last = xt_scp_remote_name(full, &length);
            if (!length) { last = sources[i].host; length = strlen(last); }
            if (xt_scp_local_name(name, sizeof name, last, length) || !AddPart((STRPTR)local, (CONST_STRPTR)name, PATH_SIZE))
            {
                failure(remote, "its name cannot be used on AROS; name the target file");
                if (full) ssh_string_free_char(full);
                continue;
            }
            if (full) ssh_string_free_char(full);
        }
        if (is_directory) download_directory(remote, local);
        else download_file(remote, local, size);
    }
    free(local);
}

static void usage(void)
{
    Printf("Usage: scp [-r] [-q] [-v] [-i keyfile] [-P port] source... target\n");
    Printf("       One side is a server, written [user@]host:path; the other is AROS.\n");
    Printf("       scp notes.txt me@server:          to the home directory there\n");
    Printf("       scp -r me@server:project Work:    a directory with its contents\n");
}

int main(int argc, char **argv)
{
    struct xt_ssh_login login = { NULL, NULL, NULL, 0 };
    struct xt_scp_spec *specs;
    char **local_sources;
    int count = 0, verbosity = 0, i, result = RETURN_FAIL;
    int to_server;

    xt_ssh_tool("scp");
    specs = calloc((size_t)argc + 1, sizeof *specs);
    local_sources = calloc((size_t)argc + 1, sizeof *local_sources);
    if (!specs || !local_sources)
    {
        Printf("scp: out of memory\n");
        return RETURN_FAIL;
    }
    for (i = 1; i < argc; ++i)
    {
        const char *a = argv[i];
        if (count == 0 && a[0] == '-' && a[1])
        {
            if (!strcmp(a, "-r")) recursive = 1;
            else if (!strcmp(a, "-q")) quiet = 1;
            else if (!strcmp(a, "-v")) ++verbosity;
            else if (!strcmp(a, "-i") && i + 1 < argc) login.keyfile = argv[++i];
            else if (!strcmp(a, "-P") && i + 1 < argc) login.port = atoi(argv[++i]);
            else if (!strcmp(a, "-V") || !strcmp(a, "--version"))
            {
                Printf("Limpet scp " LIMPET_SSH_VERSION " (libssh %s, aros-crypto %ld)\n",
                       (IPTR)ssh_version(0), (long)AROS_CRYPTO_VERSION);
                return RETURN_OK;
            }
            else
            {
                usage();
                return !strcmp(a, "-h") ? RETURN_OK : RETURN_WARN;
            }
            continue;
        }
        if (!strcmp(a, "?")) { usage(); return RETURN_OK; }
        if (xt_scp_split(a, is_device, &specs[count]))
        {
            Printf("scp: %s: too long, or no host name before the colon\n", (IPTR)a);
            return RETURN_WARN;
        }
        local_sources[count++] = argv[i];
    }
    if (count < 2 || login.port < 0 || login.port > 65535)
    {
        usage();
        return RETURN_WARN;
    }
    --count; /* the last one is the target */
    to_server = specs[count].remote;
    for (i = 0; i < count; ++i)
    {
        const struct xt_scp_spec *first = &specs[to_server ? count : 0];
        if (specs[i].remote == to_server
            || (!to_server && (strcmp(specs[i].host, first->host) || strcmp(specs[i].user, first->user))))
        {
            Printf("scp: copy from AROS to one server, or from one server to AROS\n");
            Printf("     (a server is written [user@]host:path)\n");
            return RETURN_WARN;
        }
    }
    {
        const struct xt_scp_spec *server = &specs[to_server ? count : 0];
        login.host = server->host;
        login.user = server->user[0] ? server->user : NULL;
    }

    chunk = malloc(CHUNK);
    if (!chunk)
    {
        Printf("scp: out of memory\n");
        return RETURN_FAIL;
    }
    if (xt_ssh_start(1, verbosity) != 0) return RETURN_FAIL;
    session = xt_ssh_connect(&login);
    if (session)
    {
        /* A transfer waits longer for an answer than a login does: a slow
         * link may pause for a while without being lost. */
        long patience = 120;
        ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &patience);
        sftp = sftp_new(session);
        if (!sftp || sftp_init(sftp) != SSH_OK)
        {
            Printf("scp: %s has no file transfer service (sftp): %s\n", (IPTR)login.host, (IPTR)ssh_get_error(session));
        }
        else
        {
            if (to_server) upload(local_sources, count, specs[count].path);
            else download(specs, count, specs[count].path);
            if (!quiet && files > 1)
            {
                char total[32];
                snprintf(total, sizeof total, "%llu", bytes);
                Printf("%ld files, %s bytes\n", (long)files, (IPTR)total);
            }
            if (broken) Printf("scp: stopped\n");
            result = failures || stopped ? RETURN_ERROR : RETURN_OK;
        }
        if (sftp) sftp_free(sftp);
        xt_ssh_disconnect(session);
    }
    xt_ssh_stop();
    free(chunk);
    free(specs);
    free(local_sources);
    return result;
}
