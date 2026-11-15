#ifndef XTERM_SSH_COMMON_H
#define XTERM_SSH_COMMON_H
/* What ssh, scp, ssh-keygen and ssh-copy-id share: the SSH drawer, questions
 * to the user, start of the cryptography and of the network, and the way to
 * a logged-in session. */
#include <stddef.h>
#include <libssh/libssh.h>

#define XT_SSH_DEFAULT_PORT 22

struct xt_ssh_login {
    const char *host;
    const char *user;    /* NULL: config, the USER variable, then a question */
    const char *keyfile; /* NULL: the keys of config or of the SSH drawer */
    int port;            /* 0: config, then 22 */
};

/* The program's name in messages ("scp: ..."). */
void xt_ssh_tool(const char *name);
const char *xt_ssh_tool_name(void);

/* HOME/.ssh when the HOME variable is set, otherwise ENVARC:SSH.
 * xt_ssh_make_directory() creates it when it is missing. */
void xt_ssh_directory(char *path, size_t size);
void xt_ssh_make_directory(void);

/* One line from the user; without echo nothing typed is shown. 0, or -1
 * when there is no user to ask, on Ctrl-C or at end of input. */
int xt_ssh_ask(const char *prompt, char *answer, size_t size, int echo);
/* libssh's callback for the passphrase of a private key. */
int xt_ssh_ask_passphrase(const char *prompt, char *buffer, size_t size, int echo, int verify, void *userdata);

/* Cryptography (aros-crypto, then libssh). With network: bsdsocket.library
 * too. verbosity > 0 prints libssh's log. 0, or -1 after a message. */
int xt_ssh_start(int network, int verbosity);
void xt_ssh_stop(void);

/* Connects, checks the server's key against known_hosts (asking about an
 * unknown one) and logs in. NULL after a message. */
ssh_session xt_ssh_connect(const struct xt_ssh_login *login);
void xt_ssh_disconnect(ssh_session session);

#endif
