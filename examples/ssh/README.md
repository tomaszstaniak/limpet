# SSH client

`ssh` is the SSH client that comes with Limpet since 0.11.0, for mainline AROS (x86_64 and Raspberry Pi) and ABIv11 distributions. It is built on libssh 0.12.2 and the [aros-crypto](https://github.com/tomaszstaniak/aros-crypto) kit (Mbed TLS 3.6.7).

## Use

    ssh [-v] [-i keyfile] [-p port] [-l user] [user@]host [command]

- `ssh user@host` opens a remote shell in the Limpet window. The remote side gets the window's size and follows resizes; Ctrl-C and Ctrl-D go to the remote program. Leave with `exit`; the remote shell's return code comes back to the AROS Shell.
- `ssh user@host "command"` runs one command. Its output can be redirected (`ssh >file ...`); local input is not sent to it.
- Without a user name (`-l`, `user@`, a `User` line in `config`, or the `USER` variable) `ssh` asks for one.
- `-v` prints libssh's log; repeat it for more detail.

## Keys and known hosts

The SSH drawer is `ENVARC:SSH`, or `HOME/.ssh` when the `HOME` variable is set. Its files have the formats of OpenSSH:

| File | Purpose |
| --- | --- |
| `id_ed25519`, `id_ecdsa`, `id_rsa` | your private keys, tried in this order; a passphrase is asked for when a key has one |
| `known_hosts` | the keys of servers you have accepted |
| `config` | per-host settings (`Host`, `HostName`, `User`, `Port`, `IdentityFile`, ...) |

This version does not make keys. Make one on another computer (`ssh-keygen -t ed25519`), put the public half on the server and copy the private file into the drawer.

The first time a server is contacted, `ssh` shows the fingerprint of its key and asks. Compare it with the fingerprint the server's administrator gives you; only `yes` accepts. A server whose key differs from the remembered one is refused: either it was set up again, or someone is answering in its place. Remove its line from `known_hosts` only when you know the reason.

Login is tried with a key, then with the server's questions (keyboard-interactive), then with a password. What you type as a password or passphrase is not shown.

## Random numbers

Encryption needs unpredictable numbers. On mainline AROS they come from the system (`entropy.resource`). ABIv11 has no such source; there the kit uses jitterentropy, which measures timing differences of the processor and tests its timer at start. If the source fails, `ssh` says so and stops before anything is sent.

## Limits

- No file transfer (scp, sftp), port forwarding, agent, key generation or IPv6. On ABIv11 host names are looked up as IPv4 addresses through `bsdsocket.library`.
- Use it in a Limpet window. In the system console (`CON:`) it was not tested; window size and colours are not available there.
- One command's output is passed through as bytes; in an interactive session the remote program's basic colours are rewritten so that Limpet shows them as colours and not as Workbench pens.
- When the connection is lost, `ssh` ends with return code 20. It does not reconnect.
- Mainline nightly build of 2026-11-13 under QEMU with the e1000 network card: network traffic can stop the whole system. This also happens with a network test that does not use `ssh`.

## Build

    AROS_GCC=<compiler> AROS_SDK=<SDK> AROS_CRYPTO=<aros-crypto kit> sh tools/build-ssh.sh <target>

`<target>` is `aros-x86_64-abiv1-pc`, `aros-aarch64-abiv1-raspi` or `aros-x86_64-abiv11-pc`; the kit is the one built for the same target. The script downloads libssh 0.12.2 from its release site, checks the SHA-256, applies `tools/ssh/libssh-aros.patch` and builds `build/<target>/ssh/ssh`.

What the patch changes in libssh, and why:

- the home directory comes from the `HOME` variable or the program's drawer, not from an account database;
- position-independent code and compiler thread-local storage are off (the AROS loader has neither relocation kind);
- libssh does not initialise itself before `main`, because the program sets the kit up first;
- a failed seeding of the random generator is an error (upstream goes on without one);
- on ABIv11, sockets are closed and switched with `bsdsocket.library` calls, because its C library does not know socket descriptors.

`tools/ssh/aros-abiv11.c` holds what ABIv11's C library lacks. It is not compiled into the mainline builds.

Host tests: `make test` runs `test_ssh_pump`, `test_ssh_term_query` and `test_ssh_sgr`.

## Licences

`client.c` derives from mainline AROS `workbench/network/sshd/ssh.c` and is under the AROS Public License (`licenses/AROS-APL.txt`); the other files here are MIT like the rest of Limpet. libssh is LGPL 2.1 (`licenses/libssh-COPYING.txt`) and is linked statically: this source and the build script are what you need to link it again with a changed libssh.
