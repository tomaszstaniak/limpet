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

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/var.h>

#include <bsdsocket/socketbasetags.h>
#include <utility/tagitem.h>
#include <errno.h>

#include <libssh/libssh.h>
#include <libssh/callbacks.h>
#include <aros_crypto.h>
#include "stream.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef LIMPET_SSH_VERSION
#define LIMPET_SSH_VERSION "0.0"
#endif
#ifndef LIMPET_SSH_DATE
#define LIMPET_SSH_DATE "1.1.2026"
#endif
static const char __attribute__((used)) verstag[] = "\0$VER: ssh " LIMPET_SSH_VERSION " (" LIMPET_SSH_DATE ")\r\n";

#define DEFAULT_PORT 22

/* libnet owns startup/shutdown of bsdsocket.library. */
extern struct Library *SocketBase;

/* Where keys, known_hosts and config are: HOME/.ssh when the HOME variable
 * is set, as on other systems; otherwise ENVARC:SSH. */
static void ssh_directory(char *path, size_t size)
{
    char home[256];
    LONG length = GetVar("HOME", home, sizeof home, 0);
    if (length > 0) {
        char last = home[length - 1];
        snprintf(path, size, "%s%s.ssh", home, last == ':' || last == '/' ? "" : "/");
    } else {
        snprintf(path, size, "ENVARC:SSH");
    }
}

/* One line from the user. With echo off the terminal is read key by key and
 * nothing is shown. 0 on success, -1 when there is no user to ask (input is
 * a file), on Ctrl-C or at end of input. */
static int ask(const char *prompt, char *answer, size_t size, int echo)
{
    BPTR input = Input();
    size_t at = 0;
    int result = -1;
    answer[0] = 0;
    if (!IsInteractive(input)) return -1;
    Printf("%s", (IPTR)prompt);
    Flush(Output());
    if (echo) {
        /* Read(), not FGets(): the handle's buffer still holds this
         * command's own argument line. */
        LONG got = Read(input, answer, (LONG)size - 1);
        if (got > 0) {
            at = (size_t)got;
            answer[at] = 0;
            while (at && (answer[at - 1] == '\n' || answer[at - 1] == '\r')) answer[--at] = 0;
            result = 0;
        }
        return result;
    }
    if (!SetMode(input, 1)) return -1;
    for (;;) {
        unsigned char c;
        if (Read(input, &c, 1) != 1 || c == 3) break;         /* end, Ctrl-C */
        if (c == '\r' || c == '\n') { result = 0; break; }
        if (c == 8 || c == 127) { if (at) answer[--at] = 0; continue; }
        if (c == 0x9b || c == 0x1b) continue;                  /* start of a key sequence */
        if (c >= 32 && at + 1 < size) { answer[at++] = (char)c; answer[at] = 0; }
    }
    SetMode(input, 0);
    Printf("\n");
    if (result) memset(answer, 0, size);
    return result;
}

/* The passphrase of a private key (libssh asks through this). */
static int ask_passphrase(const char *prompt, char *buffer, size_t size, int echo, int verify, void *userdata)
{
    char text[300];
    (void)verify; (void)userdata;
    snprintf(text, sizeof text, "%s: ", prompt ? prompt : "Passphrase");
    return ask(text, buffer, size, echo);
}

/* --- The server's key --- */
static int verify_host(ssh_session session, const char *host, int port)
{
    ssh_key key = NULL;
    unsigned char *hash = NULL;
    size_t hash_size = 0;
    char *fingerprint = NULL;
    const char *type = "unknown";
    char answer[16];
    int result = -1;

    if (ssh_get_server_publickey(session, &key) == SSH_OK) {
        type = ssh_key_type_to_char(ssh_key_type(key));
        if (ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_SHA256, &hash, &hash_size) == SSH_OK)
            fingerprint = ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hash_size);
    }
    if (!fingerprint) {
        Printf("ssh: cannot read the server's key: %s\n", (IPTR)ssh_get_error(session));
        goto done;
    }
    switch (ssh_session_is_known_server(session)) {
    case SSH_KNOWN_HOSTS_OK:
        result = 0;
        break;
    case SSH_KNOWN_HOSTS_CHANGED:
        Printf("ssh: THE KEY OF %s HAS CHANGED.\n", (IPTR)host);
        Printf("It is now %s %s.\n", (IPTR)type, (IPTR)fingerprint);
        Printf("Either the server was set up again or someone is answering in its place.\n");
        Printf("Not connected. If the change is expected, remove the old line for this\n");
        Printf("host from known_hosts.\n");
        break;
    case SSH_KNOWN_HOSTS_OTHER:
        Printf("ssh: %s is known with a key of another type; the server now offers\n", (IPTR)host);
        Printf("%s %s.\n", (IPTR)type, (IPTR)fingerprint);
        Printf("Not connected: this can be an attempt to make you accept a new key.\n");
        break;
    case SSH_KNOWN_HOSTS_NOT_FOUND:
    case SSH_KNOWN_HOSTS_UNKNOWN:
        if (port == DEFAULT_PORT) Printf("%s is not a known host.\n", (IPTR)host);
        else Printf("%s, port %ld, is not a known host.\n", (IPTR)host, (long)port);
        Printf("Its %s key has the fingerprint %s.\n", (IPTR)type, (IPTR)fingerprint);
        if (ask("Connect and remember this key (yes/no)? ", answer, sizeof answer, 1) != 0) {
            Printf("ssh: the key cannot be confirmed without a terminal; not connected\n");
            break;
        }
        if (strcmp(answer, "yes") != 0 && strcmp(answer, "YES") != 0 && strcmp(answer, "Yes") != 0) {
            Printf("Not connected.\n");
            break;
        }
        {
            /* libssh makes missing drawers with the C library; make the SSH
             * drawer the AmigaDOS way first so that a failure is seen here. */
            char directory[300];
            BPTR lock;
            ssh_directory(directory, sizeof directory);
            if ((lock = Lock(directory, SHARED_LOCK)) || (lock = CreateDir(directory))) UnLock(lock);
        }
        if (ssh_session_update_known_hosts(session) != SSH_OK)
            Printf("ssh: the key was not saved (%s); you will be asked again\n", (IPTR)ssh_get_error(session));
        result = 0;
        break;
    default:
        Printf("ssh: cannot check known_hosts: %s\n", (IPTR)ssh_get_error(session));
        break;
    }
done:
    if (fingerprint) ssh_string_free_char(fingerprint);
    if (hash) ssh_clean_pubkey_hash(&hash);
    if (key) ssh_key_free(key);
    return result;
}

/* --- Login --- */
static int authenticate(ssh_session session, const char *keyfile)
{
    char answer[256];
    int rc, methods, tries;

    rc = ssh_userauth_none(session, NULL);
    if (rc == SSH_AUTH_SUCCESS) return 0;
    if (rc == SSH_AUTH_ERROR) return -1;
    methods = ssh_userauth_list(session, NULL);

    if (methods & SSH_AUTH_METHOD_PUBLICKEY) {
        if (keyfile) {
            ssh_key key = NULL;
            rc = ssh_pki_import_privkey_file(keyfile, NULL, ask_passphrase, NULL, &key);
            if (rc == SSH_OK) {
                rc = ssh_userauth_publickey(session, NULL, key);
                ssh_key_free(key);
                if (rc == SSH_AUTH_SUCCESS) return 0;
            } else {
                Printf("ssh: cannot read the key %s\n", (IPTR)keyfile);
            }
        } else {
            /* The keys named in config, or id_ed25519, id_ecdsa and id_rsa
             * of the SSH drawer. */
            rc = ssh_userauth_publickey_auto(session, NULL, NULL);
            if (rc == SSH_AUTH_SUCCESS) return 0;
        }
    }

    if (methods & SSH_AUTH_METHOD_INTERACTIVE) {
        for (tries = 0; tries < 3; ++tries) {
            rc = ssh_userauth_kbdint(session, NULL, NULL);
            while (rc == SSH_AUTH_INFO) {
                const char *text = ssh_userauth_kbdint_getinstruction(session);
                int count = ssh_userauth_kbdint_getnprompts(session), i;
                if (text && *text) Printf("%s\n", (IPTR)text);
                for (i = 0; i < count; ++i) {
                    char echo = 0;
                    const char *prompt = ssh_userauth_kbdint_getprompt(session, i, &echo);
                    if (ask(prompt, answer, sizeof answer, echo) != 0) return -1;
                    rc = ssh_userauth_kbdint_setanswer(session, i, answer);
                    memset(answer, 0, sizeof answer);
                    if (rc < 0) return -1;
                }
                rc = ssh_userauth_kbdint(session, NULL, NULL);
            }
            if (rc == SSH_AUTH_SUCCESS) return 0;
            if (rc != SSH_AUTH_DENIED && rc != SSH_AUTH_PARTIAL) break;
        }
    }

    if (methods & SSH_AUTH_METHOD_PASSWORD) {
        for (tries = 0; tries < 3; ++tries) {
            if (ask("Password: ", answer, sizeof answer, 0) != 0) return -1;
            rc = ssh_userauth_password(session, NULL, answer);
            memset(answer, 0, sizeof answer);
            if (rc == SSH_AUTH_SUCCESS) return 0;
            if (rc != SSH_AUTH_DENIED && rc != SSH_AUTH_PARTIAL) break;
            Printf("Not accepted.\n");
        }
    }
    return -1;
}

/* -v: libssh's own log on standard output, so that it can be redirected
 * to a file with the rest. */
static void log_line(int priority, const char *function, const char *buffer, void *userdata)
{
    (void)userdata;
    Printf("ssh[%ld] %s: %s\n", (long)priority, (IPTR)function, (IPTR)buffer);
}

static void usage(void)
{
    Printf("Usage: ssh [-v] [-i keyfile] [-p port] [-l user] [user@]host [command]\n");
    Printf("       ssh -V\n");
}

int main(int argc, char **argv)
{
    const char *host = NULL;
    const char *user = NULL;
    const char *keyfile = NULL;
    const char *command = NULL;
    char directory[300], path[340], name[128];
    char *known_user = NULL;
    struct ssh_callbacks_struct callbacks;
    ssh_session session = NULL;
    ssh_channel channel = NULL;
    int port = 0;
    int i;
    int exit_code = RETURN_FAIL;
    int verbosity = 0;
    long timeout = 30;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-i") == 0 && i + 1 < argc)
            keyfile = argv[++i];
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc)
            port = atoi(argv[++i]);
        else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc)
            user = argv[++i];
        else if (strcmp(argv[i], "-v") == 0)
            ++verbosity;
        else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0)
        {
            Printf("Limpet ssh " LIMPET_SSH_VERSION " (libssh %s, aros-crypto %ld, entropy from %s)\n",
                   (IPTR)ssh_version(0), (long)AROS_CRYPTO_VERSION, (IPTR)aros_crypto_entropy_source());
            return RETURN_OK;
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "?") == 0 || (argv[i][0] == '-' && !host))
        {
            usage();
            return strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "?") == 0 ? RETURN_OK : RETURN_WARN;
        }
        else if (!host)
        {
            char *at = strrchr(argv[i], '@');
            if (at)
            {
                *at = 0;
                user = argv[i];
                host = at + 1;
            }
            else
            {
                host = argv[i];
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
    if (!host || !*host || port < 0 || port > 65535)
    {
        usage();
        return RETURN_WARN;
    }

    if (!SocketBase)
    {
        Printf("ssh: no network: bsdsocket.library did not open\n");
        return RETURN_FAIL;
    }

    /* libssh reads socket errors from the C library's errno (EINPROGRESS,
     * EAGAIN); bsdsocket.library stores them there once told where it is. */
    {
        struct TagItem tags[] = {
            { SBTM_SETVAL(SBTC_ERRNOLONGPTR), (IPTR)&errno },
            { TAG_END, 0 }
        };
        if (sizeof(errno) != sizeof(LONG) || SocketBaseTagList(tags) != 0)
        {
            Printf("ssh: bsdsocket.library did not accept the error variable\n");
            return RETURN_FAIL;
        }
    }

#ifdef XT_SSH_ABIV11
    /* On ABIv11 bsdsocket numbers its descriptors from 0, apart from the C
     * library's. libssh's poll emulation takes descriptor 0 for "none" (on
     * Unix it is never a socket), so that number is kept busy here. The
     * library closes it when the program ends. */
    (void)socket(AF_INET, SOCK_STREAM, 0);
#endif

    if (verbosity)
    {
        ssh_set_log_callback(log_line);
        ssh_set_log_level(verbosity == 1 ? SSH_LOG_PROTOCOL : verbosity == 2 ? SSH_LOG_PACKET : SSH_LOG_FUNCTIONS);
    }

    /* The cryptography is the aros-crypto kit; libssh seeds its random
     * generator from the kit's entropy source and fails without one. */
    aros_crypto_setup();
    if (ssh_init() != SSH_OK)
    {
        Printf("ssh: no usable source of random numbers (%s); nothing was sent\n",
               (IPTR)aros_crypto_entropy_source());
        aros_crypto_teardown();
        return RETURN_FAIL;
    }
    session = ssh_new();
    if (!session)
    {
        Printf("ssh: out of memory\n");
        goto finish;
    }
    memset(&callbacks, 0, sizeof callbacks);
    callbacks.auth_function = ask_passphrase;
    ssh_callbacks_init(&callbacks);
    ssh_set_callbacks(session, &callbacks);

    /* Options given here win over config; config wins over the defaults. */
    /* libssh's own defaults are all "<home>/.ssh/..."; the drawer is named
     * here file by file so that it can be ENVARC:SSH. */
    ssh_directory(directory, sizeof directory);
    ssh_options_set(session, SSH_OPTIONS_SSH_DIR, directory);
    snprintf(path, sizeof path, "%s/known_hosts", directory);
    ssh_options_set(session, SSH_OPTIONS_KNOWNHOSTS, path);
    for (i = 0; i < 3; ++i) /* the last one added is tried first */
    {
        static const char *const names[] = { "id_rsa", "id_ecdsa", "id_ed25519" };
        snprintf(path, sizeof path, "%s/%s", directory, names[i]);
        ssh_options_set(session, SSH_OPTIONS_ADD_IDENTITY, path);
    }
    ssh_options_set(session, SSH_OPTIONS_HOST, host);
    ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeout);
    if (port) ssh_options_set(session, SSH_OPTIONS_PORT, &port);
    if (user) ssh_options_set(session, SSH_OPTIONS_USER, user);
    snprintf(path, sizeof path, "%s/config", directory);
    if (ssh_options_parse_config(session, path) != SSH_OK)
    {
        Printf("ssh: %s: %s\n", (IPTR)path, (IPTR)ssh_get_error(session));
        goto finish;
    }
    /* No account names on AROS: without -l, user@ or a User line in config,
     * the USER variable, then a question. */
    if (ssh_options_get(session, SSH_OPTIONS_USER, &known_user) != SSH_OK || !known_user || !*known_user)
    {
        if (GetVar("USER", name, sizeof name, 0) <= 0 &&
            (ask("User name on the server: ", name, sizeof name, 1) != 0 || !*name))
        {
            Printf("ssh: no user name; give one with -l or as user@host\n");
            goto finish;
        }
        ssh_options_set(session, SSH_OPTIONS_USER, name);
    }
    if (known_user) ssh_string_free_char(known_user);
    {
        unsigned int chosen = 0;
        ssh_options_get_port(session, &chosen);
        port = (int)chosen;
    }

    if (ssh_connect(session) != SSH_OK)
    {
        Printf("ssh: cannot connect to %s, port %ld: %s\n",
               (IPTR)host, (long)port, (IPTR)ssh_get_error(session));
        goto finish;
    }
    if (verify_host(session, host, port) != 0)
        goto disconnect;
    if (authenticate(session, keyfile) != 0)
    {
        Printf("ssh: login was not accepted\n");
        goto disconnect;
    }

    channel = ssh_channel_new(session);
    if (!channel || ssh_channel_open_session(channel) != SSH_OK)
    {
        Printf("ssh: cannot open a session: %s\n", (IPTR)ssh_get_error(session));
        goto disconnect;
    }
    exit_code = xt_ssh_stream(session, channel, command);
    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);

disconnect:
    if (channel) ssh_channel_free(channel);
    ssh_disconnect(session);
finish:
    if (session) ssh_free(session);
    ssh_finalize();
    aros_crypto_teardown();
    return exit_code;
}
