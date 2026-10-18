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

Mention whether the problem also occurs in `CON:`; this helps distinguish
terminal-specific behaviour from shared system behaviour.

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

Update [doc/capabilities.md](doc/capabilities.md) when adding or changing a
feature. Distinguish released functionality from unreleased development and
experimental interfaces. Keep detailed test results in verification records.

Public release notes describe user-visible changes, installation and relevant
limitations. Do not include internal test inventories, CI logs or acceptance
reports; link to the verification record only where it is useful.
