# SDK 0.1.0 verification record

*Historical. SDK 0.1.0 was never published; it was superseded by 0.2.0,
recorded in [verification-0.2.0.md](verification-0.2.0.md).*

What was checked for Limpet SDK 0.1.0, where, and what was not.

## Environment

| Item | Value |
| --- | --- |
| Date | 2026-10-05, final binaries rerun 2026-10-06 |
| System | AROS ABIv11 2026.09, x86_64, installed system, QEMU (TCG), 1024x768 |
| Terminal | Limpet 0.1.0 from the published archive `Limpet-0.1.0-abiv11.zip`, SHA-256 `668ba57e97d204896f23c7604fd1ee38b469f9b2bf566b56742b83b84fa339a0` (same digest as the GitHub release asset) |
| System console | `CON:` of the same system, opened with RAmiga+W |
| Compiler | `x86_64-aros-gcc` 10.5.0 |
| AROS SDK | ABIv11 2026.09 Developer SDK |
| Host tests | macOS (Apple clang); Ubuntu 24.04 arm64 (GCC 13) in a container |

## Tested binaries

Built from the SDK sources with `make ... ABI=abiv11 examples`. These are
the final binaries; examples built from the release archive are
byte-identical to them. The first run (2026-10-05) used an earlier build
without two later fixes (an empty final Kitty chunk after a failed write,
and the error path of `image`); the screenshots come from that run and the
final binaries were run again with the same results.

| Program | SHA-256 |
| --- | --- |
| `hello` | `f6a65a1af71cdc94b6276679d3ef622b07db35bb1c44e509617152bbed35643c` |
| `colors` | `8fded19915c86471993552b0499117e66a30b6ad0c7fcbd8ac18e6d583f18c36` |
| `progress` | `0e1c4110acbf2d126ecfa749e5ec860aa34d69e351a12a5094f323c989792839` |
| `geometry` | `0794a631660c1682553a8efd2342997d3b06b17d8bf443e8c774a7755931b2ef` |
| `hyperlink` | `e416acdd54592d45fd8fb589358ee6cc4c0f71058ad728d2df7b52d14da3c937` |
| `image` | `a988f0f380b48a0207e33f44dff1097f9b4f6363b504e83aec615683d0732bb4` |
| `keys` | `4d2c75b3c96fe533ec87e90b20c5e7e03a3b16c3f688184552e9cca1cb07fb13` |

None of them has an undefined symbol (`x86_64-aros-nm -u`).

## Results on AROS

| Check | `CON:` | Limpet 0.1.0 |
| --- | --- | --- |
| `hello` | "does not answer the Limpet query", plain text | all five fields known: 24-bit colour, pens, bold, line rewrite, 117x37 |
| all seven programs with output redirected to a file | plain text, no ESC byte in any file | identical files, no ESC byte |
| `colors` | names only, no sequences | 16 colours as `38;5;n`, 256-colour ramp, 24-bit gradient |
| `progress` | `0%` ... `100%` on separate lines | one line rewritten in place |
| `progress`, Ctrl-C at about 33% | not run | `***Break at 33%`, return code 5 |
| `geometry` | "does not report its size" | 117x37, then 91x28 and 117x37 after two resizes during the run |
| `hyperlink` | labels with URLs in angle brackets | labels as links; hover underlines and shows the target in the title bar |
| `image` | "Not a Limpet console: no image sent" | `a=q` answered in raw mode, 160x96 image drawn below the text, deleted on a key press |
| `keys`: Up, Left, `a`, Esc, Ctrl-C, then a Shell command | Up and Left as 8-bit CSI, cooked mode restored | Up and Left as `ESC [`, cooked mode restored |
| comparison probe (`tools/con_compare.c`) | UTF-8 as Latin-1, SGR 31 as pen 1, 256 and 24-bit colour ignored, OSC 8 printed as text, CR + `CSI K` honoured | all shown as intended, SGR 31 as pen 1 |

Screenshots (cropped from full-screen captures):

| | |
| --- | --- |
| ![hello in Limpet](../screenshots/sdk-hello.png) | ![examples in CON:](../screenshots/sdk-con-fallback.png) |
| ![colors in Limpet](../screenshots/sdk-colors.png) | ![image in Limpet](../screenshots/sdk-image.png) |
| ![geometry in Limpet](../screenshots/sdk-geometry.png) | ![progress in Limpet](../screenshots/sdk-progress.png) |
| ![probe in CON:](../screenshots/sdk-con-compare.png) | ![probe in Limpet](../screenshots/sdk-limpet-compare.png) |

## Host tests

`make test` passes on both hosts, including `test-sdk`:

- `sdk/tests/test_sdk.c`, under AddressSanitizer and UndefinedBehaviorSanitizer:
  the SDK's request answered by Limpet's own reply code
  (`src/xterm_presentation.h`); geometry as a snapshot; unknown geometry;
  pens on and off; refusal as from `CON:`, refusal without an error code,
  a block returned unanswered, an out-of-range result, success with an
  error code, an invalid geometry claim, other DOS errors; a reply from a
  future terminal with unknown bits and reserved values; no packet for a
  non-interactive handle; the caller's block unchanged on every failure;
  SGR, cube reduction, OSC 8 validation; Kitty framing at sizes around
  every chunk boundary, the largest accepted image, refused arguments, a
  failing writer, delete, query and reply parsing; short writes.
- The SDK library compiled as C on both hosts and its header as C++.
- `sdk/include/limpet/tty_control.h` is byte-identical to the header
  Limpet 0.1.0 was built from (SHA-256 `921bc72d...cdf1`).
- The terminal built from this tree for ABIv11 is byte-identical to a
  build of the `v0.1.0` tag with the same command: moving the protocol
  header into the SDK changed nothing in the terminal.

## Not shown by these tests

- Nothing was run on mainline AROS, on AROS One 1.3, or on real hardware.
  No SDK library for mainline was built.
- Opening a link through OpenURL (no browser was configured).
- Ctrl-C in `progress` was not tried in `CON:`.
- Performance of large images was not measured.
- Other consoles (KingCON and similar) were not tried.
- Programs other than the examples and apkg have not used the SDK.
