LIMPET: console handler for Limpet @VERSION@ -- experimental, AROS on Raspberry Pi (ABIv1, AArch64)
===================================================================================================

LIMPET: opens a Limpet window where a program would open CON:, for
example NewShell WINDOW LIMPET:. It is an experiment: it does not replace
CON:, programs that open CON: keep the system console, and some things
CON: does are missing (below). Run under QEMU's Raspberry Pi 3 model
only, not on a real Pi.

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
were not tried on AROS). With AUTO the window opens at the first read or
write, as in CON:. WINDOW0x... (a console in the caller's own window) is
opened by the system's CON: handler instead.

CON: itself (experimental)
--------------------------

    SYS:Utilities/Limpet/LimpetCON ON

makes CON: open Limpet windows, so NewShell and programs that open CON:
get one. LimpetCON OFF gives CON: back to the system; LimpetCON STATUS
says which it is. Nothing is written to disk: a reboot always restores
the system's CON:. Windows that are open keep the console that opened
them. RAW: and the boot console are not changed. What Limpet cannot serve
(WINDOW0x..., a console that does not come up) the system's handler opens
instead. Every Limpet window loads its own fonts, so many small console
windows cost much more memory and time than with the system's CON:.
LimpetCON was not run on the Raspberry Pi.

Missing compared with CON:
--------------------------

- Ctrl-\ waits its turn behind keys typed before it; CON: ends input at
  once;
- each window loads its own fonts, which takes memory and time;
- programs that use the console.device behind the window cannot;

Full table and details: doc/limpet-handler.md in the source,
https://github.com/tomaszstaniak/limpet
