# Verification record: Limpet 0.14.0

Checks of 2026-11-27 and 2026-11-28. All targets were built from the same
source with version 0.14.0 and date 27.11.2026: ABIv11 with the 2026.09
SDK, mainline x86_64 with the SDK of nightly 2026-11-24, the Raspberry Pi
with the Developer tree of the Pi nightly 2026-11-24, all with aros-crypto
kit 1. `make test` passes, with new tests for option lines, the program
path and `apkg run` in `tests/test_completion_args.c`. All runtime checks
used QEMU and the released archives.

## What changed

- Definitions describe Unix-style options (`option` lines); the size limit
  is 16384 bytes.
- A definition is looked for in `ENVARC:Limpet/Completions`, beside the
  program (`Completions/<command>`), in `SYS:Packages/<command>` and among
  the built-in ones; `apkg run <package>` uses the package's.
- After a completion with a single match the next TAB starts at the
  following word.
- The SSH programs are rebuilt with the new version number; their source
  is unchanged.

## Runtime checks

The same steps on each system, with eza 0.23.5 revision 3 and Fastfetch
2.70.0 revision 6 unpacked from their release archives:

| Step | AROS One 1.3 (ABIv11) | mainline x86_64, nightly 2026-11-24 | Raspberry Pi, nightly 2026-11-24 (QEMU raspi3b) |
| --- | --- | --- | --- |
| `eza --so` TAB gives `--sort`, the next TABs its values (drawer on the Shell's path) | pass | pass | pass |
| `eza --color=a` TAB gives `always`, one of three | pass | pass | not run |
| `fastfetch --logo-t` TAB, `s` TAB gives `--logo-type sixel` | pass (path typed) | pass (path typed) | pass (drawer on the path) |
| `apkg run eza --git-r` TAB with the definition in `SYS:Packages/eza` only | pass | pass | not run |
| `apkg --ro` TAB gives `--root` | pass | pass | not run |
| `Copy data.txt t` TAB gives `TO` | pass | pass | pass |
| `eza -l --sort size`, `fastfetch -c aros-compact` run | pass | pass | pass |

The definition in `SYS:Packages/eza` was put there by hand for the check
and removed afterwards.

## Not checked

- An upgrade from 0.13.x through apkg on a system with user settings.
- The Preferences program and the SSH programs, which did not change.
- A real Raspberry Pi.
