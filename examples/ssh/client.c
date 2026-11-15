/*
 * Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Author: Fabian Schmieder (@metaneutrons)
 *
 * SSH client for Limpet: libssh 0.12.2 with the aros-crypto kit (Mbed TLS).
 * Derived from mainline AROS workbench/network/sshd/ssh.c; see README.md.
 *
 * Usage: ssh [-v] [-i keyfile] [-p port] [-l user] [user@]host [command]
 *
 *   - keys, known_hosts and config in one drawer (ssh_directory below), in
 *     the formats of OpenSSH
 *   - an unknown host key is shown and asked about; a changed one is refused
 *   - public key, keyboard-interactive and password login
 *   - an interactive session, or one remote command with its return code
 */

#include <proto/dos.h>
#include <exec/types.h>
#include <dos/dos.h>

#include <libssh/libssh.h>
#include <aros_crypto.h>
#include "common.h"
#include "stream.h"

#include <string.h>
#include <stdlib.h>

#ifndef LIMPET_SSH_VERSION
#define LIMPET_SSH_VERSION "0.0"
#endif
#ifndef LIMPET_SSH_DATE
#define LIMPET_SSH_DATE "1.1.2026"
#endif
static const char __attribute__((used)) verstag[] = "\0$VER: ssh " LIMPET_SSH_VERSION " (" LIMPET_SSH_DATE ")\r\n";

static void usage(void)
{
    Printf("Usage: ssh [-v] [-i keyfile] [-p port] [-l user] [user@]host [command]\n");
    Printf("       ssh -V\n");
}

int main(int argc, char **argv)
{
    struct xt_ssh_login login = { NULL, NULL, NULL, 0 };
    const char *command = NULL;
    ssh_session session;
    ssh_channel channel;
    int i;
    int exit_code = RETURN_FAIL;
    int verbosity = 0;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-i") == 0 && i + 1 < argc)
            login.keyfile = argv[++i];
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc)
            login.port = atoi(argv[++i]);
        else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc)
            login.user = argv[++i];
        else if (strcmp(argv[i], "-v") == 0)
            ++verbosity;
        else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0)
        {
            Printf("Limpet ssh " LIMPET_SSH_VERSION " (libssh %s, aros-crypto %ld, entropy from %s)\n",
                   (IPTR)ssh_version(0), (long)AROS_CRYPTO_VERSION, (IPTR)aros_crypto_entropy_source());
            return RETURN_OK;
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "?") == 0 || (argv[i][0] == '-' && !login.host))
        {
            usage();
            return strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "?") == 0 ? RETURN_OK : RETURN_WARN;
        }
        else if (!login.host)
        {
            char *at = strrchr(argv[i], '@');
            if (at)
            {
                *at = 0;
                login.user = argv[i];
                login.host = at + 1;
            }
            else
            {
                login.host = argv[i];
            }
        }
        else if (!command)
        {
            command = argv[i];
        }
        else
        {
            Printf("ssh: put the remote command in quotes\n");
            return RETURN_WARN;
        }
    }
    if (!login.host || !*login.host || login.port < 0 || login.port > 65535)
    {
        usage();
        return RETURN_WARN;
    }

    if (xt_ssh_start(1, verbosity) != 0)
        return RETURN_FAIL;
    session = xt_ssh_connect(&login);
    if (session)
    {
        channel = ssh_channel_new(session);
        if (!channel || ssh_channel_open_session(channel) != SSH_OK)
        {
            Printf("ssh: cannot open a session: %s\n", (IPTR)ssh_get_error(session));
        }
        else
        {
            exit_code = xt_ssh_stream(session, channel, command);
            ssh_channel_send_eof(channel);
            ssh_channel_close(channel);
        }
        if (channel) ssh_channel_free(channel);
        xt_ssh_disconnect(session);
    }
    xt_ssh_stop();
    return exit_code;
}
