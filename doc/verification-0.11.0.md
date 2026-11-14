# Verification record: Limpet 0.11.0

Checks of 2026-11-14. All targets were built from the same source with
version 0.11.0 and date 14.11.2026, with the toolchains and SDKs of 0.10.1
(ABIv11: GCC 10.5 with the 2026.09 Developer SDK; mainline: the Developer
SDK from `74a2d80b`; the Pi: the AROS Clang 20.1 wrapper and its SDK), and
the aros-crypto kit 1 built for each target with the same compiler.
`make test` passes, including `test_ssh_sgr`. All runtime checks used QEMU.

Systems: AROS One 1.3 (x86_64 ABIv11); official mainline nightly 20261113
(x86_64 ABIv1); official Raspberry Pi nightly 20261110 (AArch64 ABIv1,
raspi3b model). The SSH servers were on the host computer, reached through
QEMU's user network: OpenSSH (public keys, a forced clean shell) and a small
test server for password and keyboard-interactive login.

## The 0.11.0 archives

Each release archive was unpacked in the guest and run from there, with
`HOME` pointing to a drawer in `RAM:` that held one key and a `known_hosts`
line for the server.

| Check | ABIv11 | mainline x86_64 | Pi |
| --- | --- | --- | --- |
| `Version` of `Limpet` and `ssh`: 0.11.0 | pass | pass | pass |
| `ssh -V` names libssh 0.12.2, aros-crypto 1 and the entropy source | jitterentropy | getentropy | getentropy |
| `ssh host "uname -sm; git log --oneline"`: output, return code 0 | pass | pass | pass |
| `ssh host "exit 7"`: return code 7 | pass | pass | pass |
| Interactive session in a Limpet window: Git colours, `exit`, return code 0 | pass | pass | pass |
| Remote Vim with syntax colours opened and left with `:q` | pass | pass | not run |
| `ENVARC:` listing before and after: identical | pass | pass | pass |

## The client before packaging

Run with builds of the same source before the version number was set
(`ssh -V` said 0.0); the only later source change is the version.

| Check | ABIv11 | mainline x86_64 | Pi |
| --- | --- | --- | --- |
| aros-crypto self-test, twice, return code 0 | pass | pass | pass |
| Unknown server: fingerprint shown, `yes` saves an OpenSSH `known_hosts` line, second run asks nothing | pass | pass | not run |
| Password login; a wrong password is asked again; nothing typed is shown | pass (one try) | pass | not run |
| Host name through DNS: `ssh git@github.com` shows GitHub's published Ed25519 fingerprint; `no` leaves with return code 20 | pass | pass | not run |
| Changed server key: refused, return code 20, the command not run | not run | pass | not run |
| Key with a passphrase (`-i`) | not run | pass | not run |
| Keyboard-interactive login with one shown and one hidden answer | not run | pass | not run |
| Default drawer `ENVARC:SSH` (no `HOME`): `known_hosts` created, key `id_ed25519` used; drawer deleted afterwards | not run | pass | not run |
| Connection closed by the server during a session: message, return code 20, Shell usable | not run | pass | not run |
| Server not listening; address that does not answer (30 s) | not run | pass | not run |

## Found and not fixed here

On the mainline nightly 20261113 (x86_64, e1000 under QEMU) the whole
system stopped responding three times during network traffic: in an
interactive SSH session, in a run of twenty SSH command sessions (during a
2.7 MB transfer), and in a network test without `ssh` (one TCP connection
sending both ways; its control run without a socket passed). The cause is
not established; the network driver's memory allocation in interrupt code is
the known candidate. Not seen on the other two systems in these checks.

## Not checked

A real Raspberry Pi; real PCs; servers other than OpenSSH on macOS, GitHub's
and the test server; RSA and ECDSA user keys; `config` files; long sessions;
the system console instead of a Limpet window.
