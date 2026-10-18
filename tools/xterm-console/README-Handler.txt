LIMPET: console handler for Limpet @VERSION@ -- experimental, ABIv11 (x86_64)
==========================================================================

LIMPET: opens a Limpet window where a program would open CON:, for
example NewShell WINDOW LIMPET:. It is an experiment: it does not replace
CON:, programs that open CON: keep the system console, and some things
CON: does are missing (below). Tested on AROS One 1.3 under QEMU only.

Install
-------

Copy the files of this archive into the Limpet drawer (beside Limpet and
its Fonts drawer), for example by unpacking it over SYS:Utilities, then:

    Execute SYS:Utilities/Limpet/Install-Handler SYS:Utilities/Limpet
    NewShell WINDOW LIMPET:

LIMPET: stays mounted until the next reboot. Nothing is written to S:,
DEVS: or ENVARC:, and CON: is not changed. To take it off before that:

    Execute SYS:Utilities/Limpet/Remove-Handler

which refuses while a LIMPET: window is open. A few KB stay in memory
until the reboot.

Names
-----

As with CON:: LIMPET:left/top/width/height/title/options, for example

    NewShell WINDOW "LIMPET:40/40/700/400/My Shell/CLOSE"

Options CLOSE, NOCLOSE, WAIT, INACTIVE, NODEPTH, NOSIZE, NODRAG, NOBORDER,
BACKDROP and SCREEN<name> are read (NOBORDER, BACKDROP, INACTIVE and SCREEN
were not tried on AROS); AUTO opens the window at once; WINDOW0x... is
refused.

Missing compared with CON:
--------------------------

- the window-size (CSI 0 SPACE q) and cursor (CSI 6 n) reports are
  one-based, as AmigaOS answers; the CON: of ABIv11 answers one lower;
- Ctrl-\ waits its turn behind keys typed before it; CON: ends input at
  once;
- in raw mode a line feed moves down without going to the left edge;
- one program at a time may read or wait for keys;
- about 32 MB of memory per window, and a start of 15-25 s under QEMU;
- programs that use the console.device behind the window cannot;
- memory: 10 windows opened and closed in a row left about 87 KB less
  free memory after the first five and about 0.5 KB after the next five.
  Whether that is a leak in the handler, memory the system keeps, or
  neither, has not been established.

Full table and details: doc/limpet-handler.md in the source,
https://github.com/tomaszstaniak/limpet
