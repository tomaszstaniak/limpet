# SSH programs

Limpet comes with four SSH programs for mainline AROS (x86_64 and Raspberry Pi) and ABIv11 distributions: `ssh` (since 0.11.0), and `scp`, `ssh-keygen` and `ssh-copy-id` (since 0.12.1). They are built on libssh 0.12.2 and the [aros-crypto](https://github.com/tomaszstaniak/aros-crypto) kit (Mbed TLS 3.6.7).

## First login with a key

    ssh-keygen
    ssh-copy-id user@host
    ssh user@host

`ssh-keygen` makes a key pair in the SSH drawer and asks for a passphrase. `ssh-copy-id` logs in with your password once and adds the public key to your account on the server. From then on `ssh` and `scp` log in with the key.

## ssh

    ssh [-v] [-i keyfile] [-p port] [-l user] [user@]host [command]

- `ssh user@host` opens a remote shell in the Limpet window. The remote side gets the window's size and follows resizes; Ctrl-C and Ctrl-D go to the remote program. Leave with `exit`; the remote shell's return code comes back to the AROS Shell.
- `ssh user@host "command"` runs one command. Its output can be redirected (`ssh >file ...`); local input is not sent to it.
- Without a user name (`-l`, `user@`, a `User` line in `config`, or the `USER` variable) `ssh` asks for one.
- `-v` prints libssh's log; repeat it for more detail.

## scp

    scp [-r] [-q] [-v] [-i keyfile] [-P port] source... target

Copies files between AROS and one server, through the server's SFTP service.

    scp notes.txt user@host:                 into the home directory there
    scp #?.txt user@host:documents           several files into a directory
    scp user@host:/etc/motd RAM:             from the server
    scp -r Work:project user@host:backup     a drawer with its contents
    scp -r user@host:project Work:           a directory with its contents

- The server side is written `[user@]host:path`. A path without a leading `/` starts in your home directory there.
- AROS paths have a colon as well. The text before the first colon is taken as a server unless it is a volume, device or assign on this computer; with `user@` in front it is always a server.
- Local sources may be AmigaDOS patterns. Patterns on the server side are not expanded: name the files or copy the directory.
- `-r` copies drawers and directories. Copying into an existing directory puts the source inside it; otherwise the target gets the source's contents.
- Use `""` as the target for the current drawer.
- In a Limpet window a large file shows its percentage; each finished file gets one line. `-q` prints errors only. Ctrl-C stops the copy.
- A file arrives under a temporary name (`scp-....part` on AROS, `.<name>.scp-part` on the server) and replaces the target only when it is complete. A copy that is stopped or fails leaves the file that was there before as it was. If the computer or the connection stops in the middle, the temporary file can remain. `scp` names one it could not remove from the server; delete it, or upload the file again, which writes over it.
- Two server names that would be one name on AROS (`a:b.txt` and `a_b.txt`, or `Foo` and `foo`) are not written over each other: the first is copied, the second is reported and the return code is 10. Copy the second by itself with another target name. The same holds for two sources with one name.
- New files on the server get mode 644 and directories 755; a file that replaces one keeps the old one's mode. Dates and protection bits are not carried over. A `:` in a server file name becomes `_` on AROS. Links to files are copied as files; links to directories are not followed.
- The return code is 0 when everything was copied, 10 when something was not, 20 when the connection or login failed.

## ssh-keygen

    ssh-keygen [-t ed25519|ecdsa|rsa] [-b bits] [-f file] [-C comment] [-N passphrase]
    ssh-keygen -l [-f file]
    ssh-keygen -y [-f file]

Without options it makes `id_ed25519` and `id_ed25519.pub` in the SSH drawer. `-t ecdsa` (256, 384 or 521 bits) and `-t rsa` (2048 to 8192 bits, 3072 unless `-b` says otherwise) are for servers that need them; `-f` names another file. The passphrase is asked for twice and not shown; an empty one leaves the key unprotected. An existing key is never overwritten.

`-l` shows the fingerprint of a key, `-y` prints the public key of a private key (to make a lost `.pub` file again).

## ssh-copy-id

    ssh-copy-id [-i keyfile] [-p port] [-v] [user@]host

Adds a public key to `.ssh/authorized_keys` of your account on the server: the one named with `-i`, or the first of `id_ed25519.pub`, `id_ecdsa.pub`, `id_rsa.pub` in the SSH drawer. A key the server has already is not added twice. The server needs a POSIX shell.

## Keys and known hosts

The SSH drawer is `ENVARC:SSH`, or `HOME/.ssh` when the `HOME` variable is set. Its files have the formats of OpenSSH, so keys made elsewhere work here and keys made here work elsewhere:

| File | Purpose |
| --- | --- |
| `id_ed25519`, `id_ecdsa`, `id_rsa` | your private keys, tried in this order; a passphrase is asked for when a key has one |
| `id_ed25519.pub`, ... | their public halves, which servers get |
| `known_hosts` | the keys of servers you have accepted |
| `config` | per-host settings (`Host`, `HostName`, `User`, `Port`, `IdentityFile`, ...) |

With a `config` entry, `ssh name` and `scp file name:` use its host, port, user and key.

The first time a server is contacted, the program shows the fingerprint of its key and asks. Compare it with the fingerprint the server's administrator gives you; only `yes` accepts. A server whose key differs from the remembered one is refused: either it was set up again, or someone is answering in its place. Remove its line from `known_hosts` only when you know the reason.

Login is tried with a key, then with the server's questions (keyboard-interactive), then with a password. What you type as a password or passphrase is not shown.

## Random numbers

Encryption needs unpredictable numbers. On mainline AROS they come from the system (`entropy.resource`). ABIv11 has no such source; there the kit uses jitterentropy, which measures timing differences of the processor and tests its timer at start. If the source fails, the programs say so and stop before anything is sent or any key is made.

## Limits

- No port forwarding, agent, interactive `sftp` program or IPv6. On ABIv11 host names are looked up as IPv4 addresses through `bsdsocket.library`.
- Use the programs in a Limpet window. In the system console (`CON:`) they were not tested; window size and colours are not available there.
- One command's output is passed through as bytes; in an interactive session the remote program's basic colours are rewritten so that Limpet shows them as colours and not as Workbench pens.
- When the connection is lost, `ssh` ends with return code 20. It does not reconnect.
- Ctrl-C stops a program that is connecting or logging in at once. During a copy `scp` finishes the block in flight, removes its unfinished file and stops; when the server has not answered for two seconds it stops without waiting further. A name lookup that does not answer cannot be stopped.
- `scp` sends and receives one block at a time. Under QEMU's Raspberry Pi model sending reaches about 30 KB per second (receiving about 400 KB per second); on the PC targets 5 MB take a few seconds either way.

## Known system problems

On two configurations the whole system can stop responding while much data is sent, and an `scp` upload sends much data. Save your work before a large transfer.

- **ABIv11, `e1000.device` 1.1 (AROS One 1.3):** this driver has a known fault, memory allocation in interrupt code, corrected in AROS as `e1000.device` 1.2 ([deadwood2/AROS#274](https://github.com/deadwood2/AROS/issues/274)); no distribution includes that version yet. In our runs an upload stopped the system with 1.1, and the same transfers passed with 1.2 loaded.
- **Mainline x86_64, nightly build of 2026-11-13, e1000 card under QEMU:** the system stopped in most of our runs with SSH traffic (eleven of fifteen boots in the last round), in uploads and in downloads. It also stops under a network test that uses none of these programs. The cause is not established; whether it is the network driver is not known.

## Build

    AROS_GCC=<compiler> AROS_SDK=<SDK> AROS_CRYPTO=<aros-crypto kit> sh tools/build-ssh.sh <target>

`<target>` is `aros-x86_64-abiv1-pc`, `aros-aarch64-abiv1-raspi` or `aros-x86_64-abiv11-pc`; the kit is the one built for the same target. The script downloads libssh 0.12.2 from its release site, checks the SHA-256, applies `tools/ssh/libssh-aros.patch` and builds the four programs in `build/<target>/ssh/`.

What the patch changes in libssh, and why:

- the home directory comes from the `HOME` variable or the program's drawer, not from an account database;
- position-independent code and compiler thread-local storage are off (the AROS loader has neither relocation kind);
- libssh does not initialise itself before `main`, because the program sets the kit up first;
- a failed seeding of the random generator is an error (upstream goes on without one);
- on ABIv11, sockets are closed and switched with `bsdsocket.library` calls, because its C library does not know socket descriptors.

`tools/ssh/aros-abiv11.c` holds what ABIv11's C library lacks. It is not compiled into the mainline builds.

Host tests: `make test` runs `test_ssh_pump`, `test_ssh_term_query`, `test_ssh_sgr` and `test_ssh_scp_spec`.

## Licences

`client.c` and `common.c` derive from mainline AROS `workbench/network/sshd/ssh.c` and are under the AROS Public License (`licenses/AROS-APL.txt`); the other files here are MIT like the rest of Limpet. libssh is LGPL 2.1 (`licenses/libssh-COPYING.txt`) and is linked statically: this source and the build script are what you need to link it again with a changed libssh.
