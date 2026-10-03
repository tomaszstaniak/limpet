# Hyperlinks

Limpet supports HTTP/HTTPS hyperlinks independently of PTY,
SSH transport, SDL and terminal graphics protocols.

- Hover a link to underline it and see its actual target in the window title.
- **Ctrl+left-click** hands the target to optional `openurl.library`.
- **Ctrl+Shift+left-click** copies the target, not an OSC 8 label, using the
  existing asynchronous UTF-8 clipboard writer.
- An ordinary drag still selects text. A Ctrl gesture over a link takes priority
  over TUI mouse reporting only when no application button is already captured.
  Shift without Ctrl retains the selection override for TUI applications.
- A click is cancelled by dragging, changing output, keyboard input, resizing or
  losing focus. The target is checked again on release.

## Explicit links and plain text

Applications may emit OSC 8, for example:

```text
ESC ] 8 ; ; https://aros.org ESC \ AROS website ESC ] 8 ; ; ESC \
```

BEL also terminates OSC. Sequences may span writes. An empty target closes the
link. Parameters (including `id`) are bounded but not otherwise interpreted.
SGR reset does not close a link. Wide and combining labels work because the
target is cell metadata, not inferred from the label. Metadata follows output
into scrollback and survives alternate-screen save/restore; erasure removes it.

Plain ASCII `http://` and `https://` targets are detected without application
changes, including across soft wraps, but never across hard newlines. Sentence
punctuation and unmatched closing brackets are excluded. Detection is deliberately
conservative; it is not a complete browser URL parser. Non-ASCII addresses need
ASCII/percent-encoded URLs rather than a partially clickable prefix.

## Bounds and safety

Targets are at most 2047 bytes; oversized targets are rejected, not truncated.
There are 64 retained explicit targets per terminal instance. Evicted cells become
inert, never rebound to a different target. Plain detection does not consume this
store. Invalid explicit targets cannot fall back to interpreting the label.

Only HTTP/HTTPS with a nonempty authority are accepted. Userinfo, whitespace,
controls and non-ASCII URLs are rejected. Opening additionally rejects raw quote,
apostrophe, backtick, dollar, star, backslash, angle-bracket and pipe characters.
Percent escapes must be valid and are passed unchanged, never decoded by Limpet.

This extra check matters because mainline OpenURL expands `%u` into configured
browser command/ARexx templates. Limpet does not construct or execute a Shell
command itself. OpenURL preferences and the configured browser remain trusted
local configuration; use the normal quoted `%u` templates. An allowed network
address is not a claim that its destination is trustworthy.

Nothing opens automatically, including output received over SSH. No OSC 52
clipboard writes, `file:`, `mailto:`, executable links or remote-control command
schemes are enabled by this feature.

## Optional browser integration

The opener dynamically opens `openurl.library` in a worker process. Neither that
library nor a browser is required to start Limpet or copy links. Completion/error
status appears in the title. Closing the terminal waits for an outstanding opener
worker, as it does for clipboard work; a stuck external OpenURL request can delay
final close, but does not synchronously block the GUI event loop.

Long target previews are abbreviated in the title (and may be visually clipped
by narrow windows). Copy provides the complete target; no launch confirmation
dialog or persistent status bar is implemented in this step.
