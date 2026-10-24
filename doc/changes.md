# Limpet changes

Changes of the terminal. The SDK has its own list in
[sdk/CHANGES.md](../sdk/CHANGES.md).

## Unreleased

Fixed:

- Full-screen programs scrolled slowly, and after a held cursor key they
  kept moving for seconds after the key was released. Such a program sends
  one screen in many small writes, and Limpet redrew the window after each
  one, so the program waited for every partial redraw. Limpet now redraws
  once the program's output pauses, and up to about ten times a second while
  output keeps coming (less often when redrawing itself is slow, so the
  program keeps most of the machine). Long output such as `Type` of a large
  file also finishes several times sooner. Keys and window changes still
  redraw at once.
- While a full-screen program scrolled, the window was often seen half old
  and half new, with one line doubled where the halves met (it could look
  like larger letters appearing at the bottom). Limpet now prepares all
  changed lines first and puts them on screen in one go.
- A program that reads keys itself and writes a bare line feed (`\n`) in
  raw mode got stepped lines. In a Limpet window a line feed now returns to
  the left edge, as in the system console. A program that wants a line feed
  to move down only sends `CSI 20 l`.
- A second program reading or waiting on the same window got an error at
  once. Several programs can now read from one window; they get its input
  in turn. Several can wait for a key (`WaitForChar`) at once, each with
  its own timeout, also while another program reads.

Added:

- Synchronized output (`CSI ? 2026 h` / `l`): a program that marks where
  each screen update starts and ends is shown one whole update at a time,
  so a scrolling editor no longer shows lines of two screens at once. A
  program that leaves an update open is still shown after a quarter of a
  second. Programs that do not use the marks are drawn as before.
- Links to local files: an OSC 8 link `file:///Volume:path` (with `%XX`
  escapes for spaces and other bytes) opens with Ctrl+click as a Workbench
  double-click would, through its default tool. HTTP and HTTPS links still
  go to OpenURL. Remote hosts, relative and Unix paths are not accepted.
- The archive includes DejaVu Sans Mono Nerd Font, which is now the default
  font when no `font` is set in `Limpet.conf`. It has the file-type icons
  that `eza --icons` uses, drawn one cell wide. A font set in `Limpet.conf`
  is still used as before.
- Programs can ask Limpet (through the presentation query of the Limpet SDK)
  which text styles it draws, whether it opens OSC 8 links, and whether its
  font has the eza icons in one cell; eza uses this to decide on styles,
  `--hyperlink` and `--icons`.

## 0.6.1 (2026-10-19)

Fixed:

- Letters typed on a keymap of another character set came out wrong:
  Limpet read every character above 127 as Latin-1, so on a Polish keymap
  ą arrived as ± and ż as ¿. Limpet now uses the character set chosen in
  Locale preferences (`ENV:CHARSET`, through codesets.library). Without
  that variable it keeps Latin-1, as before.
- After a full-screen editor quit, the Shell could receive a stray line and
  answer "object not found". Editors such as Helix send `ESC [ 0 SPACE q`
  (the xterm cursor-style reset) when they leave; 0.6.0 read it as the
  Amiga console's window-size query and answered on the input. That query is
  now answered only when it starts with the 8-bit CSI (0x9B), as with the
  cursor query.
- A program that waited for keys with `WaitForChar()` and read only when
  it answered true never noticed that its window had been closed, and the
  window stayed on "Closing: waiting for process". At the end of input
  `WaitForChar()` now answers true, so the next `Read()` returns 0. Writes
  to the closed window fail with `ERROR_DEVICE_NOT_MOUNTED` instead of
  `ERROR_BREAK`, which C libraries report as an interrupted call that
  programs retry.
- At 12 and 14 pixels, letters a pixel wider than the cell (w and W in Vera
  Mono) were drawn smaller and raised, which looked like a gap before them.
  They keep their height and baseline now.

New:

- Full-screen programs such as Helix fill the whole window: the margin
  around the text takes the colour of the cells next to it while a program
  uses the alternate screen, instead of a frame in the theme's background.
- Programs started in a Limpet window see `COLORTERM=truecolor` and can use
  24-bit colour (Helix shows its own theme with it). It is a local variable
  of the window's Shell; nothing is saved to `ENV:` or `ENVARC:`.

Changes for programs:

- `ESC [ 0 SPACE q` and `ESC [ SPACE q` (7-bit) get no answer any more; send
  the window-size query with the 8-bit CSI, `0x9B 0 SPACE q`.
- At the end of input (F5, Ctrl-\, a closed window) `WaitForChar()` answers
  true and `Read()` returns 0. A loop that reads while `WaitForChar(fh, 0)`
  is true must stop when `Read()` returns 0; `CON:` answers false there.
- Writes to a closed window fail with `ERROR_DEVICE_NOT_MOUNTED`.

The experimental [LIMPET:](limpet-handler.md) handler has the same fixes and
the margin colours. `COLORTERM` is set only for sessions that Limpet starts.

## 0.6.0 (2026-10-18)

For users:

- Ctrl-D, Ctrl-E and Ctrl-F work as in the system console: the program
  reading the window gets the break signal (Ctrl-D stops a running script).
- Ctrl-\ ends input, as in the system console: a program reading the
  window sees the end of its input; at the Shell's prompt the Shell ends.
  F5 still ends the window's input for good.

For programs:

- The Amiga console's window-size and cursor queries are answered
  (`CSI 0 SPACE q`, `CSI 6 n`), with the 8-bit or the 7-bit CSI, one-based
  as AmigaOS does.
- Shift+Enter can be told from Enter: a raw-mode program that sends
  `CSI > 4 ; 1 m` receives Shift+Enter as `CSI 27 ; 2 ; 13 ~`. The
  presentation query announces it with `XTY_INPUT_MODIFIED_ENTER`, which
  Limpet SDK 0.4.0 names. `CSI > 4 m`, `CSI > 4 n` and `CSI > m` turn it
  off, and it ends with raw mode like the other input modes.

The experimental [LIMPET:](limpet-handler.md) handler, still a separate
archive, has the same three: the Amiga console's reports, Ctrl-D/E/F and
Ctrl-\.

## 0.5.0 (2026-10-15)

Fixed:

- **A POSIX program that asked for the window size crashed** in a Limpet
  window up to 0.4.1. `ioctl(TIOCGWINSZ)` in the C library reads the size
  through `ACTION_DISK_INFO`'s `id_InUse`, which Limpet left 0, and died
  with "Illegal address access" in crt.library. It now gets the window's
  rows and columns, also after a resize: `ACTION_DISK_INFO` gives the
  window and a read-only console unit with the grid's size.

For users:

- Find in output: RAmiga+F or Ctrl+Shift+F finds text in the scrollback
  and on the screen; Up and Down go to older and newer matches, Esc goes
  back to where you were, RAmiga+V pastes into the search text. Output that
  arrives meanwhile does not move the view. The alternate screen of a
  full-screen program is not searched.
  [Details](search-and-settings.md#find-in-output).
- A settings file, `ENV:Limpet/Limpet.conf` (or `ENVARC:`): font and font
  size, theme, scrollback lines and command history length. A wrong file is
  ignored whole, with the line named in the title bar.
  Project > Reload settings applies the theme at once.
  [Details](search-and-settings.md#settings).
- The scrollback and the command history are allocated when the window
  opens, at the size set; with no settings they are as before (256 lines at
  160 columns, 32 commands).

For programs:

- Several processes writing to one Limpet console at once wait their turn
  instead of the later ones getting an error.
- The SDK stays 0.3.0: no header or library changed.

Separately, experimental and not in the Limpet archive or apkg:
[LIMPET:](limpet-handler.md), a Limpet window where a program would open
`CON:` (`Limpet-handler-experimental-0.5.0-abiv11.zip`). It does not
replace `CON:`.

## 0.4.1 (2026-10-13)

For users:

- Closing the window while a drop arrives no longer depends on how exec
  schedules interrupts: a small process answers drops while the window
  closes, and the port is freed after it has exited. 0.4.0's close relied
  on a uniprocessor exec. Not tested on an SMP exec; that the new close
  holds there rests on reading the code.

For programs:

- A program can take back a console read it still has out with
  `XTY_ACTION_CANCEL` (read only, by the packet's identity; see
  [the experimental guide](sdk/experimental.md)). Hardcode uses it at exit.
  Limpet 0.4.0 and earlier refuse it. The SDK stays 0.3.0: no header or
  library changed.

## 0.4.0 (2026-10-13)

For users:

- Icons dragged from Workbench onto the window put their full paths on the
  Shell's command line, at the cursor, quoted as the AROS Shell reads them.
  Nothing runs until Enter. Several icons at once, drawers and disks work;
  an icon without its object, a name that is not UTF-8, a running command,
  a program reading keys itself and an open history search are refused,
  with the reason in the title bar. [Details](capabilities.md#file-drop).
- A drop that arrives while the window closes is answered at once, so
  closing the window cannot wait for good on Wanderer (found in review
  before the release; checked with `tests/drop_close.c`).

For programs: nothing changes. The protocol and the SDK are as in 0.3.0.

## 0.3.0 (2026-10-11)

For users:

- The window title says what runs in the window: `Shell - Limpet` at the
  prompt, `Wait - Limpet` while a command runs, the program's own state
  when it reports one (`title demo: receiving - Limpet`), then `Shell`
  again. A program reports its state only once it is built with SDK 0.3.0
  or sends the sequence itself. A link under the pointer still shows its target there.
  Terminal messages follow the name (`| Copied`), report-only ones go after
  about four seconds, and key hints moved to the About window. Jobs started
  with `Run` are not shown. [Details](capabilities.md#window-title).
- Ctrl-R searches the commands typed in the window (the last 32): typing
  narrows, Ctrl-R again goes further back, Enter puts the command on the
  line without running it, Esc puts back what was there.
  [How](capabilities.md#history-search).
- Ctrl-C while a command runs gives one new prompt; it used to give two on
  one line.
- An image near the bottom of the window shows whole: with `C=0` the text
  scrolls up to make room and the cursor ends beside the image's last row.
  `clear` (`CSI 2 J`) removes images with the text, and a full-screen
  program starts on an alternate screen without old images.
- Images stay on their text row when the window is resized with lines in
  the scrollback; they used to vanish.
- A selection is dropped when the screen switches between main and
  alternate or the view moves in the history, even over blank text.
- The title is composed in a buffer Intuition is not drawing from.

For programs:

- OSC 0 and OSC 2 set the window title while the command that sent them
  runs, announced by the presentation query (known bit 64, `values[8]`,
  `XTY_FEATURE_TITLE`); SDK 0.3.0 has `limpet_osc_title()`. Limpet 0.2.x
  does not confirm the feature, and `CON:` prints the sequence as text.

Size: the program is 3,375,824 bytes, 18,288 more than 0.2.1 (+0.54 %). Checks:
[verification-0.3.0.md](verification-0.3.0.md).

## 0.2.1 (2026-10-11)

- HarfBuzz 14.5.0 (was 11.2.1): maintenance update, with upstream fixes in
  shaping and in handling damaged fonts. No new terminal features. The
  clusters of the shaping corpus draw the same as with 11.2.1. With a font
  whose tables are damaged, a cluster may now fall back to another font or
  lose a ligature where 11.2.1 still used the damaged table. Speed is
  not settled: long sessions showed redraws about 1.8x slower, with the
  emulator's window state not controlled; a shorter A/B run without that
  difference was too unsteady to rule out a smaller one. The program grows from 3,199,064 to
  3,357,536 bytes (+4.95 %). Source and build settings:
  [vendor/README.md](../vendor/README.md). Checks:
  [verification-0.2.1.md](verification-0.2.1.md).
- `make test-shaping-corpus`: every cluster of a fixed corpus as the
  renderer draws it, compared with the accepted output.
- `make abiv11-bench-shaping`: a shaping benchmark to run on AROS.

## 0.2.0 (2026-10-07)

For users:

- A selection stays while a program redraws other rows: a status line, a
  clock, a reply streaming below it. Output that changes or scrolls the
  selected text still clears it. Works during a drag, including selections
  larger than 1 KB.
- Erasing a whole row no longer joins it to the row above when the width
  changes later, so a redrawn status panel leaves no joined lines in the
  history.

For programs in raw mode, announced by the presentation query (known bit
32, `values[7]`):

- Bracketed paste, DEC mode 2004: a paste arrives as `CSI 200 ~` text
  `CSI 201 ~`, without the multi-line confirmation. Nothing else is put
  inside the frame; a paste cancelled with Esc still gets its end marker.
- Resize reports, DEC mode 2048: `CSI 48 ; rows ; cols ; height ; width t`
  at once and after each change.
- Wheel reports, private mode 2100: wheel steps as SGR mouse reports while
  buttons and drags stay with the terminal.
- When the console returns to cooked mode, these modes and mouse tracking
  1000/1002/1006 are turned off.

Protocol: version 1 unchanged; known bit 32 and `values[7]` are new, and
0.1.0 clients ignore both.

## 0.1.0 (2026-10-03)

First release, for AROS ABIv11 x86_64.
