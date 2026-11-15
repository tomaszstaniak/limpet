/*
 * Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Author: Fabian Schmieder (@metaneutrons)
 *
 * The part of the SSH client that scp and ssh-copy-id use as well: the SSH
 * drawer, questions to the user, the server's key, login. Taken out of
 * client.c (derived from mainline AROS workbench/network/sshd/ssh.c).
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
#include "common.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* libnet owns startup/shutdown of bsdsocket.library. */
extern struct Library *SocketBase;

static const char *tool = "ssh";
static int started;

void xt_ssh_tool(const char *name) { tool = name; }
const char *xt_ssh_tool_name(void) { return tool; }

/* Where keys, known_hosts and config are: HOME/.ssh when the HOME variable
 * is set, as on other systems; otherwise ENVARC:SSH. */
void xt_ssh_directory(char *path, size_t size)
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

/* libssh makes missing drawers with the C library; the SSH drawer is made
 * the AmigaDOS way first so that a failure is seen where it happens. */
void xt_ssh_make_directory(void)
{
    char directory[300];
    BPTR lock;
    xt_ssh_directory(directory, sizeof directory);
    if ((lock = Lock(directory, SHARED_LOCK)) || (lock = CreateDir(directory))) UnLock(lock);
}

/* One line from the user. With echo off the terminal is read key by key and
 * nothing is shown. 0 on success, -1 when there is no user to ask (input is
 * a file), on Ctrl-C or at end of input. */
int xt_ssh_ask(const char *prompt, char *answer, size_t size, int echo)
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
int xt_ssh_ask_passphrase(const char *prompt, char *buffer, size_t size, int echo, int verify, void *userdata)
{
    char text[300];
    (void)verify; (void)userdata;
    snprintf(text, sizeof text, "%s: ", prompt ? prompt : "Passphrase");
    return xt_ssh_ask(text, buffer, size, echo);
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
        Printf("%s: cannot read the server's key: %s\n", (IPTR)tool, (IPTR)ssh_get_error(session));
        goto done;
    }
    switch (ssh_session_is_known_server(session)) {
    case SSH_KNOWN_HOSTS_OK:
        result = 0;
        break;
    case SSH_KNOWN_HOSTS_CHANGED:
        Printf("%s: THE KEY OF %s HAS CHANGED.\n", (IPTR)tool, (IPTR)host);
        Printf("It is now %s %s.\n", (IPTR)type, (IPTR)fingerprint);
        Printf("Either the server was set up again or someone is answering in its place.\n");
        Printf("Not connected. If the change is expected, remove the old line for this\n");
        Printf("host from known_hosts.\n");
        break;
    case SSH_KNOWN_HOSTS_OTHER:
        Printf("%s: %s is known with a key of another type; the server now offers\n", (IPTR)tool, (IPTR)host);
        Printf("%s %s.\n", (IPTR)type, (IPTR)fingerprint);
        Printf("Not connected: this can be an attempt to make you accept a new key.\n");
        break;
    case SSH_KNOWN_HOSTS_NOT_FOUND:
    case SSH_KNOWN_HOSTS_UNKNOWN:
        if (port == XT_SSH_DEFAULT_PORT) Printf("%s is not a known host.\n", (IPTR)host);
        else Printf("%s, port %ld, is not a known host.\n", (IPTR)host, (long)port);
        Printf("Its %s key has the fingerprint %s.\n", (IPTR)type, (IPTR)fingerprint);
        if (xt_ssh_ask("Connect and remember this key (yes/no)? ", answer, sizeof answer, 1) != 0) {
            Printf("%s: the key cannot be confirmed without a terminal; not connected\n", (IPTR)tool);
            break;
        }
        if (strcmp(answer, "yes") != 0 && strcmp(answer, "YES") != 0 && strcmp(answer, "Yes") != 0) {
            Printf("Not connected.\n");
            break;
        }
        xt_ssh_make_directory();
        if (ssh_session_update_known_hosts(session) != SSH_OK)
            Printf("%s: the key was not saved (%s); you will be asked again\n", (IPTR)tool, (IPTR)ssh_get_error(session));
        result = 0;
        break;
    default:
        Printf("%s: cannot check known_hosts: %s\n", (IPTR)tool, (IPTR)ssh_get_error(session));
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
            rc = ssh_pki_import_privkey_file(keyfile, NULL, xt_ssh_ask_passphrase, NULL, &key);
            if (rc == SSH_OK) {
                rc = ssh_userauth_publickey(session, NULL, key);
                ssh_key_free(key);
                if (rc == SSH_AUTH_SUCCESS) return 0;
            } else {
                Printf("%s: cannot read the key %s\n", (IPTR)tool, (IPTR)keyfile);
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
                    if (xt_ssh_ask(prompt, answer, sizeof answer, echo) != 0) return -1;
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
            if (xt_ssh_ask("Password: ", answer, sizeof answer, 0) != 0) return -1;
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
    Printf("%s[%ld] %s: %s\n", (IPTR)tool, (long)priority, (IPTR)function, (IPTR)buffer);
}
int xt_ssh_start(int network, int verbosity)
{
    if (network)
    {
        /* libssh reads socket errors from the C library's errno (EINPROGRESS,
         * EAGAIN); bsdsocket.library stores them there once told where it is. */
        /* By default the socket calls of a task with a pending Ctrl-C end
         * at once with EINTR, and libssh then repeats them without end. The
         * programs look at Ctrl-C themselves, so that mask is cleared. */
        struct TagItem tags[] = {
            { SBTM_SETVAL(SBTC_ERRNOLONGPTR), (IPTR)&errno },
            { SBTM_SETVAL(SBTC_BREAKMASK), 0 },
            { TAG_END, 0 }
        };
        if (!SocketBase)
        {
            Printf("%s: no network: bsdsocket.library did not open\n", (IPTR)tool);
            return -1;
        }
        if (sizeof(errno) != sizeof(LONG) || SocketBaseTagList(tags) != 0)
        {
            Printf("%s: bsdsocket.library did not accept the program's settings\n", (IPTR)tool);
            return -1;
        }
#ifdef XT_SSH_ABIV11
        /* On ABIv11 bsdsocket numbers its descriptors from 0, apart from the C
         * library's. libssh's poll emulation takes descriptor 0 for "none" (on
         * Unix it is never a socket), so that number is kept busy here. The
         * library closes it when the program ends. */
        (void)socket(AF_INET, SOCK_STREAM, 0);
#endif
    }
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
        Printf("%s: no usable source of random numbers (%s); nothing was done\n",
               (IPTR)tool, (IPTR)aros_crypto_entropy_source());
        aros_crypto_teardown();
        return -1;
    }
    started = 1;
    return 0;
}

void xt_ssh_stop(void)
{
    if (!started) return;
    ssh_finalize();
    aros_crypto_teardown();
    started = 0;
}

/* Ctrl-C pressed while connecting or logging in: say so once and leave the
 * signal for the caller, which ends without doing its work. */
static int break_pending(void)
{
    static int told;
    if (!(SetSignal(0, 0) & SIGBREAKF_CTRL_C)) return 0;
    if (!told) Printf("%s: stopped\n", (IPTR)tool);
    told = 1;
    return 1;
}

ssh_session xt_ssh_connect(const struct xt_ssh_login *login)
{
    /* The callbacks must live as long as the session. */
    static struct ssh_callbacks_struct callbacks;
    char directory[300], path[340], name[128];
    char *known_user = NULL;
    ssh_session session = ssh_new();
    int port = login->port;
    long timeout = 30;
    int i;

    if (!session)
    {
        Printf("%s: out of memory\n", (IPTR)tool);
        return NULL;
    }
    memset(&callbacks, 0, sizeof callbacks);
    callbacks.auth_function = xt_ssh_ask_passphrase;
    ssh_callbacks_init(&callbacks);
    ssh_set_callbacks(session, &callbacks);

    /* Options given here win over config; config wins over the defaults.
     * libssh's own defaults are all "<home>/.ssh/..."; the drawer is named
     * here file by file so that it can be ENVARC:SSH. */
    xt_ssh_directory(directory, sizeof directory);
    ssh_options_set(session, SSH_OPTIONS_SSH_DIR, directory);
    snprintf(path, sizeof path, "%s/known_hosts", directory);
    ssh_options_set(session, SSH_OPTIONS_KNOWNHOSTS, path);
    for (i = 0; i < 3; ++i) /* the last one added is tried first */
    {
        static const char *const names[] = { "id_rsa", "id_ecdsa", "id_ed25519" };
        snprintf(path, sizeof path, "%s/%s", directory, names[i]);
        ssh_options_set(session, SSH_OPTIONS_ADD_IDENTITY, path);
    }
    ssh_options_set(session, SSH_OPTIONS_HOST, login->host);
    ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeout);
    if (port) ssh_options_set(session, SSH_OPTIONS_PORT, &port);
    if (login->user) ssh_options_set(session, SSH_OPTIONS_USER, login->user);
    snprintf(path, sizeof path, "%s/config", directory);
    if (ssh_options_parse_config(session, path) != SSH_OK)
    {
        Printf("%s: %s: %s\n", (IPTR)tool, (IPTR)path, (IPTR)ssh_get_error(session));
        goto fail;
    }
    /* No account names on AROS: without a user name from the command line or
     * a User line in config, the USER variable, then a question. */
    if (ssh_options_get(session, SSH_OPTIONS_USER, &known_user) != SSH_OK || !known_user || !*known_user)
    {
        if (GetVar("USER", name, sizeof name, 0) <= 0 &&
            (xt_ssh_ask("User name on the server: ", name, sizeof name, 1) != 0 || !*name))
        {
            Printf("%s: no user name; give one as user@host\n", (IPTR)tool);
            if (known_user) ssh_string_free_char(known_user);
            goto fail;
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
        if (break_pending()) goto fail;
        Printf("%s: cannot connect to %s, port %ld: %s\n",
               (IPTR)tool, (IPTR)login->host, (long)port, (IPTR)ssh_get_error(session));
        goto fail;
    }
    if (break_pending()) goto disconnect;
    if (verify_host(session, login->host, port) != 0)
        goto disconnect;
    if (authenticate(session, login->keyfile) != 0)
    {
        if (!break_pending()) Printf("%s: login was not accepted\n", (IPTR)tool);
        goto disconnect;
    }
    if (break_pending()) goto disconnect;
    return session;

disconnect:
    ssh_disconnect(session);
fail:
    ssh_free(session);
    return NULL;
}

void xt_ssh_disconnect(ssh_session session)
{
    if (!session) return;
    ssh_disconnect(session);
    ssh_free(session);
}
