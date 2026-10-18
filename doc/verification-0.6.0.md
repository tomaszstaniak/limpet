# Verification record: Limpet 0.6.0

Limpet 0.6.0 adds Ctrl-D/E/F and Ctrl-\ as in the system console, the
Amiga console's window-size and cursor reports, and Shift+Enter for
raw-mode programs that ask for it (`CSI > 4 ; 1 m`). The experimental
`LIMPET:` handler archive has the same changes. SDK 0.4.0 names the new
input bit ([its record](sdk/verification-0.4.0.md)).

## Build

| Item | Value |
| --- | --- |
| Sources | tag `v0.6.0` |
| Compiler | `x86_64-aros-gcc` 10.5.0 |
| SDK | ABIv11 2026.09 Developer SDK |
| Commands | `make abiv11-frontend LIMPET_VERSION=0.6.0 LIMPET_DATE=18.10.2026`, then `make abiv11-handler abiv11-xterm-console` with the same variables, then `tools/release/make-packages.sh 0.6.0 OUT abiv11` |
| Program | 3,416,912 bytes, 3,336 more than 0.5.0 (+0.10 %) |
| Archives | `Limpet-0.6.0-abiv11.zip` and `Limpet-handler-experimental-0.6.0-abiv11.zip`; hashes in the release's `SHA256SUMS` |

`make -j` with `abiv11-handler` in the same run fails, because the handler
needs the frontend's HarfBuzz object; build the frontend first.

## Host tests

`make test` passes. New in this release: `tests/test_amiga_reports.c`
(bounds and cursor reports with the 8-bit and the 7-bit CSI, after a
resize, split over several writes); in `tests/test_bracketed_paste.c` the
Shift+Enter mode (on, off by `CSI > 4 m`, level 0, `CSI > 4 n` and
`CSI > m`; `CSI > 0 m`, `CSI > 1 n` and other `CSI >` sequences change
nothing and print nothing; off after the session reset and on return to
cooked mode); in `tests/test_input_echo.c` the Shift+Enter encoding. The
SDK compatibility test now also checks an SDK 0.3.0 client.

The Shift+Enter change was reviewed before the merge; the review's points
(Amiga keys excluded, `CSI > m` and `CSI > 4 n` turn the mode off,
`CSI > 0 m` does not) are in the code and tests above.

## On AROS

Pool slot v11-2, AROS One 1.3 (ABIv11), QEMU TCG, 2026-10-18, one boot.
Everything ran from the release candidate archives, unpacked to `RAM:`;
the program inside is the one built from the tag (same SHA-256). Bytes
were logged by `tests/enter_probe.c` (`enter-probe`, built by
`make abiv11-xterm-console`), which writes each key's bytes to a file.

### Shift+Enter

| Key, after `CSI > 4 ; 1 m` | Bytes received |
| --- | --- |
| Enter | `0d` |
| Shift+Enter | `1b 5b 32 37 3b 32 3b 31 33 7e` (`CSI 27 ; 2 ; 13 ~`) |
| keypad Enter | `0d` |
| Shift+keypad Enter | `CSI 27 ; 2 ; 13 ~` |
| Ctrl+Enter | `0d` |
| Alt+Enter | `1b 0d` |
| Shift+LAmiga+Enter, Shift+RAmiga+Enter | `0d` each |
| Shift+Enter after `CSI > 4 n` | `0d` |
| Shift+Enter after `CSI > 4 ; 1 m` again | `CSI 27 ; 2 ; 13 ~` |

| Check | Result |
| --- | --- |
| Program ended with the mode on; next program, mode not asked for | Shift+Enter and Enter both `0d`: the mode ended with raw mode |
| Enter and Shift+Enter at the Shell prompt | each ran the command once |
| A chat client in raw mode that uses the mode (Bishop, aros-hardcode `b856e0e`, against a scripted local server) | key line "Shift+Enter new line"; a line, Shift+Enter, a line: two lines in the input; Enter sent one message of two lines (the server's log: `user_lines=2`); Shift+LAmiga+Enter sent the next message; after `/quit` the Shell took Enter normally |
| `hello` (SDK 0.4.0) | input modes list Shift+Enter (`CSI > 4 ; 1 m`) |

### Ctrl-D and Ctrl-\

| Check | Result |
| --- | --- |
| `Execute` of a script (`Wait 20`, then `Echo`), Ctrl-D during `Wait` | `Shell: ***Break` after the `Wait`; the `Echo` did not run (as in `CON:`: `Wait` does not check Ctrl-D, the script stops at the next command) |
| Ctrl-\ at the Shell prompt (pressed through input.device with `key-inject 0x0d 0x0008`, since QEMU's backslash key does not reach AROS One) | the Shell ended (`Process 10 ending`), title "Session ended (F4 restarts)"; F4 started a new Shell |

The Amiga console's reports and Ctrl-D/E/F in raw mode were checked in
[stage 2](verification-handler-stage2.md) (pool v11-2, AROS One 1.3,
2026-10-15, development builds of the same code, against `CON:`); they
were not repeated here.

### LIMPET: handler, from its archive

Unpacked over the same drawer: `Install-Handler`;
`NewShell WINDOW "LIMPET:300/200/600/300/Handler/CLOSE"` opened a window;
`limpet-console` there reported `Limpet 0.6.0`; `winsize-check` gave 13
rows, 70 columns; `EndCLI` closed it; `Remove-Handler` answered
`LIMPET: removed`.

### End

Closing the Limpet window (RAmiga+Q after `EndCLI`) left no Limpet process
in `Status`. `hello` in `CON:` and redirected to a file printed plain text.

## Not checked

- Mainline, ABIv11 2026.09, real hardware.
- The Amiga console's reports and Ctrl-E/F with the release build (see
  stage 2 above for the development build).
- Two programs reading one console at once.
