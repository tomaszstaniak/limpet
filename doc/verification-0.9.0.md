# Verification record: Limpet 0.9.0

Release checks on 2026-11-02. All targets were built from the same source
with version 0.9.0 and date 02.11.2026: ABIv11 with the 2026.09 Developer
SDK, mainline with the Developer SDK from `74a2d80b`, and the Pi with the
AROS Clang 20.1 wrapper and the 2026-08-22 SDK. `make test` passes.

All runtime checks used QEMU. On each system Window.conf held
`background_opacity=70` and `background_blur=on`.

| System | Result |
| --- | --- |
| AROS One 1.3, x86_64 ABIv11 | Solid background. Preferences, from a Limpet window, disables both options and says why. Use keeps both settings. Shell, Fastfetch and resize work. |
| nightly 20261031, x86_64 ABIv1 | Translucent background and normal frame. Shell, Fastfetch, Helix, Preferences, Use and resize work. Blur stays disabled with its note; `background_blur=on` is kept. |
| nightly 20261102, x86_64 ABIv1 (live ISO) | Translucent background, Shell, Fastfetch, Helix and resize work. Preferences not run here. |
| nightly of 2026-10-30, AArch64 ABIv1 Pi | Translucent background, Shell, Fastfetch, resize and Preferences work. Opacity changed to 32 applies at once with Use; `background_blur=on` is kept. Helix not run. |
| mainline `87266168` (2026-10-26), x86_64 ABIv1 | Outside the supported range: on 32-bit VESA the window is translucent, but its frame is not drawn and a drag on the size gadget did nothing. On a 16-bit screen the window stays solid. |

## Catalogue

With published apkg 0.7.3 on AROS One, in a RAM package root: Limpet 0.8.0
installed from the previous catalogue, then `apkg upgrade limpet` to 0.9.0 from
the public catalogue. `apkg verify` matches the 20 package files, and a file
added under `Themes` and `ENV:Limpet/Window.conf` stay as they were. The
upgraded copy was not started; its files are those run above.

## Limits

No physical hardware, SMP exec or Macaros test. The experimental handler
archives were built and checked for the correct ABI, but not run. Mainline
built between 2026-10-24 and 2026-10-26 reports the screen capability, so
Limpet cannot tell it from a newer system; opacity stays off unless chosen.
