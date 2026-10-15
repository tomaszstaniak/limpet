# XTERM: — Limpet as a console handler (experimental)

`XTERM:` is a DOS device that opens a Limpet window wherever a program
would open `CON:`: `NewShell WINDOW XTERM:`, or a program that opens
`XTERM:0/0/640/200/Title` itself. It is the first stage towards letting
Limpet take over what `CON:` does. It is **experimental**: it does not
replace `CON:`, programs that open `CON:` keep getting `CON:`, and the
table below lists what works, what works in part and what does not.

Scope of this stage: AROS ABIv11 x86_64, tested on AROS One 1.3 under
QEMU. Not built or run on mainline, not run on ABIv11 2026.09 or real
hardware.

## Install and remove

The files come from a build (`make abiv11-frontend abiv11-handler
abiv11-xterm-console`, see [Building](#building)); they go into the Limpet
drawer, beside `Limpet` and its `Fonts`:
`xterm-handler`, `aros-xterm-worker`, `xterm-remove`, `XTERM.mountlist`,
`Install-XTERM`, `Remove-XTERM`.

    Execute SYS:Utilities/Limpet/Install-XTERM SYS:Utilities/Limpet
    NewShell WINDOW XTERM:

`Install-XTERM` assigns `LimpetXT:` to the drawer and mounts `XTERM:` until
the next reboot. It writes nothing to `S:`, `DEVS:`, `ENVARC:` or the
startup, and `CON:` is not touched.

    Execute SYS:Utilities/Limpet/Remove-XTERM

takes `XTERM:` off the DOS list again. It refuses while an `XTERM:` window
is open: each window's handler runs code from the device's segment, which
removal unloads. A reboot removes it as well. (`Assign XTERM: DISMOUNT` is
not used: on a device without a resident task it would start a new
handler, which opens a window.)

## Names

`XTERM:` reads the name as `CON:` does
(`rom/filesys/console_handler/support.c`, `parse_filename`):

    XTERM:left/top/width/height/title/option/option...

| Part | XTERM: |
| --- | --- |
| left, top, width, height | pixels, as `CON:`; empty keeps the default (a 80x24 cell window at 40/60); a non-digit makes the name bad (`ERROR_BAD_STREAM_NAME`) |
| title | the window's title, instead of `Limpet` |
| `CLOSE`, `NOCLOSE` | close gadget on or off (on by default) |
| `INACTIVE`, `NODEPTH`, `NOSIZE`, `NODRAG`, `NOBORDER`, `BACKDROP` | as `CON:` |
| `SCREEN`name | opens on that public screen, else on the default one |
| `WAIT` | after the program closes its last handle, the window stays until you close it (the title may show another message than "Ended (WAIT)" then) |
| `AUTO` | accepted; the window opens at once (CON: waits for the first read or write) |
| `SIMPLE`, `SMART`, `ALT`, `BOOT` | accepted and ignored, as `CON:` does with the first three |
| `WINDOW`0x... | refused (`ERROR_BAD_STREAM_NAME`): XTERM: draws its own window |

Options match by prefix in `CON:`'s order, so `C` is `CLOSE` and `S` is
`SIMPLE`, as in `CON:`.

## Keys

The window is a Limpet window: Limpet's line editor (history, Ctrl-R, TAB
completion), selection, copy and paste, scrollback and Find in output, and
the [settings file](search-and-settings.md) read when the window opens.
There is no Project > Reload settings in an XTERM: window. Ctrl-C breaks
the program reading; **F5** ends input (EOF), where `CON:` uses Ctrl-\.

## Compatibility with CON:

Measured with `tests/con_contract.c`, which opens the console by the name
it is given and runs the same checks against `CON:` and `XTERM:`
(`make abiv11-xterm-console` builds it as `con-contract`). "Test" names the
check in its log, or the runtime step of the [verification
record](verification-0.5.0-candidate.md#xterm-stage-1).

| Contract | XTERM: | Where | Test |
| --- | --- | --- | --- |
| One window per open; `*` and `CONSOLE:` reach it | supported | DOS starts a handler per open; `aros_session.c` FIND | `open_star`, `open_console` |
| Name: geometry and title | supported | `console_name.c`, `aros_xterm.c` | `test_console_name`; NewShell at 300/200 700x420 |
| Name: window options | supported, `AUTO` partly, `WINDOW` not | as above | `test_console_name`; NOSIZE/NODEPTH/NODRAG and WAIT seen; `SCREEN`, `NOBORDER`, `BACKDROP`, `INACTIVE` not run |
| `IsInteractive` on every handle | supported | FIND sets `fh_Interactive` | `interactive`, `dup.interactive` |
| `DupLockFromFH`, `OpenFromLock`, `UnLock` (Run, System) | supported; at most 16 locks out | `ACTION_COPY_DIR_FH`, `FH_FROM_LOCK`, `FREE_LOCK` | `dup_lock`, `open_from_lock`; `Run >*` jobs |
| `Read` cooked: a line with Return | supported, with Limpet's editor instead of CON:'s | `console_input.c`, `line_editor.c` | `cooked.read`; Shell, `Ask`, `Copy ?` |
| `Read` on any handle of the console | supported (fixed in this stage) | `aros_session.c` READ | `cooked.read` on a `MODE_NEWFILE` handle |
| `SetMode` raw and cooked; no echo in raw | supported | `ACTION_SCREEN_MODE` | `set_mode.*`, `raw.read`, `raw.no_echo_left` |
| `WaitForChar` with timeout and poll | supported | `ACTION_WAIT_CHAR` | `wait_char.poll`, `wait_char.timeout` |
| Several readers or waiters at once | partly: one; another gets `ERROR_OBJECT_IN_USE` at once (CON: queues them) | `aros_session.c` READ, WAIT_CHAR | from the code; not run |
| `Write` from several processes at once | supported; up to 16 wait in order | `aros_session.c` WRITE | `writers` (3x300 writes; passed before the change too) |
| Ctrl-C to the reader | supported; the program reading gets it, a background job that only writes does not (CON: gives it to the last to read or write) | `aros_break.c` | `break`; `Wait 20` + Ctrl-C |
| Ctrl-D, Ctrl-E, Ctrl-F | not supported | | `break_d` |
| EOF from the keyboard | partly: F5, not Ctrl-\ | `aros_session_eof` | `eof` |
| Close gadget while a program reads | supported: the read ends, the Shell ends with the window | `aros_session_stop` | NewShell windows closed |
| Program ends before the window closes | supported: the last `Close` closes the window, `WAIT` keeps it | `aros_session.c` END; `aros_xterm.c` | `close`; `EndCLI`; `Status` |
| `ACTION_SEEK` (Flush of input after a command) | supported: -1 at once (error 219; CON: 209) | | `seek` |
| `ExamineFH`, `NameFromFH`, `Info` on the handle | as CON:: `ERROR_ACTION_NOT_KNOWN` | default case | `examine_fh` |
| `ACTION_CHANGE_SIGNAL` | supported; the old value starts at 0 (CON: the opener) | | `change_signal` |
| `ACTION_DISK_INFO`: window in `id_VolumeNode` | supported (fixed in this stage; was -1) | `aros_session.c` | `disk_info.fields` |
| `ACTION_DISK_INFO`: `id_InUse` as a console.device request (posixc `TIOCGWINSZ`) | partly: an `IOStdReq` whose unit is a `ConUnit` with the grid's size and the window, read-only (fixed in this stage; was 0, and `TIOCGWINSZ` crashed) | `aros_session.c` | `disk_info.unit`; `tty-winsize` |
| I/O sent to that console.device request | not supported: it is no device; a program doing it fails | | none |
| Amiga console reports in raw mode: window bounds (`CSI 0 SPACE q`), cursor (`CSI 6 n`) | not supported: no answer | | `report.bounds`, `report.cursor` (CON: answers both, 0-based) |
| Amiga raw input events (`CSI n {`) | not supported (ABIv11 CON: does not report them either) | | none |
| Output: Amiga console sequences | partly: Limpet's VT subset with SGR 30-37/40-47 as pens; not checked one by one against console.device's set | `screen_model.c` | Shell prompt, `List`, `Search` |
| Memory per window | about 32.5 MiB (fonts read per window) | `font_cache.c` | earlier measurement, `docs` 2026-09-25 |
| Open and close cycles | about 109 B per cycle after the first ones | | 10 cycles, `Avail FLUSH` |

Not compatibility in the sense of this table: modern VT sequences that
Limpet understands (colours, Unicode, images) are not the Amiga console
protocol, and a program written for `CON:` does not need them.

### Where each part lives

- **The Shell** (`workbench/c/shell`) is unchanged and does not know the
  console. It assumes cooked mode and reads byte by byte from the DOS
  buffer; all editing is the console's.
- **The handler and DOS packets**: `tools/xterm_handler.c` (the launcher
  DOS starts per open), `src/aros_handler.c` (startup), `src/aros_session.c`
  (the packets above), `src/console_name.c` (the name).
- **console.device**: not used. `CON:` draws through a console.device unit;
  Limpet draws itself, so a program that reaches for the unit gets the
  read-only `ConUnit` and nothing more.
- **Limpet's window**: `src/aros_xterm.c` (`-DXTERM_DOS_HANDLER`): the
  renderer, editor and keys of an ordinary Limpet window.

## Gaps before XTERM: could stand in for CON:

Each with how to see it and what it breaks.

1. **No window-bounds or cursor report.** `con-contract` `report.bounds`:
   `CON:` answers `CSI 1;1;15;75 r`, `XTERM:` nothing. A program that asks
   the console for its size this way waits or guesses (UnZip's screen-size
   query is one). High impact for console-aware Amiga programs.
2. **Ctrl-D, Ctrl-E, Ctrl-F are not signalled.** `break_d`. Scripts use
   Ctrl-D to stop; some programs use E and F.
3. **Ctrl-\ is not end of input**; F5 is. Users of `CON:` press Ctrl-\.
4. **One reader at a time.** A second process reading or waiting on the
   same console gets an error at once instead of waiting its turn.
5. **`AUTO` opens the window at once**, so a program started from
   Workbench with an `AUTO` console shows an empty window it never uses.
6. **`WINDOW` is refused.** Programs that hand the console their own window
   get an error.
7. **No console.device behind `ACTION_DISK_INFO`.** Only reading the
   `ConUnit` works.
8. **32.5 MiB and a font load per window**, and a start of 15-25 s under
   QEMU TCG (window state not controlled). Many small `CON:` windows (each
   `Run`-started tool with its own window) would cost much more than
   with `CON:`.
9. **Not run on mainline**, where DOS keeps the handler's own port and
   `CliInit` puts requesters on the console's window.

## Next stage: an optional stand-in for CON:

Not built; a design for review.

- **Opt-in, after boot only.** A command (`LimpetCON ON`) swaps the
  handler of the `CON` device node for `xterm-handler` under the DOS list
  lock, after saving the original handler, segment, startup value and
  stack. `LimpetCON OFF` puts the saved values back. Windows already open
  keep running on the handler that opened them; neither segment is
  unloaded while windows of it may run. Nothing goes into the startup in
  this stage; a later one may offer a line in `S:User-Startup`, never
  `S:Startup-Sequence`.
- **Per open, the original stays one step away.** When `XTERM:` cannot
  serve a name (`WINDOW`, `BOOT`, a screen it cannot open, no memory for
  fonts), its launcher passes the startup packet to the saved `CON:`
  handler instead of failing, so that program gets the system console.
  `RAW:` stays the system's until raw-first opens are covered.
- **Way back without a working window.** `ECON:` and the boot console are
  never touched. `LimpetCON OFF` works from any Shell, and a reboot
  always restores `CON:`, because nothing is written to disk.
- **Before it is offered:** gaps 1 to 5 closed, gap 8 reduced (one font
  load shared by all windows), and the contract probe passing the same on
  both consoles, plus a round of programs started from Workbench with
  console windows.

## Building

    make abiv11-frontend abiv11-handler abiv11-xterm-console \
        ABIV11_GCC=.../x86_64-aros-gcc ABIV11_SDK=.../Development

`build/abiv11/` then holds `aros-xterm` (Limpet), `aros-xterm-worker`,
`xterm-handler`, `xterm-remove`, `con-contract` and `tty-winsize`; the
scripts and the mountlist are in `tools/xterm-console/`.

## Sources read

The contract above was taken from the system's own sources, not from other
consoles: ABIv11 `deadwood2/AROS` master `e33ca77d` (2026-09-15) and
mainline `aros-development-team/AROS` `74a2d80b` (2026-09-04):
`workbench/c/shell`, `workbench/c/shellcommands` (NewShell, Run, EndCLI),
`rom/filesys/console_handler`, `rom/devs/console`, `rom/dos` (open,
fs_driver, newcliproc, systemtaglist, runhandler, read/write/flush,
waitforchar, setmode) and `compiler/crt/posixc/ioctl.c`. KingCON and
ViNCEd were not used: no public source was found for either, and their
licences are not known.
