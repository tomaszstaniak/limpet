# Verification record: Limpet 0.12.1

Checks of 2026-11-15. All targets were built from the same source with
version 0.12.1 and date 15.11.2026, with the toolchains, SDKs and aros-crypto
kit 1 of 0.11.0. `make test` passes, including `test_ssh_scp_spec` with the
name-collision cases. All runtime checks used QEMU.

0.12.1 follows the unreleased 0.12.0 ([its record](verification-0.12.0.md)).
What changed in the programs since then:

- `scp` writes each file under a temporary name and renames it when it is
  complete, in both directions;
- `scp` refuses the second of two names that are one name on AROS, and the
  second of two sources with one name;
- Ctrl-C during connecting or login ends the four programs once the
  attempt is over.

Systems: AROS One 1.3 (x86_64 ABIv11) with the corrected `e1000.device`
1.2 loaded from `RAM:`; official mainline nightly 20261113 (x86_64 ABIv1);
official Raspberry Pi nightly 20261110 (AArch64 ABIv1, raspi3b model).
Servers on the host computer, reached through QEMU's user network: OpenSSH
with public keys and SFTP, and three more OpenSSH instances that break a
transfer on purpose: one ends the SFTP service three seconds after it
starts, one refuses to write past 512 KB, one is killed when it writes past
512 KB. The directory with colliding names was on a case-sensitive volume.

The terminal, its settings and the console handler have the code of 0.11.0
and were only started from each archive.

## What each system ran

The same script ran from the final archives on all three systems. On
ABIv11 and on the Pi it ran in one boot. On mainline the system stopped
responding in most boots (see below), so the script was cut into four parts
and each part was repeated until one boot completed it: keys, logins and
collisions; interrupted copies; copies and errors without the large
transfers; the large transfers.

## Copy safety (new in 0.12.1)

| Check | ABIv11 | mainline x86_64 | Pi |
| --- | --- | --- | --- |
| `scp -r` of a directory holding `a:b.txt`, `a_b.txt`, `Foo`, `foo`, `plain.txt`: three files arrive, the second of each pair is named in a message, return code 10; the files that arrived have the contents of their own originals | pass | pass | pass |
| Each of the four colliding files fetched by itself under another name: all four contents arrive and are sent back intact | pass | pass | pass |
| Two server sources with one name into a drawer: the first is kept, the second reported, return code 10 | pass | pass | pass |
| Two local sources with one name to a server directory: the first is kept, the second reported, return code 10 | pass | pass | pass |
| Download over an existing file, the server cut off after three seconds: return code 10, the old content is still there, no temporary file left | pass | pass | pass |
| Upload over an existing server file, the server refusing to write past 512 KB: return code 10, the old server file unchanged, no temporary file left | pass | pass | pass |
| Upload over an existing server file, the server killed at 512 KB: return code 10, the old server file unchanged; the temporary file `.<name>.scp-part` remains, as documented | pass | pass | pass |
| Upload over an existing server file with mode 600: replaced, mode still 600 | pass | pass | pass |
| Download over an existing file: replaced, no temporary file left | pass | pass | pass |
| Download over a file protected from deletion: return code 10, old content kept, no temporary file left | pass | pass | pass |
| By hand: Ctrl-C during a download over an existing file, with the percentage showing: "stopped", the old content kept, no temporary file left | pass (Limpet) | pass (system Shell; the build before the Ctrl-C change, same `scp.c`) | pass (Limpet) |
| By hand: Ctrl-C while `scp` and `ssh` wait for a server that accepts the connection and says nothing: "stopped", return code 20, within the 30-second timeout | pass | pass | not run |

## The checks of 0.12.0, repeated on the 0.12.1 archives

| Check | ABIv11 | mainline x86_64 | Pi |
| --- | --- | --- | --- |
| `Version` of the five programs: 0.12.1 | pass | pass | pass |
| `ssh host "uname -sm; exit 7"`: return code 7 | pass | pass | pass |
| `ssh-keygen`: five keys, each read by the host's `ssh-keygen -y`; existing file kept; `-l`, `-y`; unknown type refused | pass | pass | pass |
| `ssh-keygen` without a running network stack | pass | not run | not run |
| `ssh-copy-id` for four keys, repeated run, a file that is no public key | pass | pass | pass |
| `ssh -i` with each key, and with an RSA key made by OpenSSH; a `config` entry used by `ssh` and `scp` | pass | pass | pass |
| `scp`: one file, four programs by pattern, a drawer to a new and into an existing directory, 5 MB down and up, two server files, the current drawer, a directory down and up again; all compared on the host | pass | pass | pass |
| The ten error cases with their return codes | pass | pass | pass |
| `ENVARC:` listing before and after: identical | pass | pass | pass |

Not repeated on the 0.12.1 archives: the hand checks of 0.12.0 that the
changes do not touch (passphrase questions of `ssh-keygen`, `ssh-copy-id`
and `scp` with password login, a passphrase-protected key). They passed on
the 0.12.0 archives on all three systems, except the passphrase-protected
OpenSSH key, which was not run on the Pi.

## Failures seen, and what is known about them

- **Mainline x86_64: the whole system stops responding.** In eleven of
  fifteen boots with the 0.12.1 programs the guest stopped during SSH
  traffic: in uploads, in the three-second download of a large file, and
  once between two transfers. A new Shell could not be opened; the machine
  was powered off. The four boots that passed are the ones recorded above.
  The system also stops under a network test that uses no SSH program
  (0.11.0 record). The cause is not established: no dump was taken and no
  other network driver or card was tried. The host was under load from
  other virtual machines during these runs, which may matter.
  When the system stopped in an upload, the server afterwards held the
  unfinished `.<name>.scp-part` file and the old target file unchanged.
- **ABIv11 with the system's `e1000.device` 1.1** stopped in the first
  upload (0.12.0 record). That driver has a known fault
  ([deadwood2/AROS#274](https://github.com/deadwood2/AROS/issues/274),
  corrected in 1.2). With 1.2 loaded, four complete runs passed (two with
  the 0.12.0 archives, two with 0.12.1 builds). This supports the driver as the
  cause on ABIv11 and says nothing about mainline.
- **Pi: slow logins under host load.** With the host busy, an SSH login
  from the Pi model took more than a minute. Ctrl-C pressed in that time
  takes effect when the login step ends. In one run a download got no
  answer for 120 seconds and ended with "no answer from the server in
  time"; the old target was kept.

## Known limits of what was done

- Ctrl-C is looked at between blocks and between steps. While the programs
  wait for the server (up to 30 seconds when connecting, up to 120 seconds
  during a copy) it does not end the wait.
- The first of two colliding names wins; which one that is depends on the
  order in which the server lists the directory.
- The temporary file of a copy cut off by a lost connection or a stopped
  computer stays where it is; nothing removes it later.
- Not checked: real hardware; other network cards; installation through
  apkg; files larger than 2 GB written to a disk; the programs in the
  system console beyond the one interrupted `scp` above.

## Speed (final archives)

| System | 5 MB down | 5 MB up |
| --- | --- | --- |
| ABIv11 | 2 s | 2 s |
| mainline x86_64 | about 1 s | under 1 s |
| Pi (QEMU raspi3b, host under load) | 149 s | 109 s |
