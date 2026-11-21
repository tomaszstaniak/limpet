/*
 * ssh-copy-id for Limpet: adds your public key to the keys a server accepts
 * for your account (.ssh/authorized_keys), so that the next login needs no
 * password. MIT, like the rest of Limpet.
 *
 * Usage: ssh-copy-id [-i keyfile] [-p port] [-v] [user@]host
 */

#include <proto/dos.h>
#include <proto/exec.h>
#include <exec/types.h>
#include <dos/dos.h>

#include <libssh/libssh.h>
#include <aros_crypto.h>
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef LIMPET_SSH_VERSION
#define LIMPET_SSH_VERSION "0.0"
#endif
#ifndef LIMPET_SSH_DATE
#define LIMPET_SSH_DATE "1.1.2026"
#endif
static const char __attribute__((used)) verstag[] = "\0$VER: ssh-copy-id " LIMPET_SSH_VERSION " (" LIMPET_SSH_DATE ")\r\n";

/* The server's shell adds the line it reads when the file does not have it
 * yet, and keeps both away from other accounts. */
static const char remote_command[] =
    "umask 077; mkdir -p .ssh || exit 1; IFS= read -r key || exit 1; "
    "if grep -qxF -- \"$key\" .ssh/authorized_keys 2>/dev/null; then echo present; "
    "else printf '%s\\n' \"$key\" >> .ssh/authorized_keys && echo added; fi";

/* One public key line from file, without the line end. 0, or -1. */
static int read_public(const char *path, char *line, size_t size)
{
    BPTR in = Open((CONST_STRPTR)path, MODE_OLDFILE);
    LONG got;
    size_t i;
    if (!in) return -1;
    got = Read(in, line, (LONG)size - 1);
    Close(in);
    if (got <= 0) return -1;
    line[got] = 0;
    line[strcspn(line, "\r\n")] = 0;
    /* "type base64 [comment]" in printable characters, not a private key. */
    if (strncmp(line, "ssh-", 4) && strncmp(line, "ecdsa-", 6) && strncmp(line, "sk-", 3)) return -1;
    if (!strchr(line, ' ')) return -1;
    for (i = 0; line[i]; ++i)
        if ((unsigned char)line[i] < 32 || line[i] == 127) return -1;
    return 0;
}

static void usage(void)
{
    Printf("Usage: ssh-copy-id [-i keyfile] [-p port] [-v] [user@]host\n");
    Printf("       Adds your public key to your account on the server.\n");
}

static int program(int argc, char **argv)
{
    struct xt_ssh_login login = { NULL, NULL, NULL, 0 };
    const char *keyfile = NULL;
    char directory[300], path[420], line[8192], answer[64];
    ssh_session session;
    ssh_channel channel = NULL;
    int verbosity = 0, i, result = RETURN_FAIL;

    xt_ssh_tool("ssh-copy-id");
    for (i = 1; i < argc; ++i)
    {
        const char *a = argv[i];
        if (!strcmp(a, "-i") && i + 1 < argc) keyfile = argv[++i];
        else if (!strcmp(a, "-p") && i + 1 < argc) login.port = atoi(argv[++i]);
        else if (!strcmp(a, "-v")) ++verbosity;
        else if (!strcmp(a, "-V") || !strcmp(a, "--version"))
        {
            Printf("Limpet ssh-copy-id " LIMPET_SSH_VERSION " (libssh %s, aros-crypto %ld)\n",
                   (IPTR)ssh_version(0), (long)AROS_CRYPTO_VERSION);
            return RETURN_OK;
        }
        else if (a[0] == '-' || !strcmp(a, "?") || login.host)
        {
            usage();
            return !strcmp(a, "-h") || !strcmp(a, "?") ? RETURN_OK : RETURN_WARN;
        }
        else
        {
            char *at = strrchr(argv[i], '@');
            if (at) { *at = 0; login.user = argv[i]; login.host = at + 1; }
            else login.host = argv[i];
        }
    }
    if (!login.host || !*login.host || login.port < 0 || login.port > 65535)
    {
        usage();
        return RETURN_WARN;
    }

    /* The public file: the one named (with or without .pub), or the first
     * of the SSH drawer's keys. */
    if (keyfile)
    {
        size_t length = strlen(keyfile);
        snprintf(path, sizeof path, "%s%s", keyfile, length > 4 && !strcmp(keyfile + length - 4, ".pub") ? "" : ".pub");
        if (read_public(path, line, sizeof line))
        {
            Printf("ssh-copy-id: %s is not a public key file\n", (IPTR)path);
            return RETURN_ERROR;
        }
    }
    else
    {
        static const char *const names[] = { "id_ed25519", "id_ecdsa", "id_rsa" };
        xt_ssh_directory(directory, sizeof directory);
        for (i = 0; i < 3; ++i)
        {
            snprintf(path, sizeof path, "%s/%s.pub", directory, names[i]);
            if (read_public(path, line, sizeof line) == 0) break;
        }
        if (i == 3)
        {
            Printf("ssh-copy-id: no public key in %s; make a key with ssh-keygen first\n", (IPTR)directory);
            return RETURN_ERROR;
        }
    }

    if (xt_ssh_start(1, verbosity) != 0) return RETURN_FAIL;
    session = xt_ssh_connect(&login);
    if (!session) goto done;
    channel = ssh_channel_new(session);
    if (!channel || ssh_channel_open_session(channel) != SSH_OK
        || ssh_channel_request_exec(channel, remote_command) != SSH_OK)
    {
        Printf("ssh-copy-id: the server did not run the command: %s\n", (IPTR)ssh_get_error(session));
        goto close;
    }
    strcat(line, "\n");
    if (ssh_channel_write(channel, line, (uint32_t)strlen(line)) != (int)strlen(line)
        || ssh_channel_send_eof(channel) != SSH_OK)
    {
        Printf("ssh-copy-id: the key was not sent: %s\n", (IPTR)ssh_get_error(session));
        goto close;
    }
    {
        /* The answer is one word; anything else the server says is shown. */
        size_t at = 0;
        int got;
        while (at + 1 < sizeof answer
               && (got = ssh_channel_read_timeout(channel, answer + at, (uint32_t)(sizeof answer - 1 - at), 0, 30000)) > 0)
            at += (size_t)got;
        answer[at] = 0;
        answer[strcspn(answer, "\r\n")] = 0;
    }
    if (!strcmp(answer, "added"))
    {
        Printf("The key %s is now accepted by %s.\n", (IPTR)path, (IPTR)login.host);
        result = RETURN_OK;
    }
    else if (!strcmp(answer, "present"))
    {
        Printf("%s has the key %s already.\n", (IPTR)login.host, (IPTR)path);
        result = RETURN_OK;
    }
    else
    {
        char text[256];
        int got = ssh_channel_read_timeout(channel, text, sizeof text - 1, 1, 2000);
        Printf("ssh-copy-id: the server did not add the key");
        if (*answer) Printf(": %s", (IPTR)answer);
        Printf("\n");
        if (got > 0) { text[got] = 0; Printf("%s", (IPTR)text); }
        result = RETURN_ERROR;
    }
close:
    if (channel)
    {
        ssh_channel_close(channel);
        ssh_channel_free(channel);
    }
    xt_ssh_disconnect(session);
done:
    xt_ssh_stop();
    return result;
}

int main(int argc, char **argv)
{
    return xt_ssh_run(program, argc, argv);
}
