# Verification record: Limpet 0.7.0

Limpet 0.7.0 brings together three lines of work: faster drawing for
full-screen programs (the 0.6.2-test1 pre-release), the eza additions (local
file links, the bundled Nerd Font, style and feature bits in the
presentation reply; 0.6.2-test2, which was built and tested but not
published), and the console contracts of handler stage 3 (queued reads, a
timer per `WaitForChar`, line feed mode) together with the public console
geometry packet. SDK 0.5.0 names the new presentation bits.

Scope against `v0.6.1`: everything on the `perf-down` line (redraw
scheduling, buffered painting, synchronized output, capability reply,
`file://` links, Nerd Font packaging, console geometry packet), merged with
handler stage 3 (`34a68a2` queued reads and waiters, `b02002f` line feed
mode, `82fbe3f` a partly typed line wakes waiters), then test and
documentation commits. While merging, end of input kept the 0.6.1 answer:
`WaitForChar` true, so the next `Read` returns 0. An independent review of
the merge found no handler bug; it found three native test inputs and one
handler document line that still expected the older behaviour, all fixed
before the build.

## Build

| Item | Value |
| --- | --- |
| Sources | branch `release/0.7.0`; binaries built from the tree of its commit "Handler ReadMe: ..." (only documentation changed after it) |
| Compiler | `x86_64-aros-gcc` 10.5.0 |
| SDK | ABIv11 2026.09 Developer SDK |
| Commands | `make abiv11-frontend LIMPET_VERSION=0.7.0 LIMPET_DATE=24.10.2026`, then `abiv11-handler` and `abiv11-xterm-console` with the same variables, then `tools/release/make-packages.sh 0.7.0 OUT abiv11` |
| Program | 3,432,840 bytes, 11,136 more than 0.6.1 |
| Archives | `Limpet-0.7.0-abiv11.zip` (SHA-256 `5ef9188e8cdd39f4fb8f9c27f55424cb672d706e6e4555f831061b74770b91c6`), `Limpet-handler-experimental-0.7.0-abiv11.zip` (`155734692588dbbf98029e34cc21eef798e3b1e8a8edfa0bf57190eb222ea74f`) |

Also built, not run: the mainline (ABIv1) frontend, handler worker and the
endpoint test with the mainline GCC 10.5 toolchain and SDK. That shows the
sources compile for mainline; it is not a mainline runtime result.

## Host tests

`make test` passes (43 Python tests, fixture check, replay corpus, SDK
contract and compatibility tests). Changed or new:

- `tests/test_amiga_reports.c`: line feed mode on in console sessions,
  `CSI 20 l` / `CSI 20 h` with both introducers, a reset following the
  session again, VT sessions off by default.
- `sdk/tests/test_sdk_compat.sh`: SDK 0.4.0 sources (vendored in
  `sdk/tests/compat-0.4.0`) decode the 0.7.0 reply as they decoded 0.6.0's,
  and an object built with the 0.4.0 header links against the 0.5.0 library.

## On AROS

AROS One 1.3 (ABIv11), pool slot v11-1, QEMU TCG, 1024x768, 2026-10-24, one
boot. Everything ran from the two release archives unpacked to `RAM:`.

| Check | Result |
| --- | --- |
| `Version` | Limpet 0.7 for the program and the handler worker |
| Packet endpoint test (`tests/test_aros_endpoint.c`, built for ABIv11) | all seven groups pass, including the new one: three reads and two `WaitForChar` out at once, the middle read cancelled (answered BREAK, then the cancel TRUE), then stop: the other reads answered 0, the waiters TRUE, nothing left parked, the session disposed |
| Console geometry probe in a Limpet window | snapshot 80x24, 640x408 px, agrees with the private grid; short, negative, version 0, unknown version and null buffer refused with error 115 and the buffer unchanged; invalid handle refused |
| The same after resizing the window | 109x33, 872x561 px, agreeing; `TIOCGWINSZ` 33x109 |
| `TIOCGWINSZ` probe | 24x80 before the resize |
| `con-contract ... shared` on `LIMPET:300/200/700/400/cc` | `END failures=0`: two readers served in order (`one`, `two`), a waiter beside a read answered TRUE when a key came, two waiters with no key both answered FALSE after their timeouts, line feed mode on by default, off after `CSI 20 l`, on after `CSI 20 h`, raw and cooked mode switches, 3 writers x 300 writes |
| `read-cancel` in a Limpet window | the cancelled read answered -1/304 before the cancel (TRUE); the next key reached a plain `Read` |
| `eza -l --icons=always --hyperlink=always` (eza 0.23.5 candidate 11) | icons drawn, one cell each, columns aligned |
| OSC 8 `file:///RAM%20Disk:t/notes.txt` | hover shows the URL in the title; Ctrl+click opened the file in the default text tool with its contents |
| OSC 8 `https://www.aros.org/` | hover shows the URL; Ctrl+click started OWB through OpenURL with exactly that address (OWB then reported its own certificate error for the site in this guest) |
| `font=FONTS:TrueType/VeraMono.ttf` in `ENV:Limpet/Limpet.conf` | a new window used Vera Mono; eza drew no icons there; colours unchanged. The file was deleted afterwards |
| Helix 25.07.1 preview2-test1 | opened a file, scrolled 25 lines, inserted a line, quit with `:q!`; the Shell screen came back intact |
| `NewShell WINDOW "LIMPET:.../CLOSE"`, close gadget with `abc` typed and no Return | the Shell ended; no process left |
| `Remove-Handler` | "LIMPET: removed" |
| `Status` at the end, after closing Limpet | only the system's processes |
| SDK `hello` | see [SDK 0.5.0](sdk/verification-0.5.0.md) |

The apkg upgrade from 0.6.1 was run after publishing, from the release
asset; see the catalogue's report for Limpet 0.7.0.

## Not checked

- Mainline runtime; ABIv11 2026.09 base; real hardware.
- More than two readers or waiters at once on a real window (the endpoint
  test covers three reads and two waiters); the queue limits (16).
- Clipboard exchange with ConClip or ConsoleNG.
