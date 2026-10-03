# Limpet documentation

- [Presentation query](presentation.md): how a program asks what the
  terminal renders (colours, pens, size) before decorating its output.
- [Kitty graphics subset](kitty-graphics.md): inline images.
- [Hyperlinks](hyperlinks.md): OSC 8 and plain URLs.

Source layout: `src/screen_model.c` is the terminal model (parser, grid,
scrollback, reflow); `src/aros_xterm.c` the Intuition window;
`src/aros_session.c` serves the AROS console packets to the Shell;
`src/theme.c` colours and pens; `src/kitty_graphics.c` images;
`tests/` host tests run by `make test`.
