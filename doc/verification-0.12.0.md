# Verification record: Limpet 0.12.0 (not released)

0.12.0 was tagged and not released: a review found that `scp` could write
one received file over another and could lose the old target of a stopped
copy. 0.12.1 corrects that; its record is
[verification-0.12.1.md](verification-0.12.1.md). This record stays as the
account of the 0.12.0 archives.

Checks of 2026-11-15. All targets were built from the same source with
version 0.12.0 and date 15.11.2026, with the toolchains, SDKs and aros-crypto
kit 1 of 0.11.0. `make test` passes, including `test_ssh_scp_spec`. All
runtime checks used QEMU.

Systems: AROS One 1.3 (x86_64 ABIv11); official mainline nightly 20261113
(x86_64 ABIv1); official Raspberry Pi nightly 20261110 (AArch64 ABIv1,
raspi3b model). The servers were on the host computer, reached through
QEMU's user network: OpenSSH with public keys and its SFTP service, and a
small test server with password login, commands and SFTP.

The terminal, its settings and the console handler have the code of 0.11.0
and were not run again beyond starting Limpet from each archive. `ssh` has
the same behaviour; its login code moved to `common.c` and is exercised by
every check below.

## The 0.12.0 archives

Each release archive was unpacked in the guest and run from there, with
`HOME` pointing to a drawer in `RAM:`. One script ran the same steps on all
three systems; on the Pi the two 5 MB transfers and the four-program upload
were left out of the last run (they passed there in an earlier run of the
same source, see below). Files sent were compared on the host with their
originals; files received were sent back and compared.

| Check | ABIv11 | mainline x86_64 | Pi |
| --- | --- | --- | --- |
| `Version` of `Limpet`, `ssh`, `scp`, `ssh-keygen`, `ssh-copy-id`: 0.12.0 | pass | pass | pass |
| `ssh host "uname -sm; exit 7"`: output, return code 7 | pass | pass | pass |
| `ssh-keygen`: Ed25519, ECDSA 256 and 521, RSA 3072, Ed25519 with a passphrase (`-N`); the host's `ssh-keygen -y` reads each private file and gives the same public key | pass | pass | pass |
| `ssh-keygen` on an existing file: nothing changed, return code 5; `-l` and `-y`; an unknown type refused | pass | pass | pass |
| `ssh-keygen` without a running network stack | pass | not run | not run |
| `ssh-copy-id` for each of the four keys; a second run reports the key as present; a file that is not a public key is refused | pass | pass | pass |
| `ssh -i` login with each key made in the guest, and with an RSA key made by OpenSSH | pass | pass | pass |
| `config` entry (`Host`, `HostName`, `Port`, `User`, `IdentityFile`) used by `ssh` and `scp` | pass | pass | pass |
| `scp` one file up; a pattern (`#?`) of four programs into a directory | pass | pass | one file |
| `scp -r` a drawer to a new directory and into an existing one | pass | pass | pass |
| `scp` 5 MB down, then up again, identical | pass | pass | earlier run |
| `scp` two server files into a drawer; a file into the current drawer (`""`, `~/`) | pass | pass | pass |
| `scp -r` a directory down and up again: nested directories, an empty file and directory, a name with a space, `a:b.txt` stored as `a_b.txt`, a link to a file copied, a link to a directory left out with a message | pass | pass | pass |
| Errors, with return codes: missing server file (10), missing local file (10), drawer or directory without `-r` (10), two servers or none (5), no permission (10), several files to a file (10), missing server directory (10), refused connection (20) | pass | pass | pass |
| `ENVARC:` listing before and after: identical | pass | pass | pass |

In a Limpet window, typed by hand:

| Check | ABIv11 | mainline x86_64 | Pi |
| --- | --- | --- | --- |
| `ssh-keygen` asks for the passphrase twice without showing it | pass | pass | pass |
| `ssh-copy-id` to the password server: unknown-host question, password, "now accepted" | pass | pass | pass |
| `ssh -i` with that key: passphrase asked, command run | pass | pass | pass |
| `scp` with password login | pass | pass | pass |
| `ssh -i` with a passphrase-protected ECDSA key made by OpenSSH | pass | pass | not run |
| `scp` of a 3 GB file to `NIL:`: percentage shown; Ctrl-C stops it, return code 10 | pass | pass | not run |

The first table is from the final archives on all three systems, and so is
the Ctrl-C row on mainline. The other rows of the second table are from
archives whose `scp` differs from the final one in the wording after Ctrl-C,
the text of one error and the time `scp` waits for an answer; `ssh`,
`ssh-keygen` and `ssh-copy-id` were byte-identical.

## Earlier runs of the same source

Before the version was set, the full script (with both 5 MB transfers and
the four-program upload) passed on mainline x86_64 and on the Pi.

## Failures seen, and what they were

- **ABIv11, system network driver.** With the system's `e1000.device` 1.1 an
  `scp` upload stopped the whole guest after 640 KB; a new Shell could not
  be opened. This driver allocates memory in interrupt code
  ([deadwood2/AROS#274](https://github.com/deadwood2/AROS/issues/274),
  corrected in 1.2). With the corrected driver loaded from `RAM:` before the
  network stack started, the complete script passed twice. The ABIv11
  results above are with that driver; nothing was installed.
- **Mainline x86_64.** In four of eight boots the whole guest stopped
  responding during SSH traffic: once when a connection was being opened,
  the others during or right after an upload. The other four boots passed
  the complete script. The same system stops under a network test that
  uses no SSH program (0.11.0 record); the cause is not established.
- **Pi, one run.** A 300 KB download and the directory listing after it got
  no answer for more than a minute; `scp` reported both with an empty
  reason, copied the remaining files and returned 10. The reason now reads
  "no answer from the server in time", and `scp` waits 120 seconds instead
  of 30 during transfers. It did not happen in the other three Pi runs.

## Speed

| System | 5 MB down | 5 MB up |
| --- | --- | --- |
| ABIv11 | about 1 s | 2 s |
| mainline x86_64 | about 1 s | about 1 s |
| Pi (QEMU raspi3b) | 12 s | 172 s |

## Not checked

- Real hardware; network cards other than QEMU's e1000 and the Pi model's
  USB network.
- Installation through apkg (see the catalogue record after publication).
- Files larger than 2 GB written to a disk, dates and protection bits
  (not carried over), server-side patterns (not supported).
- The programs in the system console (`CON:`).
