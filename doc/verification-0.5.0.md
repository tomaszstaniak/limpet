# Verification record: Limpet 0.5.0 and the experimental LIMPET: handler

Limpet 0.5.0 fixes a crash of POSIX programs that ask for the window size,
and adds Find in output and a settings file. The experimental console
handler `LIMPET:` is a separate archive ([details](limpet-handler.md)).
The SDK is unchanged (SDK 0.3.0).

## Build

| Item | Value |
| --- | --- |
| Sources | tag `v0.5.0` |
| Compiler | `x86_64-aros-gcc` 10.5.0 |
| SDK | ABIv11 2026.09 Developer SDK |
| Commands | `make abiv11-frontend abiv11-handler abiv11-xterm-console LIMPET_VERSION=0.5.0 LIMPET_DATE=15.10.2026`, then `tools/release/make-packages.sh 0.5.0 OUT abiv11` |
| Program | 3,413,576 bytes, 27,280 more than 0.4.1 (+0.81 %) |
| Archives | `Limpet-0.5.0-abiv11.zip` and `Limpet-handler-experimental-0.5.0-abiv11.zip`; hashes in the release's `SHA256SUMS` |

The package script refuses a main archive that carries any handler file;
unpacking it was checked to give only `Limpet`, `Limpet.info`, `README.txt`,
`LICENSE.txt`, `Fonts`, `Themes` and `Licenses`.

## Host tests

`make test` passes. New in this release: `tests/test_output_search.c`
(with AddressSanitizer and UBSan), `tests/test_settings.c`,
`tests/test_console_name.c`, and a configured history length in
`tests/test_line_editor.c`. The search test includes a resize while the
alternate screen is up, a case an independent review found (Find numbered
matches backwards after resizing under vim or less); it fails on the code
before the fix.

## On AROS

Pool slot v11-2, AROS One 1.3 (ABIv11), QEMU TCG, 2026-10-15. All runs
from `RAM:`. The final checks used the release archives; earlier boots
used development builds of the same sources and are listed only where
they cover something the final checks did not.

### TIOCGWINSZ (the crash fix)

`tests/winsize_check.c` (`make abiv11-xterm-console` builds it as
`winsize-check`) asks for the size with `ioctl(TIOCGWINSZ)` and checks it
is a Limpet grid.

| Terminal | Result |
| --- | --- |
| Limpet 0.4.1 (the published program) | "Software Failure": Illegal address access, crt.library `ioctl`+0x1FE. The C library reads the size through `ACTION_DISK_INFO`'s `id_InUse`, which 0.4.1 left 0 |
| Limpet 0.5.0, new window | 24 rows, 80 columns |
| after making the window smaller | 16 rows, 56 columns |
| after making it larger | 28 rows, 104 columns |
| with JetBrains Mono 15 px from the settings file | 24 rows, 80 columns |
| `LIMPET:300/200/600/300` | 13 rows, 70 columns |

### Find in output

| Check | Result |
| --- | --- |
| `needle` after `List` output | `4 of 4`, the match in selection colours, others underlined |
| Up while a background job prints | `1 of 4 (oldest)`; the view stayed on the match while output arrived |
| Esc | back to the bottom |
| Width change with Find open | the same match (`2 of 4`) after the rewrap |
| On the alternate screen | refused, with the reason in the title |
| Select a word, RAmiga+C, RAmiga+F, RAmiga+V | the word became the search text: `Find: pastetoken - 2 of 2` |
| Ctrl-C with Find open | Find closed, the Shell got the break (`^C`, new prompt) |
| Next command | ran normally (`after find rc=0`) |

### Settings

| Check | Result |
| --- | --- |
| `font=` JetBrains Mono TTF, `font_size=15`, light theme, new window | that font at 15 px, light theme, 80x24 cells |
| `font=` a file that does not exist, `font_size=15` | Vera Mono at 15 px; title: `font RAM:p/none.ttf is not a usable monospace font; Vera Mono` |
| file changed to `font_size=30`, then Project > Reload settings | title: `Limpet.conf line 2: font_size takes 8 to 20; nothing changed`; the window kept its font and theme |
| a wrong file at start | the built-in settings, the line named in the title |
| a changed theme, Reload settings | the theme changed at once |
| `scrollback=1000`, `history=3` (development build) | Up reached back three commands |
| close the window | gone from `Status` |

### LIMPET: handler (experimental)

From the release archive, unpacked over the same drawer:
`Install-Handler`; the contract probe (`tests/con_contract.c`) on
`LIMPET:0/300/640/200/Probe/CLOSE` passed every check it passes on `CON:`
except the two Amiga console reports, which `LIMPET:` does not answer; a
`NewShell WINDOW "LIMPET:300/200/600/300/Handler/CLOSE"` ran and ended with
`EndCLI`; `Remove-Handler` then removed the device.

The fuller handler checks ran on development builds under its working name
`XTERM:`: CON:'s name syntax and options, Execute, redirection, `Run`,
Ctrl-C, `Ask`, `Copy ?`, WAIT, removal refused while a window is open, and
a side-by-side comparison with `CON:`. Measured differences from `CON:`:
no answer to the window-bounds and cursor reports, Ctrl-D not signalled,
F5 instead of Ctrl-\ for end of input, one reader at a time. Memory: 10
Shell windows opened and closed in a row left free memory 86,864 B lower
after the first five and 544 B lower after the next five; what that is was
not established. About 32.5 MiB per open window.

## apkg

The release asset (`Limpet-0.5.0-abiv11.zip`, 3,666,468 bytes, SHA-256
`695499c8…`, matching the release's `SHA256SUMS`; the program inside is
the clean rebuild of the tag) is in the apkg catalogue since
arospkg-index PR #19 (2026-10-15). Before the merge, on pool slot v11-2
(AROS One 1.3) with apkg 0.5.0 and the branch's catalogue: 0.4.1 installed
from the public catalogue; `Themes/light.theme` changed by hand (red
cursor); `upgrade limpet` to 0.5.0 (rc 0; 2 files replaced, 13 unchanged);
`verify` 14 as installed and the theme changed locally, byte-identical to
before the upgrade; the upgraded program started with that theme and a
settings file (15 px), `winsize-check` 24x80, Find found its match, the
window closed with nothing left in `Status`; `remove` kept the changed
theme. The handler archive from the release, unpacked separately, passed
the contract probe on `LIMPET:` and was removed with `Remove-Handler`.

## Not checked

- Mainline, ABIv11 2026.09, real hardware.
- Fonts other than Vera Mono and JetBrains Mono.
- Two programs reading one console at once; the write queue with a full
  output queue.
- `LIMPET:` options `SCREEN`, `NOBORDER`, `BACKDROP`, `INACTIVE` on AROS
  (host tests only).
