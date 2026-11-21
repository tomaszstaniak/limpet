# Verification record: Limpet 0.13.0

Checks of 2026-11-21. All targets were built from the same source with
version 0.13.0 and date 21.11.2026, with the toolchains, SDKs and aros-crypto
kit 1 of 0.12.1. `make test` passes, with new tests for the argument
definitions (`tests/test_completion_args.c`), the `prompt` setting and the
prompt line. All runtime checks used QEMU and the released archives.

## What changed

- A prompt chosen in Preferences (Shell page), kept in `Shell.conf`.
- TAB offers a known command's keywords, only drawers for drawer
  arguments, and allowed words; 43 command definitions and one for apkg are
  built in, and files in `ENVARC:Limpet/Completions` add more.
- The SSH programs run on a 512 KB stack of their own.
- Ctrl-C ends a wait for the server in `ssh`, `scp` and `ssh-copy-id`.

## Where the built-in definitions come from

Each command was run with `?` and its input from `NIL:`, which prints the
command's template, on the mainline nightly of 2026-11-15 (x86_64) and on
AROS One 1.3 (ABIv11). 44 commands were read; `Why` has no arguments and
got no definition. The templates are equal on both systems except for
`Assign`, `List`, `Mount` and `Protect`, where mainline has one or two more
keywords; the definitions hold the common part. Which arguments take a
drawer (`CD`, `MakeDir`, `Dir`, `Assign`, `Path`, `Lock`, `Relabel`,
`AddBuffers`) was written by hand. The apkg commands are those of
`apkg --help` in apkg 0.7.8.

## What each system ran

Systems: AROS One 1.3 (x86_64 ABIv11), the mainline nightly of 2026-11-15
(x86_64 ABIv1), the Raspberry Pi nightly of 2026-11-10 (AArch64 ABIv1,
QEMU's raspi3b model). The SSH programs were started from the system's own
Shell with its default stack of 40960 bytes, against OpenSSH on the host
computer, which negotiates the ML-KEM key exchange.

| Check | AROS One 1.3 | mainline | Raspberry Pi |
|---|---|---|---|
| `ssh host command` with a key | passed | passed | passed |
| `scp` up, down and up again, 60000 bytes, the three copies equal by SHA-256 | passed | passed | passed |
| Ctrl-C while `ssh` waits for a server that accepts the connection and says nothing | `ssh: stopped` at once | the same | the same |
| Ctrl-C while `scp` waits for a file service that does not answer | `scp: stopped` within seconds | the same | the same |
| Coloured prompt set through `Shell.conf`, return code after a failed command | passed | passed | passed |
| `Copy data.txt t`, TAB | `TO` first, then volumes and files | the same | the same |
| `CD d`, TAB: volumes and the drawer `docs/`, no files, not the argument's own name `DIR` | passed | passed | no files; `DIR` was still offered (earlier build) |
| `apkg re`, TAB twice | `recent`, `remove` | the same | the same |
| Own definition in `ENVARC:Limpet/Completions`: allowed words, drawers only | passed | passed | passed |
| Preferences: the Prompt choice shows the saved value; Use writes `prompt=` to `ENV:Limpet/Shell.conf` | not run | passed (test build) | not run |

AROS One and mainline ran the released archives. "Test build" is a mainline
archive built earlier the same day, before two changes to the order of
TAB's matches. The Raspberry Pi ran the terminal built before the last of
them (the name of the argument being typed is no longer offered); its SSH
programs are those of the released archive. The released Pi archive was
not run again.

Before the stack change, the first `ssh` login from the system's Shell on
mainline ended in the "Stack extends out of range" requester, inside the
ML-KEM code.

## Not checked

- The message that names an unfinished copy left on the server: the test
  server stopped answering before the file was opened, so no temporary file
  existed.
- The released Raspberry Pi archive (see above).
- A stalled server in the middle of a file; the checks above stall before
  the first block.
- `ssh-keygen` and `ssh-copy-id` were not run again. Their code changed only
  in the way `main` is entered.
- Large transfers. The known issue with QEMU's e1000 card stands.
- The console handler archive: built, not run; its code is unchanged.
- Installation through apkg is checked after publication.
- Real hardware.
