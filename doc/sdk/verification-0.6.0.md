# Verification record: SDK 0.6.0

Checked on 2026-10-29. SDK 0.6.0 adds text capabilities, terminal identity,
Kitty graphics confirmation and the public console geometry query. The
presentation structure and library link names changed; applications must
be rebuilt to use the new fields.

## Host checks

`make test` passes, including SDK contract, compatibility and link checks.
The eight examples were built with ABIv11 GCC 10.5.0 and the 2026.09 SDK.

## Runtime checks

AROS One 1.3, x86_64 ABIv11, under QEMU, with the release build of Limpet
0.8.0. `hello`, `colors`, `progress`, `geometry`, `hyperlink`, `image` and
`title` completed with rc 0, both in the terminal and with output redirected.
The image appeared and was removed; the title changed and returned to Shell.
`keys` reported Down and quit on q, returning to the Shell. `hello` in CON:
reported an unsupported query and used plain text.

This run did not repeat every example in CON:, test old terminal releases,
or run SDK example binaries on mainline, Pi or physical hardware. Host
compatibility tests cover replies read by older SDK clients. Fastfetch's
use of the new contract is covered by the terminal release verification.

## Example binaries

| Program | SHA-256 |
| --- | --- |
| `colors` | `56eb3342f6e7e913d8912f6d73ba049869ef36aceaf8838e8c58e7542f5be75a` |
| `geometry` | `3fe9a587b76ebbbf085702d9b82d490006b0149120f5e67967e204b1c865057c` |
| `hello` | `bb9e0d0e219565a53a3d4c98b184afeeabca0b003a4de55d1bfeb4ebdea931ec` |
| `hyperlink` | `b4c670541985be80f8455ecd0a8d673280c807778f1865c7de57620a261ad87b` |
| `image` | `a615b71b51b3dc31e4d4c97691a991fc00fad28d21657379004560ace011ee51` |
| `keys` | `87522e9bb2025df0abb286ada2b0169f1bcedda4abee43f35b017c82bcbdf8fd` |
| `progress` | `3ea2247a06338bb46761ab635fd60db0ab6189bb571ffe8e43475d4a1bba4881` |
| `title` | `fdb0ad630b2f29e3b017f57566107ddb6aa76379f5928cf6bed2559f0a0b6e19` |
