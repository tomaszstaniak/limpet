/*
 * ssh-keygen for Limpet: makes a key pair for SSH login, in the file formats
 * of OpenSSH. MIT, like the rest of Limpet.
 *
 * Usage: ssh-keygen [-t ed25519|ecdsa|rsa] [-b bits] [-f file] [-C comment] [-N passphrase]
 *        ssh-keygen -l [-f file]     the fingerprint of a key
 *        ssh-keygen -y [-f file]     the public key of a private key
 */

#include <proto/dos.h>
#include <exec/types.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <dos/var.h>

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
static const char __attribute__((used)) verstag[] = "\0$VER: ssh-keygen " LIMPET_SSH_VERSION " (" LIMPET_SSH_DATE ")\r\n";

/* Making a key needs no network. With SocketBase defined here the link
 * library that opens bsdsocket.library at start (and stops the program when
 * the network is not running) stays out; libssh's socket code is linked but
 * never called. */
struct Library *SocketBase;

static int exists(const char *path)
{
    BPTR lock = Lock((CONST_STRPTR)path, SHARED_LOCK);
    if (lock) UnLock(lock);
    return lock != 0;
}

static char *fingerprint_of(ssh_key key)
{
    unsigned char *hash = NULL;
    size_t size = 0;
    char *text = NULL;
    if (ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_SHA256, &hash, &size) == SSH_OK)
    {
        text = ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256, hash, size);
        ssh_clean_pubkey_hash(&hash);
    }
    return text;
}

/* The public half as one line: type, key, comment. */
static int public_line(ssh_key key, const char *comment, char *line, size_t size)
{
    char *encoded = NULL;
    const char *type = ssh_key_type(key) == SSH_KEYTYPE_ECDSA_P256 || ssh_key_type(key) == SSH_KEYTYPE_ECDSA_P384
        || ssh_key_type(key) == SSH_KEYTYPE_ECDSA_P521 ? ssh_pki_key_ecdsa_name(key) : ssh_key_type_to_char(ssh_key_type(key));
    int length;
    if (!type || ssh_pki_export_pubkey_base64(key, &encoded) != SSH_OK) return -1;
    length = snprintf(line, size, "%s %s%s%s\n", type, encoded, comment && *comment ? " " : "", comment ? comment : "");
    ssh_string_free_char(encoded);
    return length > 0 && (size_t)length < size ? 0 : -1;
}

/* The key in file: a private key (its passphrase is asked for), or a public
 * key file when only that can be read. */
static ssh_key load(const char *file, int need_private)
{
    ssh_key key = NULL;
    if (!need_private && ssh_pki_import_pubkey_file(file, &key) == SSH_OK) return key;
    if (ssh_pki_import_privkey_file(file, NULL, xt_ssh_ask_passphrase, NULL, &key) == SSH_OK) return key;
    return NULL;
}

static void usage(void)
{
    Printf("Usage: ssh-keygen [-t ed25519|ecdsa|rsa] [-b bits] [-f file] [-C comment] [-N passphrase]\n");
    Printf("       ssh-keygen -l [-f file]     show the fingerprint of a key\n");
    Printf("       ssh-keygen -y [-f file]     print the public key of a private key\n");
}

static int program(int argc, char **argv)
{
    const char *type_name = "ed25519", *file = NULL, *comment = NULL, *passphrase = NULL;
    char directory[300], path[400], public_path[410], line[4096], first[256], second[256], user[100];
    enum ssh_keytypes_e type = SSH_KEYTYPE_ED25519;
    ssh_pki_ctx context = NULL;
    ssh_key key = NULL;
    char *fingerprint = NULL;
    int bits = 0, show = 0, print_public = 0, i, result = RETURN_FAIL;
    BPTR out;

    xt_ssh_tool("ssh-keygen");
    for (i = 1; i < argc; ++i)
    {
        const char *a = argv[i];
        if (!strcmp(a, "-t") && i + 1 < argc) type_name = argv[++i];
        else if (!strcmp(a, "-b") && i + 1 < argc) bits = atoi(argv[++i]);
        else if (!strcmp(a, "-f") && i + 1 < argc) file = argv[++i];
        else if (!strcmp(a, "-C") && i + 1 < argc) comment = argv[++i];
        else if (!strcmp(a, "-N") && i + 1 < argc) passphrase = argv[++i];
        else if (!strcmp(a, "-l")) show = 1;
        else if (!strcmp(a, "-y")) print_public = 1;
        else if (!strcmp(a, "-V") || !strcmp(a, "--version"))
        {
            Printf("Limpet ssh-keygen " LIMPET_SSH_VERSION " (libssh %s, aros-crypto %ld, entropy from %s)\n",
                   (IPTR)ssh_version(0), (long)AROS_CRYPTO_VERSION, (IPTR)aros_crypto_entropy_source());
            return RETURN_OK;
        }
        else
        {
            usage();
            return !strcmp(a, "-h") || !strcmp(a, "?") ? RETURN_OK : RETURN_WARN;
        }
    }
    if (!strcmp(type_name, "ed25519")) type = SSH_KEYTYPE_ED25519;
    else if (!strcmp(type_name, "rsa")) type = SSH_KEYTYPE_RSA;
    else if (!strcmp(type_name, "ecdsa"))
        type = bits == 384 ? SSH_KEYTYPE_ECDSA_P384 : bits == 521 ? SSH_KEYTYPE_ECDSA_P521 : SSH_KEYTYPE_ECDSA_P256;
    else
    {
        Printf("ssh-keygen: the key type is ed25519, ecdsa or rsa\n");
        return RETURN_WARN;
    }
    if (type == SSH_KEYTYPE_RSA && !bits) bits = 3072;
    if ((type == SSH_KEYTYPE_RSA && (bits < 2048 || bits > 8192))
        || (!strcmp(type_name, "ecdsa") && bits && bits != 256 && bits != 384 && bits != 521)
        || (type == SSH_KEYTYPE_ED25519 && bits))
    {
        Printf("ssh-keygen: -b is 2048 to 8192 for rsa, 256, 384 or 521 for ecdsa, and not used for ed25519\n");
        return RETURN_WARN;
    }

    xt_ssh_directory(directory, sizeof directory);
    if (file) snprintf(path, sizeof path, "%s", file);
    else snprintf(path, sizeof path, "%s/id_%s", directory, type_name);

    if (xt_ssh_start(0, 0) != 0) return RETURN_FAIL;

    if (show || print_public)
    {
        key = load(path, print_public);
        if (!key)
        {
            Printf("ssh-keygen: cannot read a key from %s\n", (IPTR)path);
            goto done;
        }
        if (print_public)
        {
            if (public_line(key, comment, line, sizeof line)) goto failed;
            Printf("%s", (IPTR)line);
        }
        else
        {
            if (!(fingerprint = fingerprint_of(key))) goto failed;
            Printf("%s (%s)\n", (IPTR)fingerprint, (IPTR)ssh_key_type_to_char(ssh_key_type(key)));
        }
        result = RETURN_OK;
        goto done;
    }

    snprintf(public_path, sizeof public_path, "%s.pub", path);
    if (exists(path) || exists(public_path))
    {
        Printf("ssh-keygen: %s or its .pub file exists already; nothing was changed.\n", (IPTR)path);
        Printf("Name another file with -f, or delete the old key if you no longer use it.\n");
        result = RETURN_WARN;
        goto done;
    }
    if (!passphrase)
    {
        if (xt_ssh_ask("Passphrase for the new key (empty for none): ", first, sizeof first, 0) != 0)
        {
            Printf("ssh-keygen: no terminal to ask for a passphrase; give one with -N (-N \"\" for none)\n");
            goto done;
        }
        if (xt_ssh_ask("The same passphrase again: ", second, sizeof second, 0) != 0 || strcmp(first, second))
        {
            Printf("ssh-keygen: the two passphrases differ; no key was made\n");
            goto done;
        }
        passphrase = first;
    }
    if (!comment)
    {
        if (GetVar("USER", user, sizeof user - 6, 0) <= 0) strcpy(user, "user");
        strcat(user, "@aros");
        comment = user;
    }

    if (type == SSH_KEYTYPE_RSA)
    {
        Printf("Making the key; an RSA key takes a while...\n");
        Flush(Output());
        if (!(context = ssh_pki_ctx_new()) || ssh_pki_ctx_options_set(context, SSH_PKI_OPTION_RSA_KEY_SIZE, &bits) != SSH_OK)
            goto failed;
    }
    if (ssh_pki_generate_key(type, context, &key) != SSH_OK || !key) goto failed;
    if (public_line(key, comment, line, sizeof line)) goto failed;
    if (!(fingerprint = fingerprint_of(key))) goto failed;

    if (!file) xt_ssh_make_directory();
    if (ssh_pki_export_privkey_file(key, *passphrase ? passphrase : NULL, NULL, NULL, path) != SSH_OK)
    {
        Printf("ssh-keygen: cannot write %s\n", (IPTR)path);
        goto done;
    }
    out = Open((CONST_STRPTR)public_path, MODE_NEWFILE);
    if (!out || Write(out, line, (LONG)strlen(line)) != (LONG)strlen(line))
    {
        if (out) Close(out);
        Printf("ssh-keygen: cannot write %s\n", (IPTR)public_path);
        goto done;
    }
    Close(out);
    Printf("Private key: %s%s\n", (IPTR)path, (IPTR)(*passphrase ? "" : "  (no passphrase)"));
    Printf("Public key:  %s\n", (IPTR)public_path);
    Printf("Fingerprint: %s (%s)\n", (IPTR)fingerprint, (IPTR)ssh_key_type_to_char(ssh_key_type(key)));
    Printf("Keep the private key to yourself. To log in with this key, put the public\n");
    Printf("key on the server:  ssh-copy-id %s%s%suser@host\n",
           (IPTR)(file ? "-i " : ""), (IPTR)(file ? path : ""), (IPTR)(file ? " " : ""));
    result = RETURN_OK;
    goto done;

failed:
    Printf("ssh-keygen: the key could not be made\n");
done:
    memset(first, 0, sizeof first);
    memset(second, 0, sizeof second);
    if (fingerprint) ssh_string_free_char(fingerprint);
    if (key) ssh_key_free(key);
    if (context) ssh_pki_ctx_free(context);
    xt_ssh_stop();
    return result;
}

int main(int argc, char **argv)
{
    return xt_ssh_run(program, argc, argv);
}
