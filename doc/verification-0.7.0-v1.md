# Verification record: Limpet 0.7.0 for mainline x86_64 and Raspberry Pi

Limpet 0.7.0 was first released for AROS ABIv11 x86_64
([record](verification-0.7.0.md)). This record covers two more builds of the
same program for ABIv1 systems:

- **v1-x86_64**: mainline AROS, x86_64;
- **v1-aarch64**: AROS for the Raspberry Pi (AArch64, ABIv1), the native Pi
  system, not Macaros.

The C and C++ sources of the program, the handler and the worker are those
of tag `v0.7.0` (`a4cbf27`); `git diff v0.7.0 -- src tools/xterm_handler.c
tools/xterm-console/*.c` is empty. What was added is build files
(`mk/target*.mk`, a `TARGET=` switch in the Makefile), a packaging mode in
`tools/release/make-packages.sh` and a ReadMe per system. The ABIv11
packaging was rebuilt from the same tree and its archive contents matched
`dist/0.7.0` file for file.

## Build

Both builds start from an empty `build/<target>` directory; nothing is
shared between them or with the ABIv11 build (HarfBuzz is compiled again for
each target).

    make target TARGET=v1-x86_64 LIMPET_VERSION=0.7.0 LIMPET_DATE=24.10.2026 \
        T_CC=.../x86_64-aros-gcc T_SDK=.../AROS/Developer
    tools/release/make-packages.sh 0.7.0 OUT v1-x86_64
    make target TARGET=v1-aarch64 LIMPET_VERSION=0.7.0 LIMPET_DATE=24.10.2026 \
        T_ROOT=.../raspi-aarch64 T_CXX_INCLUDE=.../include/c++/v1
    tools/release/make-packages.sh 0.7.0 OUT v1-aarch64

`LIMPET_DATE` must be given: the version string carries the date, and two
builds on different days would differ.

| | v1-x86_64 | v1-aarch64 |
| --- | --- | --- |
| Compiler | mainline `x86_64-aros-gcc` 10.5.0 | AROS-patched Clang 20.1.0 (`aarch64-aros-clang`, `--target=aarch64-unknown-aros -mcmodel=large -ffixed-x18 -fno-pic`) |
| SDK | mainline build of AROS `74a2d80b` (2026-09-04), contrib `4b915533` | headers and link libraries of the official `AROS-20260822-raspi-aarch64-system.tar.bz2`, unmodified |
| C++ (HarfBuzz) | libstdc++ of the toolchain | the Pi SDK has no C++ library: HarfBuzz is compiled with `-fno-exceptions -fno-rtti` against libc++ headers only, and links only C library symbols |
| Target flags | handler worker `-DXTERM_DOS_HANDLER` only (mainline DOS keeps the handler's `fh_Type`, so no process-port forwarding) | as x86_64, plus `-DXTERM_NO_TASK_NOTIFY` (the Pi SDK has no `AddTaskNotifyHook`) and `-Wno-string-concatenation` |
| After linking | `strip --strip-debug` (the debug sections held build paths) | not stripped (no build paths in the files) |
| Undefined symbols | none | none |
| Libraries opened | CyberGfx, FreeType2, GadTools, Graphics, Intuition, Keymap, PosixC, StdC, StdCIO | the same |
| `Limpet-0.7.0-<t>.zip` | `083778959059dd03d650ca654e86ff1b4b04e75db7fb60ee64c7ec6d9b0cb265` | `2d06cc568dec5200debad66828ad62ed1e509bf92ce45a2d9fed7a96a3b30983` |
| `Limpet-handler-experimental-0.7.0-<t>.zip` | `d9a0592cd86bceee4e2f91df9a636c685d6e58ca43568add2751704d8dbecdd2` | `8e38e1d67ceaa9f4381785e116e26358efbfc03676f9cf2b5644fa4a5df8ccfe` |
| `Limpet` | `5a5fcffd116a8a73f57066db9e5b72f0dd948eb9120b12dc0955e80b2b44ccf3` | `16955fdcede426b3d0635b639c599ee3d6b6e0bc5f311657dff378762e5061ee` |
| `limpet-console` | `5fa824e0dfda3871ed76e1efdd05553b84672c96246fae98ccd3f9972d32fa18` | `34bd5c74d7a55d5cde63bb03ac260787b9c6594e1a3c8bdf3a2627fe3d0eee5a` |
| `limpet-handler` | `5649e6d3731db2927b2b3d878c275bbab14990f47dca22862a7b1b6b96296c1c` | `d24ae9ae42b2d36b929ae606ae2ee60c1bb59d77057c403bce0a16ef371721fc` |
| `limpet-handler-remove` | `e64212cdf62ddc2079d2e65f642ebf96178e770baaaf10c0cf7cee2df0331f41` | `d94ca108474759941c1ea4704f071f3a576ab602ccc9d36c922d240621b32697` |

A second build from scratch gave the same four binaries on both targets.

The runs below used the candidate archives `622bbef2...`/`0dfe10c7...`
(x86_64) and `61c26d6a...`/`2202df5a...` (Pi). A review then found two
wrong statements in their ReadMe files (an `UnZip` install line for systems
without UnZip, and the date of the tested mainline system), and the
archives above were made again with corrected texts. Their file lists
compared by SHA-256 differ only in `README.txt` and `README-Handler.txt`;
the programs, fonts, themes and licences are the bytes that were run.
The published x86_64 archive itself was then started once more on v1-1
(`RESULTS:limpet-v1c/`): unpacked, `Version`, Unicode text, a Shell in a
`LIMPET:` window, `Remove-Handler`, only system processes left. The
published Pi archives were not run again.
The archives carry the same fonts, themes and licences as the ABIv11 one
(HarfBuzz, utf8proc, Unicode data, DejaVu Sans Mono Nerd Font, Unifont, Noto
Emoji) and a ReadMe for the system. Probes built with the same flags:
`con-contract`, `read-cancel`, `tty-winsize`, `console-geometry-probe`,
`test_aros_endpoint`, `key-inject` (x86_64).

LimpetPTY, the mainline preview that runs the Shell in a pseudo-terminal
(`-DXTERM_PTY_FRONTEND`), is a different frontend. It was not built or run
for this record and is not covered by it.

## v1-x86_64 on mainline AROS

Pool slot v1-1: base `v1-20260923`, installed from the ISO of the same
mainline build (`74a2d80b`) as the SDK. `exec.library` pc-x86_64 51.9
(2.9.2026), `kernel.resource` 4.3. That build has `ENABLE_EXECSMP` unset, so
exec runs on one CPU although QEMU has `-smp 2`; SMP was not tested. QEMU
TCG, 800x600, two boots on 2026-10-24. Everything ran from the two archives
unpacked to `RAM:L` with `C:UnZip`. Mainline does not ship UnZip; the pool
image has an ABI-matched one added when it was prepared. Results:
`RESULTS:limpet-v1a/` (boot 1), `RESULTS:limpet-v1b/` (boot 2).

| Check | Result |
| --- | --- |
| `Version` | Limpet 0.7 for the program and the worker |
| Packet endpoint test | all groups pass, including queued reads and waiters with a cancelled middle read |
| Shell start | a Limpet window with a Shell; commands, `Dir`, `Version`, `Type` |
| Unicode | Polish letters, combining marks, CJK, Hangul and emoji drawn in the default (bundled) font |
| Copy and paste | a mouse selection copied with Amiga+C and pasted with Amiga+V into a command line; Polish and emoji pasted back byte-identical (`Type HEX`) |
| Find | Amiga+F: `jaz` not found (the text has `jaź`), `echo` 5 of 5, matches marked; Esc closes |
| User settings | `ENV:Limpet/Limpet.conf` with Vera Mono, size 15 and the light theme: a new window used all three; file deleted afterwards |
| Geometry | public packet 80x24, 640x408 px, agreeing with the private grid; malformed requests refused with error 115, buffer unchanged; after a resize with the mouse 66x19, 528x323 px; `TIOCGWINSZ` 24x80, then 19x66 |
| Ctrl-C | breaks `Wait 30` in a Limpet window and in a `LIMPET:` window (return code 5), the Shell carries on |
| `EndShell`, Ctrl-\ at the prompt | the session ends ("Session ended (F4 restarts)"); F4 starts a new Shell |
| Handler install | `Install-Handler`: "LIMPET: is mounted until the next reboot" |
| `NewShell WINDOW LIMPET:` | Shell prompt, commands, Unicode output, Ctrl-C |
| `LIMPET:` contract (`con-contract ... shared`) | failures=0: two readers served in order, a waiter beside a read, two waiters both answered, writers, LF mode, raw/cooked |
| `LIMPET:` keys (`con-contract ... keys`) | failures=0 in boot 2: cooked and raw reads, raw Ctrl-D as a byte without a signal, Ctrl-C/D/E/F to the reader, end of input from Ctrl-\ (`xy`, then 0, then input again) |
| Read cancel (`read-cancel`) | the read answered -1/304 (BREAK) before the cancel TRUE |
| Ctrl-\ in a `LIMPET:` Shell | the Shell ended and the window closed |
| Close with typed input | `abc` typed at a `LIMPET:` prompt, window closed from the menu: the Shell ended, no process left |
| Remove | `Remove-Handler`: "LIMPET: removed"; `Assign LIMPET: EXISTS` says not assigned |
| Afterwards | `Status FULL` shows only system processes |

Three key runs were made. The first (`limpet-v1a/ck.log`) had two failures
(`eof.partial`, `eof.zero`): QEMU's backslash key does not reach the PC
keymap, so Ctrl-\ was never pressed. The second (`limpet-v1b/ck2.log`)
pressed it inside the guest with `key-inject 0x0d 0x0008` on a timer, but
guest time under TCG ran at about 0.6x host time, so the driver typed the
next line before the key came and the same two checks failed. The third
(`ck3.log`) waited for the next prompt on screen instead of a clock
(`keys3.sh` in the evidence) and all checks passed.

Not tested on x86_64: OSC 8 links (Ctrl+click), SMP exec, a physical
machine, memory over many cycles.

## v1-aarch64 on the Raspberry Pi system under QEMU

Pool slot rpi-1: QEMU `raspi3b` model, AROS raspi-aarch64 2026-08-22,
`exec.library` 51.8 (22.8.2026), `kernel.resource` 4.3. The serial log says
"Uniprocessor build - secondary cores left parked" (QEMU `-smp 4`), so SMP
was not tested here either. 640x480, keyboard only: the model's pointer
cannot be calibrated, so nothing was clicked or dragged. Files were put on
the SD image with mtools while the machine was stopped (the system has no
UnZip; the ReadMe tells users to unpack on another computer). Two boots on
2026-10-24. The binaries run were hashed before staging and are the four
listed above.

| Check | Result |
| --- | --- |
| `Version` | Limpet 0.7 |
| Packet endpoint test | all groups pass |
| Shell start, commands | as on x86_64 |
| Unicode | Polish, combining marks, CJK, Hangul, emoji |
| Geometry | public packet 74x24, 592x408 px, agreeing; malformed requests refused; `TIOCGWINSZ` 24x74 |
| `LIMPET:` contract, shared | failures=0 |
| `LIMPET:` keys | failures=0 in boot 2, including Ctrl-\ (QEMU's backslash reaches this keymap) |
| Read cancel | pass |
| `CON:` control run | only `disk_info.unit` differs, which is the system console's answer |
| Selection, copy, paste | keyboard selection (Shift+Home), Amiga+C, Amiga+V |
| Find | 4 of 4 marked |
| User font | Vera Mono 15 from `ENV:Limpet/Limpet.conf` used; file deleted |
| `NewShell WINDOW LIMPET:`, close with typed input, remove | as on x86_64: the Shell ended, "LIMPET: removed", no process left |
| Ctrl-C | breaks `Wait 30` |

Not tested on the Pi: window resize (no pointer), links, a physical
Raspberry Pi, network. The Pi archives are therefore offered as a test
build, not through apkg.

Macaros (hosted AArch64 on macOS) cannot load binaries built against the
native Pi SDK; it needs its own build, which is not part of this record.

## Earlier key failures were the test driver

The first runs on the Pi (and on the released ABIv11 0.7.0 and 0.6.1, run
as controls) failed Ctrl-D/E/F and the end-of-input checks. The driver sent
Return after Ctrl-C; in a `LIMPET:` window Ctrl-C itself ends the pending
read (as described in [handler stage 2](verification-handler-stage2.md)), so
every later key answered the prompt before it. With the corrected driver the
Pi run and ABIv11 0.7.0 passed Ctrl-D/E/F, so 0.7.0 has no regression there.

## Platform

The mainline review of 2026-10-24 (`6fe38896f350`) still applies; no new
reusable system fix came out of these two builds, so no mainline patch was
prepared. The build differences above (task notification hook, C++ library)
are SDK contents of the Pi system, not bugs to report.

Evidence (local, not in git): `docs/evidence/limpet-070-v1-20261024/`
(`x86_64/`, `pi/`, `abiv11-keys/`, payload hashes, screenshots, key drivers).
