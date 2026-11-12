# LIMPET: — Limpet as a console handler (experimental)

`LIMPET:` is a DOS console handler. It opens a Limpet window for
`NewShell WINDOW LIMPET:` or an application calling
`Open("LIMPET:0/0/640/200/Title", ...)`. It is **experimental**. Programs
request `LIMPET:` explicitly; `CON:` stays the system's unless
[`LimpetCON ON`](#optional-con-replacement-experimental) is run.

Released separately from Limpet, as `Limpet-handler-experimental-<version>-<target>.zip`
(`aros-x86_64-abiv11-pc`, `aros-x86_64-abiv1-pc`, `aros-aarch64-abiv1-raspi`;
up to 0.7.0 `abiv11`, `v1-x86_64`, `v1-aarch64`);
the Limpet archive and apkg do not install it. The tests of 2026-10-15
ran under the handler's earlier working name, XTERM:.

Scope of this stage: AROS ABIv11 x86_64, tested on AROS One 1.3 under
QEMU. The handler also runs on mainline x86_64 and on the Raspberry Pi
system, both under QEMU ([0.7.0](verification-0.7.0-v1.md),
[0.7.1](verification-0.7.1.md)). Not run on ABIv11 2026.09 or real
hardware.

The table describes the handler on `main`. `AUTO` as `CON:` has it, the
hand-over of `WINDOW` to the system console and `LimpetCON` came after
0.9.0 and are not in a released archive yet
([run of 2026-11-12](#the-run-of-2026-11-12)). For a short list of the
handler's limits, see the [capability reference](capabilities.md#limpet-console-handler).

## Install and remove

The archive unpacks into the Limpet drawer, beside `Limpet` and its
`Fonts`: `limpet-handler`, `limpet-console`, `limpet-handler-remove`,
`LIMPET.mountlist`, `Install-Handler`, `Remove-Handler` and
`README-Handler.txt`.

    Execute SYS:Utilities/Limpet/Install-Handler SYS:Utilities/Limpet
    NewShell WINDOW LIMPET:

`Install-Handler` assigns `LimpetHandler:` to the drawer and mounts `LIMPET:` until
the next reboot. It writes nothing to `S:`, `DEVS:`, `ENVARC:` or the
startup, and `CON:` is not touched.

    Execute SYS:Utilities/Limpet/Remove-Handler

takes `LIMPET:` off the DOS list again. It refuses while a `LIMPET:` window
is open. The device's node and handler segment stay in memory (a few KB)
until the next reboot: an open that found the device a moment before may
still be starting from them. A reboot removes it as well. (`Assign LIMPET: DISMOUNT` is
not used: on a device without a resident task it would start a new
handler, which opens a window.)

## Names

`LIMPET:` reads the name as `CON:` does
(`rom/filesys/console_handler/support.c`, `parse_filename`):

    LIMPET:left/top/width/height/title/option/option...

| Part | LIMPET: |
| --- | --- |
| left, top, width, height | pixels, as `CON:`; empty keeps the default (a 80x24 cell window at 40/60); a non-digit makes the name bad (`ERROR_BAD_STREAM_NAME`) |
| title | the window's title, instead of `Limpet` |
| `CLOSE`, `NOCLOSE` | close gadget on or off (on by default) |
| `INACTIVE`, `NODEPTH`, `NOSIZE`, `NODRAG`, `NOBORDER`, `BACKDROP` | as `CON:` |
| `SCREEN`name | opens on that public screen, else on the default one |
| `WAIT` | after the program closes its last handle, the window stays until you close it (the title may show another message than "Ended (WAIT)" then) |
| `AUTO` | the open succeeds without a window; the window comes with the first read, write or `WaitForChar`, as in `CON:`. A console opened and closed unused never shows one. Up to 0.9.0 the window opened at once |
| `SIMPLE`, `SMART`, `ALT`, `BOOT` | accepted and ignored, as `CON:` does with the first three |
| `WINDOW`0x... | the system's `CON:` handler opens this console in the caller's window; LIMPET: draws only windows of its own. Up to 0.9.0 the name was refused (`ERROR_BAD_STREAM_NAME`) |

Options match by prefix in `CON:`'s order, so `C` is `CLOSE` and `S` is
`SIMPLE`, as in `CON:`.

## Keys

The window is a Limpet window: Limpet's line editor (history, Ctrl-R, TAB
completion), selection, copy and paste, scrollback and Find in output, and
the [settings file](search-and-settings.md) read when the window opens.
There is no Project > Reload settings in a LIMPET: window. As in `CON:`,
Ctrl-C, Ctrl-D, Ctrl-E and Ctrl-F send their break signal to the program
reading the window (in raw mode they reach it as bytes instead), and
Ctrl-\ ends input once: what was typed goes to the program without a line
end, its next read gets end of input, and later reads wait for keys again.
At the Shell's prompt that ends the Shell. **F5** still ends the window's
input for good.

## Compatibility with CON:

Measured with `tests/con_contract.c`, which opens the console by the name
it is given and runs the same checks against `CON:` and `LIMPET:`
(`make abiv11-xterm-console` builds it as `con-contract`). "Test" names the
check in its log, or the runtime step of the [verification
records](verification-0.5.0.md#limpet-handler-experimental) and
[stage 2](verification-handler-stage2.md).

| Contract | LIMPET: | Where | Test |
| --- | --- | --- | --- |
| One window per open; `*` and `CONSOLE:` reach it | supported | DOS starts a handler per open; `aros_session.c` FIND | `open_star`, `open_console` |
| Name: geometry and title | supported | `console_name.c`, `aros_xterm.c` | `test_console_name`; NewShell at 300/200 700x420 |
| Name: window options | supported; `WINDOW` by the system console | as above | `test_console_name`; NOSIZE/NODEPTH/NODRAG and WAIT seen; `SCREEN`, `NOBORDER`, `BACKDROP`, `INACTIVE` not run |
| `AUTO`: no window before the first read or write | supported | `aros_xterm.c` (the open is answered first), `aros_session.c` | `con-contract ... auto`: `auto.deferred`, `auto.opens`, `auto.unused` |
| `WINDOW`: a console in the caller's window | by the system's `CON:` | `tools/xterm_handler.c` (`fall_back`) | `con-contract DEVICE: ... window`: the same results for `CON:` and `LIMPET:` |
| `IsInteractive` on every handle | supported | FIND sets `fh_Interactive` | `interactive`, `dup.interactive` |
| `DupLockFromFH`, `OpenFromLock`, `UnLock` (Run, System) | supported; at most 16 locks out | `ACTION_COPY_DIR_FH`, `FH_FROM_LOCK`, `FREE_LOCK` | `dup_lock`, `open_from_lock`; `Run >*` jobs |
| `Read` cooked: a line with Return | supported, with Limpet's editor instead of CON:'s | `console_input.c`, `line_editor.c` | `cooked.read`; Shell, `Ask`, `Copy ?` |
| `Read` on any handle of the console | supported (fixed in this stage) | `aros_session.c` READ | `cooked.read` on a `MODE_NEWFILE` handle |
| `SetMode` raw and cooked; no echo in raw | supported | `ACTION_SCREEN_MODE` | `set_mode.*`, `raw.read`, `raw.no_echo_left` |
| `WaitForChar` with timeout and poll | supported | `ACTION_WAIT_CHAR` | `wait_char.poll`, `wait_char.timeout` |
| Several readers at once | supported: reads wait in line (up to 16 behind the one served) and take input in order, as in CON:; a cancel finds a read anywhere in the line; end of input and closing answer all | `aros_session.c` READ, `read_next` | `readers.fifo` (CON: and LIMPET:) |
| Several `WaitForChar` at once, also beside a read | supported, each with its own timeout; a key or a partly typed line answers all. Better than CON: here: ABIv11's CON: keeps one waiting packet and loses the others, and a wait beside another process's read hung the guest once | `aros_session.c` WAIT_CHAR, `wait_start` | `wait.with_read`, `wait.two` |
| `Write` from several processes at once | supported; up to 16 wait in order | `aros_session.c` WRITE | `writers` (3x300 writes; passed before the change too) |
| Ctrl-C to the reader | supported; the program reading gets it, a background job that only writes does not (CON: gives it to the last to read or write) | `aros_break.c` | `break`; `Wait 20` + Ctrl-C |
| Ctrl-D, Ctrl-E, Ctrl-F | supported: cooked mode signals the reader (the same one Ctrl-C goes to; before anyone reads, the program that opened the console, as in CON:), the line is kept; raw mode gives the byte and no signal | `aros_session_break_signal` | `break_d`, `break_e`, `break_f`, `raw.ctrl_d`; Ctrl-D stopped an `Execute` script in both consoles |
| End of input from the keyboard | supported: Ctrl-\ once, typed text first without a line end, then one 0 read, then reads wait again; at the prompt the Shell ends. F5 also, for good (a Limpet extra). Differs: Ctrl-\ waits its turn behind typed keys and pastes and until a program reads (CON: marks the end at once); `WaitForChar` with an end pending answers TRUE at once, so the next read returns 0 (CON: waits out the timeout); Ctrl-C clears a pending end | `aros_session_end_input` | `eof.partial`, `eof.zero`, `eof.once`; Shell ended in both |
| Close gadget while a program reads | supported: the read ends, the Shell ends with the window | `aros_session_stop` | NewShell windows closed |
| Program ends before the window closes | supported: the last `Close` closes the window, `WAIT` keeps it | `aros_session.c` END; `aros_xterm.c` | `close`; `EndCLI`; `Status` |
| `ACTION_SEEK` (Flush of input after a command) | supported: -1 at once (error 219; CON: 209) | | `seek` |
| `ExamineFH`, `NameFromFH`, `Info` on the handle | as CON:: `ERROR_ACTION_NOT_KNOWN` | default case | `examine_fh` |
| `ACTION_CHANGE_SIGNAL` | supported; the old value starts at 0 (CON: the opener) | | `change_signal` |
| `ACTION_DISK_INFO`: window in `id_VolumeNode` | supported (fixed in this stage; was -1) | `aros_session.c` | `disk_info.fields` |
| `ACTION_DISK_INFO`: `id_InUse` as a console.device request (posixc `TIOCGWINSZ`) | partly: an `IOStdReq` whose unit is a `ConUnit` with the grid's size and the window, read-only (fixed in this stage; was 0, and `TIOCGWINSZ` crashed) | `aros_session.c` | `disk_info.unit`; `tty-winsize` |
| I/O sent to that console.device request | not supported: it is no device; a program doing it fails | | none |
| Amiga console reports: window bounds (`CSI [0] SPACE q`), cursor (`CSI 6 n`), with the 8-bit or the 7-bit CSI | supported, one-based as AmigaOS and mainline AROS answer: `0x9B 1;1;rows;columns SPACE r`, cursor in the query's introducer; the new size after a resize. ABIv11's CON: answers both one lower | `screen_model.c` | `test_amiga_reports`; `report.*`, `report.*.after_resize` |
| Console geometry (`ACTION_CONSOLE_GETGEOMETRY`, `0x434f4e47`) | supported: columns, rows and the pixel size of whole cells, as in [protocols](protocols.md#dos-packets-public-console-geometry); upstream AROS PR #1763 is still open | `aros_session.c` | `console-geometry-probe` |
| Line feed mode (`CSI 20 h` / `CSI 20 l`) | supported as console.device has it: on from the start in every console window, cooked and raw, so a line feed also returns the carriage; `CSI 20 l` turns it off, `CSI 20 h` on. VT and PTY sessions start with it off | `screen_model.c` | `lf.control`; `test_amiga_reports` |
| Amiga raw input events (`CSI n {`) | not supported (ABIv11 CON: does not report them either) | | none |
| Output: Amiga console sequences | partly: Limpet's VT subset with SGR 30-37/40-47 as pens; not checked one by one against console.device's set | `screen_model.c` | Shell prompt, `List`, `Search` |
| Memory per window | about 32.5 MiB (fonts read per window) | `font_cache.c` | measured 2026-09-25 on ABIv11 2026.09 |
| Open and close cycles | not established: free memory 86,864 B lower after the first 5 windows, 544 B lower after the next 5 (about 109 B each). Whether that is the handler leaking, memory the system keeps for libraries and segments, or neither, was not found | | 10 cycles from a script, `Avail FLUSH` |

VT sequences, Unicode rendering and inline images are separate from
compatibility with the Amiga console interface.

### Source layout

- **The Shell** (`workbench/c/shell`) is unchanged and does not know the
  console. It assumes cooked mode and reads byte by byte from the DOS
  buffer; all editing is the console's.
- **The handler and DOS packets**: `tools/xterm_handler.c` (the launcher
  DOS starts per open, and the hand-over to the system console),
  `tools/xterm-console/limpet_con.c` (`LimpetCON`), `src/aros_handler.c` (startup), `src/aros_session.c`
  (the packets above), `src/console_name.c` (the name).
- **console.device**: not used. `CON:` draws through a console.device unit;
  Limpet draws itself, so a program that reaches for the unit gets the
  read-only `ConUnit` and nothing more.
- **Limpet's window**: `src/aros_xterm.c` (`-DXTERM_DOS_HANDLER`): the
  renderer, editor and keys of an ordinary Limpet window.

## Remaining compatibility gaps

1. **Reports are one-based, ABIv11's CON: is one lower.** LIMPET: follows
   AmigaOS and mainline; a program tuned to ABIv11's CON: sees one row and
   column more than it expects there. This is an intentional compatibility difference.
2. **An `AUTO` window does not come back.** `CON:` closes an `AUTO`
   window at its close gadget and opens it again at the next read or
   write. A Limpet window that is closed ends the console: reads see the
   end of input and writes fail. The fonts and the scrollback are loaded
   when the console is opened, not with the window, so an unused `AUTO`
   console still costs the memory of gap 5.
3. **`WINDOW` is not Limpet's.** The system console serves it, so such a
   console has none of Limpet's features. Up to 0.9.0: refused; programs that hand the console their own window
   get an error.
4. **No console.device behind `ACTION_DISK_INFO`.** Only reading the
   `ConUnit` works.
5. **32.5 MiB and a font load per window**, and a start of 15-25 s under
   QEMU TCG (window state not controlled). Many small `CON:` windows (each
   `Run`-started tool with its own window) would cost much more than
   with `CON:`.
6. **On mainline** DOS keeps the handler's own port, and `CliInit` puts
   requesters on the console's window. The contract tests above pass there
   (0.7.0, mainline `74a2d80b` under QEMU); requesters were not checked.

## Optional CON: replacement (experimental)

    SYS:Utilities/Limpet/LimpetCON ON
    SYS:Utilities/Limpet/LimpetCON OFF
    SYS:Utilities/Limpet/LimpetCON STATUS

`LimpetCON` is in the handler archive, beside `limpet-handler`. It needs
no `Install-Handler` and no `LIMPET:` device.

- **ON** puts Limpet's launcher into the `CON` device node under the DOS
  list lock, after saving the node's handler, segment, stack and priority.
  From then on `NewShell`, `Run` with a console window and every program
  that opens `CON:` get a Limpet window.
- **OFF** puts the saved values back. **STATUS** says which console
  `CON:` is (return code 0 for Limpet, 5 for the system's).
- **After boot only, nothing on disk.** Nothing is written to `S:`,
  `DEVS:` or `ENVARC:`, so a reboot always restores the system's `CON:`.
  `RAW:` and the boot console are not changed.
- **Open windows stay as they are.** A window keeps the console that
  opened it, across ON and OFF. Neither handler's segment is unloaded;
  the launcher's segment and the saved state (a few KB) stay until the
  reboot.
- **The system console as the fallback.** A name with `WINDOW`, and any
  console Limpet does not bring up (a missing `limpet-console`, no fonts,
  no memory, a name it refuses), goes to the saved system handler: the
  launcher starts it as DOS would and gives it the open. Where DOS sends
  a handle's packets to the port the open went to (ABIv11), the launcher
  stays and passes them on until the handle is closed; where the handler
  names its own port in the handle (mainline), it ends after the open.
  The same hand-over serves `LIMPET:...WINDOW`.

Not done yet, and the reason this stays experimental:

- **Every window loads its own fonts and scrollback** (gap 5). Programs
  that open many small console windows cost much more memory and start
  slower than with the system's `CON:`. One font load shared by all
  windows is the next step.
- **No round of programs started from Workbench** with console windows,
  and requesters on a Limpet console window were not checked (gap 6).
- **No line for the startup.** `LimpetCON ON` is typed after every boot.
  A later stage may offer a line in `S:User-Startup`, never
  `S:Startup-Sequence`.
- **`RAW:`** stays the system's until raw-first opens are covered.

### The run of 2026-11-12

`con-contract` (the quiet checks, `auto` and `window`) against `LIMPET:`,
against `CON:` with `LimpetCON ON`, and against the system's `CON:`
before and after, under QEMU:

| System | Result |
| --- | --- |
| mainline x86_64, nightly of 2026-11-10 | no failed check; `NewShell` opened a Limpet window with ON and the system console after OFF |
| AROS One 1.3 (ABIv11) | the same |

Not run: the Raspberry Pi build, ABIv11 2026.09, real hardware, keys
through a console in the caller's window, programs started from Workbench.

## Building

One build per target, named `<system>-<arch>-<abi>-<platform>`:

| Target | System |
| --- | --- |
| `aros-x86_64-abiv11-pc` | AROS One and other ABIv11 systems |
| `aros-x86_64-abiv1-pc` | mainline AROS, x86_64 |
| `aros-aarch64-abiv1-raspi` | AROS on the Raspberry Pi |

    make target TARGET=aros-x86_64-abiv1-pc LIMPET_VERSION=0.7.1 LIMPET_DATE=... \
        T_CC=.../x86_64-aros-gcc T_SDK=.../AROS/Developer
    tools/release/make-packages.sh 0.7.1 OUT aros-x86_64-abiv1-pc

`mk/target-<target>.mk` says what each target expects: for ABIv11 `T_CC`
and `T_SDK` (or `ABIV11_GCC` and `ABIV11_SDK`), for the Pi `T_ROOT` (the
Clang wrapper toolchain with its `sdk/`) and `T_CXX_INCLUDE` (libc++
headers). Everything lands in `build/<target>/`: `Limpet`,
`limpet-console`, `limpet-handler`, `limpet-handler-remove`, and the probes
in `build/<target>/probes/`. The scripts and the mountlist are in
`tools/xterm-console/`. Each target's two archives are
`Limpet-<version>-<target>.zip` and
`Limpet-handler-experimental-<version>-<target>.zip`.

Releases up to 0.7.0 used the `abiv11-*` rules and the targets `v1-x86_64`
and `v1-aarch64`. Built from the 0.7.0 sources, the new targets give the
released binaries of all three systems byte for byte.

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
