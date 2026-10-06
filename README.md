# Limpet

I wanted a modern terminal on AROS, so I started building Limpet.

It runs the AROS Shell with Unicode, scrollback, copy and paste, clickable
links and inline images. Text and history reflow when you resize the window.

The goal is to replace `CON:`: keep compatibility with existing AROS
software and support modern command-line tools, full-screen applications
and remote work over SSH.

![Limpet showing an animated plasma drawn with the Kitty graphics protocol](doc/screenshots/kitty-plasma.png)

**Latest release: Limpet 0.1.0** for AROS ABIv11 x86_64 (AROS One 1.3,
2026.09 and later), and **Limpet SDK 0.1.0** for programs that want to use
it.

## What you can use today

Use Limpet for everyday Shell work: reading long command output, copying
commands and paths, following links, and running tools such as apkg with
colours and progress updates.

| apkg in Limpet, dark theme | Inline image beside coloured text |
| --- | --- |
| ![apkg show](doc/screenshots/apkg-show-dark.png) | ![Kitty image](doc/screenshots/kitty-image.png) |

Version 0.1 includes:

- Unicode: CJK, emoji (monochrome), combining clusters;
- 16-colour, 256-colour and 24-bit colour, themes (dark, light, high
  contrast); the Shell's own prompt keeps its Workbench pen colours;
- scrollback; selection, copy and paste through the system clipboard;
- text and scrollback rewrap when the window is resized;
- the alternate screen used by full-screen programs;
- Ctrl-C to break the running command;
- inline images through a documented subset of the Kitty graphics protocol;
- OSC 8 hyperlinks (Ctrl+click opens them through OpenURL when installed).

The AROS Shell itself is unchanged: Limpet is the window it runs in.
Limpet runs alongside the system console for now. Programs that explicitly
open `CON:` still use the existing console handler.

[Why Limpet](doc/why-limpet.md) compares the Shell in `CON:` and in Limpet,
with the same bytes sent to both.

## Install

With [apkg](https://github.com/tomaszstaniak/arospkg):

    apkg install limpet

Or by hand: download `Limpet-0.1.0-abiv11.zip` from the
[releases](https://github.com/tomaszstaniak/limpet/releases), then

    UnZip Limpet-0.1.0-abiv11.zip -d SYS:Utilities

and double-click the Limpet icon, or run `SYS:Utilities/Limpet/Limpet`.
The archive's `README.txt` lists keys, options and known limitations.

## For programmers

Ordinary programs need nothing: text written to `Output()` appears in
Limpet as in `CON:`, with UTF-8 decoded. The SDK is for programs that want
colour, progress lines, links or images in Limpet and plain text in `CON:`
and in files.

```c
LimpetPresentation p;
char red[LIMPET_SEQUENCE_MAX];

fflush(stdout);
if (limpet_query_presentation(Output(), &p, NULL) == LIMPET_OK &&
    limpet_sgr_ansi(red, sizeof red, &p, 1, 0))
    printf("%serror:%s file not found\n", red, LIMPET_SGR_FG_DEFAULT);
else
    printf("error: file not found\n");
```

The query is one DOS packet on the output handle. It does not read the
keyboard, and `CON:` refuses it at once, so the program does the right
thing in either console without configuration.

| `hello` in Limpet | the same examples in `CON:` |
| --- | --- |
| ![hello example in Limpet](doc/screenshots/sdk-hello.png) | ![examples in CON:](doc/screenshots/sdk-con-fallback.png) |

`limpet-sdk-0.1.0.zip` on the
[releases](https://github.com/tomaszstaniak/limpet/releases) page contains
the headers, a prebuilt library for ABIv11, the library sources and seven
examples.

- [Getting started](doc/sdk/getting-started.md): build and run the first
  example.
- [What to use, and when](doc/sdk/choosing.md): plain output, colour,
  links, images, raw keys.
- [Capability catalogue](doc/capabilities.md): every feature, how to detect
  it, its limits and how far it was tested.
- [API reference](doc/sdk/api.md), [protocol reference](doc/protocols.md),
  [examples](doc/sdk/examples.md).
- [Compatibility](doc/compatibility.md),
  [troubleshooting](doc/troubleshooting.md),
  [versions and stability](doc/versioning.md).

## Build

Limpet is built on a host with an AROS ABIv11 cross toolchain (GCC 10.5)
and the ABIv11 SDK:

    make abiv11-frontend LIMPET_VERSION=0.1.0 \
         ABIV11_GCC=/path/to/x86_64-aros-gcc \
         ABIV11_SDK=/path/to/SDK/Development

The result is `build/abiv11/aros-xterm` (installed as `Limpet`). Host tests
of the terminal model, the protocols and the SDK need only a C compiler:

    make test

Building the SDK archive: [doc/sdk/releasing.md](doc/sdk/releasing.md).
Source layout and protocol notes: [doc/](doc/).
Planned ports and features: [roadmap](doc/roadmap.md).

The `main` branch may contain work that is not in a release yet. This
README and `doc/` describe the latest release unless a page says otherwise.

## Contributing

Bug reports and patches: [CONTRIBUTING.md](CONTRIBUTING.md).

## Why the name?

A limpet is a mollusc. It has a shell and attaches itself to a surface.
That's where the name comes from.

## Licence

MIT, see [LICENSE.txt](LICENSE.txt); the SDK is under the same licence and
contains no third-party code. Bundled components of the terminal keep
their own licences: HarfBuzz (`vendor/harfbuzz/COPYING`), utf8proc
(`vendor/utf8proc/LICENSE.md`), Unicode data
(`data/unicode-17.0/LICENSE.txt`), Unifont (SIL OFL 1.1, `assets/fonts/`)
and Noto Emoji (SIL OFL 1.1, `assets/fonts/NotoEmoji-OFL.txt`).

Author: Tomasz Staniak, <https://tomaszstaniak.com>
