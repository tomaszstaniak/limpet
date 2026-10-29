# Verification record: Limpet 0.8.0

Release checks on 2026-10-29, following the Preferences and session-start
checks of 2026-10-28. Program sources are those of `a8348a3`; subsequent
release changes update documentation only.

## Builds

All targets were built from the same source with version 0.8.0 and date
29.10.2026. PC builds use GCC 10.5.0: ABIv11 with the 2026.09 Developer SDK,
mainline with the Developer SDK from `74a2d80b`. The native Raspberry Pi
build uses the AROS Clang 20.1 wrapper and the 2026-08-22 Pi SDK.
`make test` passes, including 43 Python tests, the native host tests and
SDK compatibility checks. Packaging verifies architecture, C-library ABI,
version and the target build records for each executable.

## Release checks

All runtime checks used QEMU with a uniprocessor exec:

| System | Checks |
| --- | --- |
| AROS One 1.3, x86_64 ABIv11 | Final Limpet and Fastfetch r3: picture logo, text fallback in CON:, configuration precedence and groups, JSON and redirected output. SDK examples in Limpet, including identity/text capabilities, geometry, image, title and keys; clean Shell exit. |
| mainline `74a2d80b`, x86_64 ABIv1 | Final Limpet and Fastfetch r3: picture logo, configuration precedence, groups, JSON and redirected output. Preferences opens and Cancel returns to the Shell; clean session exit. |
| native Raspberry Pi system of 2026-08-22, AArch64 ABIv1 | Final Limpet and Fastfetch r3: picture logo, configuration precedence, groups, JSON and redirected output. Preferences Use selects Fastfetch and the picture preset; a new window runs it and returns to the prompt. Native endpoint test passes, including queued reads, cancellation and shutdown. |

The more extensive Preferences checks on the same program sources on
2026-10-28 covered two new windows, Ctrl-C during startup, a missing
program, NewShell, Run now refusal while busy, and startup disabled on
both PC systems. Save across restart, Cancel, custom configuration and
preserving an unknown configuration line were checked on AROS One only.

The release ZIPs differ from the runtime-tested ZIPs only in README.txt;
all other entries were compared byte for byte. SDK examples rebuilt from
the SDK release ZIP match the tested binaries; see
[SDK 0.6.0 verification](sdk/verification-0.6.0.md).

## Limits

No physical hardware, SMP exec or Macaros test. The experimental handler
archives were built and checked for the correct ABI, but their install/
remove scripts were not rerun in this release pass. Pi Local IP remains
untested because the emulated Pi has no working network. These runs do not
establish a long-running memory profile.

## Archives

| Archive | Bytes | SHA-256 |
| --- | --- | --- |
| `Limpet-0.8.0-aros-aarch64-abiv1-raspi.zip` | 5196231 | `e557a04fcdfaf0680c15afb16b01a2e9178922a64854cd3d08538f3dc8de073d` |
| `Limpet-0.8.0-aros-x86_64-abiv1-pc.zip` | 5257588 | `3a4187957f8eb3118bc337f8e044f3c09d095d08dd264f9e54984a2173b97359` |
| `Limpet-0.8.0-aros-x86_64-abiv11-pc.zip` | 5338727 | `b7ad7a4b035316f98b9d7862c4de8405ec1cfcd2a92c961e774da7868afe7ed1` |
| `Limpet-handler-experimental-0.8.0-aros-aarch64-abiv1-raspi.zip` | 902748 | `365ef1f0e62eee3d624419349f89ee658a0334a1c0cf12e3e808109bfb941915` |
| `Limpet-handler-experimental-0.8.0-aros-x86_64-abiv1-pc.zip` | 971332 | `07e3538b488c95aa474bdcb8a32d0c7989bfc691ec3e3ccdc0b5483adafa3b27` |
| `Limpet-handler-experimental-0.8.0-aros-x86_64-abiv11-pc.zip` | 1053739 | `cb4fd038105462123981dea2b8b7d9d95c6533163403a77ed42032d1bc685bfb` |
