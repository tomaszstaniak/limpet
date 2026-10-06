# Contributing to Limpet

## Reporting a bug

Open an issue at <https://github.com/tomaszstaniak/limpet/issues> with:

- the Limpet version (`Version SYS:Utilities/Limpet/Limpet FULL`) and how
  it was installed;
- the AROS system: distribution and version, ABIv11 or mainline, real
  hardware or emulator;
- what you did, what you expected, what happened;
- a screenshot if it is about display;
- for a program built with the SDK: the SDK version and the output of the
  SDK's `hello` example run in the same window.

If the problem also happens in `CON:`, say so: it is then probably not a
Limpet problem.

## Changing the code

- Run `make test` before sending a change. It needs only a host C
  compiler.
- A change to terminal behaviour comes with a host test in `tests/` that
  fails without it.
- A change to the SDK comes with a test in `sdk/tests/test_sdk.c`.
- If you ran the change on AROS, write which system and how. If you did
  not, say that too; host tests alone do not show that something works on
  AROS.
- Comments explain why, not what.
- Everything in the repository is in English.

## The protocol header

`sdk/include/limpet/tty_control.h` is shared by the terminal and every
client. Do not renumber, reuse or change the meaning of anything in it.
New fields go into values that are zero today, and need a matching entry
in [doc/versioning.md](doc/versioning.md) and
[doc/capabilities.md](doc/capabilities.md).

## Documentation

A feature is listed in [doc/capabilities.md](doc/capabilities.md) with its
real status: verified on a named system, host-tested, experimental or not
supported. Do not describe a feature as supported because the code exists.
