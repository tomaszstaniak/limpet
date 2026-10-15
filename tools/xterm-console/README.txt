XTERM: for Limpet @VERSION@ -- experimental, ABIv11 (x86_64)
==========================================================

XTERM: opens a Limpet window where a program would open CON:.
It is an experiment: it does not replace CON:, programs that open CON:
keep the system console, and some things CON: does are missing (below).
Tested on AROS One 1.3 under QEMU only.

Install
-------

Copy the files of this archive into the Limpet drawer (beside Limpet and
its Fonts drawer), for example SYS:Utilities/Limpet, then:

    Execute SYS:Utilities/Limpet/Install-XTERM SYS:Utilities/Limpet
    NewShell WINDOW XTERM:

XTERM: stays mounted until the next reboot. Nothing is written to S:,
DEVS: or ENVARC:, and CON: is not changed. To take it off before that:

    Execute SYS:Utilities/Limpet/Remove-XTERM

which refuses while an XTERM: window is open.

Names
-----

As with CON:: XTERM:left/top/width/height/title/options, for example

    NewShell WINDOW "XTERM:40/40/700/400/My Shell/CLOSE"

Options CLOSE, NOCLOSE, WAIT, INACTIVE, NODEPTH, NOSIZE, NODRAG, NOBORDER,
BACKDROP and SCREEN<name> work; AUTO opens the window at once; WINDOW0x...
is refused.

Missing compared with CON:
--------------------------

- no answer to the window-size (CSI 0 SPACE q) and cursor (CSI 6 n)
  queries of the Amiga console;
- Ctrl-D, Ctrl-E and Ctrl-F send no signal; F5, not Ctrl-\, ends input;
- one program at a time may read or wait for keys;
- about 32 MB of memory per window, and a slow start;
- programs that use the console.device behind the window cannot.

Full table and details: doc/xterm-console.md in the source,
https://github.com/tomaszstaniak/limpet
