# Experimental interfaces

**Nothing on this page is in a public release.** It describes source code
in this repository that runs only in an unpublished mainline preview. It is
here so that people following the work know what exists and what it would
take. Do not ship a program that depends on it.

## What exists

| Part | Source | What it does |
| --- | --- | --- |
| Pseudo-terminal | `src/tty_pair.c`, `src/tty_service.c`, `src/aros_tty_worker.c` | a master/slave pair served as a DOS handler |
| Control operations 1 to 12 | `sdk/include/limpet/tty_control.h`, `src/tty_control.c` | terminal settings, size, flush, drain, flow control |
| Cancellation | `XTY_ACTION_CANCEL` | cancel a pending read, write or drain; the Shell session cancels reads only (below) |
| Immediate I/O | `XTY_ACTION_TRY_READ`, `XTY_ACTION_TRY_WRITE` | read or write without waiting for data or space |
| Client calls | `src/aros_tty_client.c` | `aros_tty_control_break`, `aros_tty_read_try`, ... |
| termios layer | `src/aros_tty_termios.c` | `tcgetattr`, `tcsetattr`, `tcdrain`, `tcflush`, `tcflow` on a DOS handle |
| C library bridge | `src/aros_tty_fd_bridge.c` | maps a POSIX descriptor to the PTY; needs an experimental `posixc.library` |

## Where it runs

On mainline AROS x86_64, in a PTY build of Limpet that is not published,
with an experimental `posixc.library` that is not part of AROS. A Ratatui
program and an SSH client preview have run there.

## What Limpet 0.1.0 and 0.2.0 do with it

A Limpet Shell session on ABIv11 answers every control operation except 13
with `ERROR_ACTION_NOT_KNOWN`, and does not accept `XTY_ACTION_TRY_READ` or
`XTY_ACTION_TRY_WRITE`. A program that sends `XTY_CAPS` there learns
nothing.

`XTY_ACTION_CANCEL` is the exception, after 0.4.0 (not yet released): the
session takes back a **read** the same client still has out, by the
packet's identity, as the header describes. The read is answered first
(-1, `ERROR_BREAK`), then the cancel (`DOSTRUE`), both to the client's
port. A read already answered gives `DOSFALSE` with
`ERROR_OBJECT_NOT_FOUND`. There is no capability bit for it on this
endpoint: try it, and treat a refusal (`ERROR_ACTION_NOT_KNOWN` from
Limpet 0.4.0 and earlier, and from CON:) as "not here", with the read
still out. Hardcode takes its read back this way at exit.

## Rules already fixed by the protocol

These hold for the PTY preview and will hold for any release that serves
these operations:

- `XTY_CAPS` describes the TTY endpoint, not its renderer. It never implies
  colour or any presentation feature. Use operation 13 for those.
- Negotiate a capability bit before using the operation or field it
  guards (for example `XTY_CAP_VINTR` before setting `values[9]`).
- `XTY_SET_CONFIG_DISCARD_INPUT` is not POSIX `TCSAFLUSH`: it does not
  wait for output.
- Immediate I/O returning -1 with `ERROR_OBJECT_IN_USE` means "try again",
  not end of file.
- The block is native-endian and synchronous, with no embedded pointers.
  The caller owns it until the DOS reply.

## What would make it public

A PTY build for the released ABI, a `posixc.library` that the system ships
or that a program can rely on, and acceptance tests of real POSIX programs
on that system. Until then, the SDK will not expose these calls.
