# Verification record: Limpet 0.2.0 and SDK 0.2.0

What was checked, where, and what was not.

## Environment

| Item | Value |
| --- | --- |
| Date | 2026-10-07 |
| System | AROS ABIv11 2026.09, x86_64, installed system, QEMU (TCG), 1024x768 |
| Terminal under test | Limpet 0.2.0, built from commit `60d3eac` with `LIMPET_VERSION=0.2.0 LIMPET_DATE=07.10.2026`, SHA-256 `3ff6b5d399a1ee17a7050ff92c1f48bd437ef7c548cf5297317ce80c525adc25`; the same bytes from a clean checkout of that commit and from the working tree. Later commits change only documentation, the `hello` example and a test tool (`tools/input_probe.c`); none is part of the terminal binary. |
| Older terminal | Limpet 0.1.0, the published archive (SHA-256 `668ba57e97d204896f23c7604fd1ee38b469f9b2bf566b56742b83b84fa339a0`) |
| System console | `CON:` of the same system |
| Compiler, AROS SDK | `x86_64-aros-gcc` 10.5.0, ABIv11 2026.09 Developer SDK |
| Full-screen client | hardcode `a4105b0`, built against the SDK at `60d3eac`, talking to a local test server on the host (no AI model) |
| Host tests | macOS (Apple clang); Ubuntu 24.04 arm64 (GCC 13) in a container |

## Example binaries

Built from commit `584920d` with `make ... ABI=abiv11 examples`. Examples
built from the release archive are byte-identical to these.

| Program | SHA-256 |
| --- | --- |
| `hello` | `6531efa405167a2eb851a24c30cef1e96ad65ccf7db895b99b06793f0b38e8ae` |
| `colors` | `5fbccde297c53f20cb98c33ede70d1bebec75df49e91d3cb42ea09b3a8577d6d` |
| `progress` | `7fa5b5e860044418146c2d5712c41b5fa57817ab9988524f2427ac57310103eb` |
| `geometry` | `a941887ff0abf443912b97f1c79dde629ba7676aa554c96f80a049b8e0acb499` |
| `hyperlink` | `a06f63926b4313b7fdb5366caa16ecd894190d247de0ed60e54d37cb49bb6172` |
| `image` | `e9e0139e761c8ca672944b6520ce5e513527ee58aac9cf41cbbdca14b3f73e6e` |
| `keys` | `14a4acc2b86e940c71da5417dc252c0da9f4f04a77f5fcc6395f68c98d01a0c4` |

## SDK examples on AROS

| Check | `CON:` | Limpet 0.2.0 |
| --- | --- | --- |
| `hello` | "does not answer the Limpet query", plain text | all six fields, including the three input modes |
| `hello` in Limpet 0.1.0 | | five fields; input modes "unknown" |
| `colors`, `progress`, `geometry` (two resizes), `hyperlink`, `image`, `keys` | plain fallbacks; cursor keys as 8-bit CSI | as documented; cursor keys as `ESC [` |
| all seven with output redirected to a file | no ESC byte in any file | no ESC byte; files identical to the `CON:` ones |

![hello in Limpet 0.2.0](../screenshots/sdk-hello.png)

## Input modes, recorded byte for byte

A test program (`tools/input_probe.c`) turned modes on in raw mode and
logged every input byte to a file.

| Run | What was done | Recorded |
| --- | --- | --- |
| 2004, 2048, 2100 on | 20 000-byte paste, window resized during it, three wheel steps | resize report at start; `CSI 200 ~`, 20 000 bytes with no ESC inside, `CSI 201 ~`; the two resize reports after the end marker, newest size last; three wheel reports |
| 2004 on, Esc pressed | Esc came after the paste had been delivered | full frame with end marker. Cancellation *during* delivery was not caught on AROS |
| modes left on, program ends | next program turns on nothing; paste, resize, wheel | the paste asked for confirmation and arrived unframed; no resize report; no wheel report |
| 2100 on | four wheel steps | `CSI < 64/65 ; 43 ; 12 M` with the right directions |
| after hardcode quits | paste in the Shell; a raw program with no modes, resize | confirmation asked; no report |

## hardcode in Limpet 0.2.0

| Check | Result |
| --- | --- |
| 50-line paste into the input box | 51-line draft, no confirmation; the test server received 1682 bytes, 51 lines |
| selection during a streamed reply | the selected line stayed while the reply grew; RAmiga+C copied it |
| resize during the reply | panel redrawn at the new size |
| wheel | scrolled hardcode's own view |
| `/quit` | back to the Shell, return code 0; a paste in the Shell asked for confirmation (modes reset) |

| Paste into the input box | Selection kept during the stream |
| --- | --- |
| ![hardcode after a 50-line paste](../screenshots/hardcode-paste.png) | ![selection kept while the reply streams](../screenshots/hardcode-selection-stream.png) |

## Shell regression in Limpet 0.2.0

`Version` (`Limpet 0.2.0`), `Echo` and `$RC`, redirection, `Execute`,
`Run`, `Wait 30` broken with Ctrl-C (return code 5), `List` with
scrollback, narrowing and widening the window (text rewrapped), copy and
paste. Two things seen that were the same in 0.1.0 on 2026-10-05: after
Ctrl-C in a running command the prompt appears twice, and Limpet echoes
`^C`.

## Host tests

`make test` passes on both hosts. New or changed for 0.2.0:

- `tests/test_input_order.c` (ASan, UBSan): a framed paste with resize
  reports before and inside it, a half-sent report, Esc before and after
  the start marker, a second paste while the end marker is queued, two
  pastes in a row, the input owner changing during a paste; each with the
  client reading 1, 2, 3, 5, 7, 64 bytes at a time or everything, and with
  a client that stops reading.
- `tests/test_bracketed_paste.c`: modes set, cleared by the PTY reset and
  by the cooked-mode reset.
- `tests/test_resize_report.c`, `tests/test_mouse.c`, `tests/test_reflow.c`,
  `tests/test_selection.c`, `tests/test_paste.c`.
- `sdk/tests/test_sdk.c` (ASan, UBSan), with a Limpet 0.1.0 style reply
  read by the new client as `input = 0`.
- `sdk/tests/test_sdk_compat.sh`: the SDK 0.1 sources decode the 0.2.0
  reply exactly as the 0.1.0 reply; an object built with the 0.1 header
  fails to link against the 0.2 library.

## Not shown by these tests

- Nothing on mainline AROS, on AROS One 1.3 with 0.2.0, or on real
  hardware.
- Esc during a paste that is still being delivered, a second paste during
  the first, and a client leaving raw mode in the middle of a paste were
  not produced on AROS (host tests only).
- Opening a link through OpenURL.
- A real AI model behind hardcode (a scripted local server was used).
- Other consoles than `CON:`.
