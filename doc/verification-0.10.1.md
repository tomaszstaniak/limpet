# Verification record: Limpet 0.10.1

Checks of 2026-11-14. All targets were built from the same source with
version 0.10.1 and date 14.11.2026, with the toolchains and SDKs of 0.10.0
(ABIv11: GCC 10.5 with the 2026.09 Developer SDK; mainline: the Developer
SDK from `74a2d80b`; the Pi: the AROS Clang 20.1 wrapper and its SDK).
`make test` passes. All runtime checks used QEMU.

The record has three parts with separate scopes: the 0.10.1 archives, the
checks left open in 0.10.0, and the package as apkg installs it. The
experimental handler archive was rebuilt with the new version and not run
again: its code is unchanged since 0.10.0, whose record covers it.

## The 0.10.1 archives

Each release archive was unpacked in the guest. `Window.conf` held
`background_opacity=70` on all three systems.

| System | Result |
| --- | --- |
| AROS One 1.3, x86_64 ABIv11 (layers.library 50.0) | Starts with `theme=nord`; the background stays solid; Preferences opens, shows the theme's colours and says the screen cannot show transparent windows. |
| official nightly 20261110, x86_64 ABIv1 (layers.library 50.13) | Translucent background with its frame; the window follows a drag on the size gadget. With Fastfetch set to a place where it is not, the window shows the one-line note, and "Run now" in Preferences answers "Not run: Fastfetch is not at the place set here". |
| official nightly 20261110, AArch64 ABIv1 Pi (QEMU raspi3b, layers.library 50.13) | Starts with `theme=gruvbox-dark` by name; translucent background with its frame; Preferences opens with the theme selected. |

The rule for the frame (layers.library 50.9, mainline `fc8bbc76` of
2026-10-27) comes from the mainline change itself and is covered by a host
test. It was not run on a mainline build of 2026-10-24 to 2026-10-27: the
test machines have none.

## Checks left open in 0.10.0

Run on the published 0.10.0 archives, whose theme and settings code 0.10.1
shares.

- **Colour queries from a program.** A small program in a Limpet Shell
  asked `OSC 10`, `11` (ended by BEL and by ST), `12`, `OSC 4` for entries
  1, 15 and 16, and `CSI ? 996 n`. With an imported iTerm2 scheme the
  answers were that scheme's text, background, cursor and palette colours,
  and "dark"; entry 16 is not answered, as documented. Same answers on
  AROS One, mainline x86_64 and the Pi, and with other themes in the 0.10.1
  runs above.
- **The bold switch**, on AROS One: bold text in a palette colour is shown
  in the bright colour; after `bold_bright=off` and Project > Reload
  settings it is shown in the colour asked for.
- **"Add file...", Save and a restart**, on all three systems: the file
  chosen in the requester is copied to `ENVARC:Limpet/Themes`, listed as
  `(yours)` and shown in the terminal at once; Save writes
  `theme=Mine.itermcolors` to `ENVARC:Limpet/Limpet.conf`; after the system
  is restarted a new Limpet window has that theme (confirmed by its
  answers to the colour queries).

## The package through apkg

apkg 0.7.8, with the package root on the system volume (`SYS:`), so that a
restart keeps it. This covers the main archive as the catalogue lists it;
the handler archive is not a catalogue package.

| System | Result |
| --- | --- |
| official nightly 20261110, AArch64 ABIv1 Pi | `apkg update` and `apkg install limpet` (0.10.0) from the public catalogue; `apkg verify`: 33 files as installed. A theme added and saved in Preferences. After a restart: still installed, verify unchanged, the window has the saved theme. `apkg remove limpet` leaves nothing of the package and keeps `ENVARC:Limpet` with the settings and the theme. Installed again: verify 33 files, the window has the saved theme. |
| official nightly 20261110, x86_64 ABIv1 | The same sequence, starting from 0.9.0: installed, then `apkg upgrade limpet` to 0.10.0 (13 files added, 3 replaced, 17 unchanged), verify 33 files; theme saved; restart; remove; install again. Settings and theme kept throughout. This machine has no name resolution, so apkg was given the public catalogue's index files (before and after 0.10.0) and the published archives, fetched on the host; apkg checked their published hashes. The download itself was not exercised here. |

On the Pi the first download of the archive stalled with no progress for
ten minutes while the catalogue had been fetched without trouble. It was
interrupted; apkg reported that nothing had been written, and the second
attempt completed in under two minutes. The cause was not established.


## Catalogue

After publication, 2026-11-14, apkg 0.7.8. The published archives were
downloaded again and match the tested ones byte for byte; the public
catalogue lists 0.10.1 for all three systems.

| System | Result |
| --- | --- |
| AROS One 1.3, x86_64 ABIv11 | 0.10.0 installed, `apkg update`, `apkg upgrade limpet` from the public catalogue: 3 files replaced, 30 unchanged; verify 33 files; reports 0.10.1 and opens a Shell. |
| official nightly 20261110, x86_64 ABIv1 | The same upgrade with the public index and the published archive fetched on the host (no name resolution on the test machine): 3 files replaced, verify 33 files, opens a Shell, removed with apkg. The catalogue now lists this build as installable on mainline x86_64. |
| official nightly 20261110, AArch64 ABIv1 Pi | With the package root on the SD card: 0.10.0 installed, `apkg update`, `apkg upgrade limpet` from the public catalogue; the installed program reports 0.10.1 and opens a Shell; removed with apkg. The text output of apkg's later commands, `verify` among them, was not captured in this run (the files stayed empty), so the file check is not recorded here. |

On the Pi three earlier attempts with the package root in `RAM:` ended in
a system failure while apkg installed 0.10.0 from an archive placed in its
cache, before Limpet was started. It concerns apkg or the Pi system under
QEMU, not Limpet.

## Not checked

- A mainline build of 2026-10-24 to 2026-10-27 (see above).
- Programs started from Workbench with console windows under
  `LimpetCON ON`; keys typed into a `WINDOW` console.
- Physical hardware (also for the Pi build), ABIv11 2026.09.
