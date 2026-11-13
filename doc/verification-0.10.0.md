# Verification record: Limpet 0.10.0

Release checks on 2026-11-12 and 2026-11-13. All targets were built from the
same source with version 0.10.0 and date 13.11.2026: ABIv11 with the 2026.09
Developer SDK, mainline with the Developer SDK from `74a2d80b`, and the Pi
with the AROS Clang 20.1 wrapper and its SDK. `make test` passes.

All runtime checks used QEMU, with the release archives unpacked in the
guest.

| System | Result |
| --- | --- |
| AROS One 1.3, x86_64 ABIv11 | Preferences lists the themes that come with Limpet. Handler archive: `con-contract` without keys, its `auto` and `window` checks against `LIMPET:`, and without keys against `CON:` with `LimpetCON ON`: no failed check; `NewShell` with `LimpetCON ON` opens a Limpet window. A window set to run Fastfetch on a system without it shows "Fastfetch is not installed: ..." and a prompt; with a program chosen that exists, the program runs. |
| official nightly 20261110, x86_64 ABIv1 | A window with `font_size=16`, `theme=dracula` (by name) and Fastfetch set but not installed: the larger font, Dracula's colours, the one-line note and a prompt. Handler archive: `con-contract` without keys and `auto` against `LIMPET:`, `auto` against the system's `CON:` and against `CON:` with `LimpetCON ON`: no failed check; `NewShell` with `LimpetCON ON` opens a Limpet window; `LimpetCON OFF` returns the system's console. |
| Raspberry Pi, AArch64 ABIv1 | Built; not run for this release. |

Checked on a build of the same source made earlier that day, before the last
correction (below): "Add file..." through the requester copied a colour file
into `ENVARC:Limpet/Themes`, listed it as `(yours)` and showed it in the
terminal; Use wrote `font_size`, `theme` and `scrollback` into
`ENV:Limpet/Limpet.conf`.

Checked on the candidates of 2026-11-12 (the same theme and Preferences
code): a theme by path and by name; Reload settings with a file that is not
a colour file, which names the reason and keeps the theme; preview, Use,
Save and Cancel in Preferences; "Open themes drawer"; the list following
files put into the drawer; Helix with its own theme and with
`base16_terminal` over an imported scheme. Real colour files read on the
host with the same reader: Dracula and Catppuccin Mocha (Ghostty), Nord and
Gruvbox Dark (Kitty), Tomorrow Night (iTerm2).

## Found during the release checks

The Fastfetch check for a new Shell window is several lines of startup
script, and the session refused a startup line with a line end: with
Fastfetch set, Limpet did not start at all, without a message. Corrected
before the release (the session takes several lines; a Shell that cannot be
started always says so), and the corrected archives are the ones checked in
the table above.

## Not checked

- Colour queries from a program, and the bold switch, in a guest. Both are
  covered by the host tests.
- "Add file..." and the settings written by Use on the final archives (see
  above); the font and theme they wrote were then read by the final build.
- A themes drawer with several hundred files. Programs started from
  Workbench with console windows under `LimpetCON ON`. Keys typed into a
  `WINDOW` console.
- The Pi build, physical hardware, ABIv11 2026.09.

## Catalogue

After publication, 2026-11-13. With published apkg 0.7.7 on AROS One, in a
RAM package root: Limpet 0.9.0 installed from the previous catalogue, a
file of the user's added under `Themes`, then `apkg upgrade limpet` to
0.10.0 from the public release. The plan was 13 files added, 3 replaced,
the user's file kept, 17 unchanged. `apkg verify` finds 33 files as
installed and the user's file apart from the package. The upgraded copy
reports 0.10.0 and opens a Shell.

On the mainline nightly the same was not possible: the test machine had no
name resolution, so apkg could not fetch the archive. The mainline archive
was run from the unpacked release archive (table above).

The published archives were downloaded again and match the tested ones
byte for byte; the public catalogue lists 0.10.0 for x86_64 ABIv11 and
ABIv1. The Raspberry Pi build stays outside the catalogue.
