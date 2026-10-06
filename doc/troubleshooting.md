# Troubleshooting

## The query fails in Limpet

`limpet_query_presentation()` returns something other than `LIMPET_OK`
while the program runs in a Limpet window.

| Status | Likely cause | Check |
| --- | --- | --- |
| `LIMPET_NOT_INTERACTIVE` | output is redirected (`>file`, a pipe) | run without redirection |
| `LIMPET_UNSUPPORTED` | the program's output is not the Limpet window, for example because it opened `CON:` itself or was started by another program with its own output handle | `hello` from the same Limpet window answers `Output is a Limpet console` |
| `LIMPET_UNSUPPORTED` | the Limpet session is closing | |
| `LIMPET_INVALID_REPLY` | a request with nonzero fields; a third-party console that answers badly | use `limpet_presentation_request()` |
| `LIMPET_FAILED` | another DOS error, for example `ERROR_INVALID_LOCK` for `NIL:` or a closed handle | check `*ioerr` |
| `LIMPET_BAD_ARGUMENT` | `Output()` is 0, as for a program started from Workbench without a console | |

A program that queries `Input()` instead of `Output()` gets an answer for
the wrong handle. The SDK takes whatever handle you pass.

## Colours are wrong

- **Red shows as black or grey.** The program sends SGR 31 to a console
  with pens. Use `limpet_sgr_ansi()`, which sends `38;5;1` there.
- **The Shell prompt changes colour after my program.** The program did not
  end its colours. End with `ESC[39m`/`ESC[49m` or `ESC[0m`.
- **Text vanishes.** Colour 0 on a dark theme, or 15 on a light one. Do
  not print text in those colours without a background.

## Garbage such as `]8;;https://...` or `_Ga=T...` in the output

The program sent OSC 8 or Kitty data to `CON:`. Send them only after a
successful presentation query.

## UTF-8 shows as accented Latin letters

The program runs in `CON:`, which reads bytes as Latin-1. There is no
detection for this other than the query; write ASCII when it fails, if the
text must be readable there.

## A stream query never answers

`CSI 6 n`, `CSI 18 t` or the Kitty query wait forever:

- the console is in cooked mode. Limpet answers only in raw mode
  (`SetMode(Input(), 1)`);
- the console is not Limpet. Always read with `WaitForChar()` and a limit;
- the user typed during the exchange and the reply is mixed with keys.

## An image is cut off at the bottom, or text overlaps it

Limpet 0.1.0 does not scroll to make room for an image. Print blank lines,
move back up, draw with `keep_cursor = 1`, then move down, as the `image`
example does.

## `ERROR_ACTION_NOT_KNOWN` for `XTY_CAPS` or termios calls

Expected. A Limpet 0.1.0 Shell session serves only operation 13. See
[experimental interfaces](sdk/experimental.md).

## The program crashes at start on AROS

- **Wrong ABI.** An ABIv11 binary on mainline AROS, or the reverse, fails
  in unpredictable ways. Build with the toolchain and SDK of the system
  you run on.
- **Stripped too hard.** A full `strip` of an x86_64 AROS executable can
  leave it unrelocated. Use `strip --strip-unneeded` or `--strip-debug`.

## Reporting a problem

See [CONTRIBUTING.md](../CONTRIBUTING.md#reporting-a-bug). The output of
`hello` from the same window is the most useful single thing to include.
