# Limpet documentation

For users:

- [Why Limpet](why-limpet.md): what it is, what it adds to the Shell, and a
  tested comparison with `CON:`.
- [Compatibility and limits](compatibility.md).
- [Changes](changes.md) of the terminal, release by release.
- [Roadmap](roadmap.md): ports and features planned after 0.1.

For programmers:

- [Getting started with the SDK](sdk/getting-started.md).
- [What to use, and when](sdk/choosing.md).
- [Capability catalogue](capabilities.md): the authoritative list of
  features, detection and test status.
- [SDK API reference](sdk/api.md).
- [Protocol reference](protocols.md): every escape sequence and DOS packet
  Limpet accepts.
  - [Presentation query](presentation.md): operation 13, field by field.
  - [Kitty graphics subset](kitty-graphics.md): inline images.
  - [Hyperlinks](hyperlinks.md): OSC 8 and plain URLs.
- [Examples](sdk/examples.md).
- [Troubleshooting](troubleshooting.md).
- [Versions and stability](versioning.md).
- [Experimental interfaces](sdk/experimental.md): PTY and termios, not
  released.

For maintainers:

- [Building and releasing the SDK](sdk/releasing.md).
- [Verification record for Limpet 0.3.0 and SDK 0.3.0](verification-0.3.0.md).
- [Verification record for Limpet 0.2.1](verification-0.2.1.md).
- [Verification record for Limpet 0.2.0 and SDK 0.2.0](sdk/verification-0.2.0.md).
- [Earlier record of the unpublished SDK 0.1.0](sdk/verification-0.1.0.md).
- [Contributing](../CONTRIBUTING.md).

Source layout: `src/screen_model.c` is the terminal model (parser, grid,
scrollback, reflow); `src/aros_xterm.c` the Intuition window;
`src/aros_session.c` serves the AROS console packets to the Shell;
`src/theme.c` colours and pens; `src/kitty_graphics.c` images;
`sdk/` the public SDK, whose `include/limpet/tty_control.h` is also the
terminal's protocol header; `tests/` and `sdk/tests/` host tests run by
`make test`.
