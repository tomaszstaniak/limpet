Limpet PTY preview @VERSION@ for mainline AROS (x86_64, ABI v1)
================================================================

A PREVIEW, separate from the ABIv11 package. Limpet is a terminal for the
AROS Shell, written for AROS (not a port of X11 xterm).

Author: Tomasz Staniak, https://tomaszstaniak.com
Project: https://github.com/tomaszstaniak/limpet

This build runs the Shell and programs in a pseudo-terminal (PTY), the way
Unix terminals do, instead of the AROS console protocol.

It runs on mainline AROS x86_64 only. It was tested on the pool base
v1-20260923 with the stock system libraries.


What needs what
---------------

- The terminal itself, a Shell in it, AROS commands and DOS programs: stock
  mainline. Nothing to install into the system.
- Programs that use POSIX terminal functions (termios, window size, raw
  mode through libc), e.g. Rust programs built with crossterm or Ratatui:
  these need an experimental posixc.library. That library is NOT included
  here and is not part of AROS; the tests of such programs used it.
  Without it, such programs are not expected to work; that case was not
  tested.
- The Ratatui/Crossterm demo last passed on an earlier build (2026-09-27)
  with that library. It was not repeated on this package, so no support for
  it is claimed here.


Install, run, uninstall
-----------------------

Mainline AROS does not come with UnZip: unpack the archive on another
computer and copy the LimpetPTY drawer into SYS:Utilities, or, with an
UnZip you installed:

    UnZip Limpet-PTY-@VERSION@-aros-x86_64-abiv1-pc.zip -d SYS:Utilities

Then:

    SYS:Utilities/LimpetPTY/LimpetPTY      (or double-click its icon)
    ...
    EndCLI, then close the window
    Delete SYS:Utilities/LimpetPTY ALL

Menu: Project > About, Close window (RAmiga+Q); Edit > Copy (RAmiga+C),
Paste (RAmiga+V).

Keys in this build:

    Ctrl-C   break the running command (also delivered as a key to raw-mode programs)
    Ctrl-D   end of input for the running program
    F5       hang up (ends the Shell)
    F4       start again after the Shell has ended


Known limitations
-----------------

- Memory, a mainline system issue: every program that opens posixc.library
  leaves about 164 KB behind (system report in the source tree). The
  terminal pays it in some of its starts, on average about 17 KB per
  session measured over 100 sessions. Keeping one terminal open and
  restarting with F4 should avoid the terminal's share; that was not
  measured.
- The terminal's own residue is about 0.2 KB per session, as on ABIv11.
- The window stays open, without a Shell, while background jobs started
  with Run still use the terminal.
- A Run with redirected output leaves the file empty (system Run).


Licences: Limpet is under the MIT licence (LICENSE.txt); bundled
components and fonts as in the ABIv11 package (Licenses/). FreeType is not
included; the system's freetype2.library is used.
