# Getting started with the Limpet SDK

This guide takes you from the archive to a program that prints coloured
text in Limpet and plain text everywhere else. It assumes you can already
build an AROS program in C.

## What you need

- An **AROS ABIv11 x86_64** cross toolchain (GCC) and the matching
  **ABIv11 Developer SDK**. The SDK was built and tested with GCC 10.5 and
  the 2026.09 Developer SDK.
- `make`, `unzip`.
- To run the result: AROS ABIv11 x86_64, 2026.09 or later (the examples
  were tested there; AROS One 1.3 was not tested with this SDK), with
  Limpet 0.2.0 for every feature, or Limpet 0.1.0 without the input modes.
  Without Limpet the examples still run and print plain text.

ABIv11 and mainline AROS binaries do not run on each other's systems. This
SDK release ships a library for ABIv11 only. See
[compatibility](../compatibility.md) for mainline.

## 1. Unpack

    unzip limpet-sdk-0.2.0.zip
    cd limpet-sdk-0.2.0

| Path | Contents |
| --- | --- |
| `include/limpet/limpet.h` | the SDK API |
| `include/limpet/tty_control.h` | the wire protocol, shared with the terminal |
| `lib/abiv11/libLimpet.a` | the library, prebuilt for ABIv11 x86_64 |
| `src/` | the library's source, two C files |
| `examples/` | seven small programs |
| `doc/` | this documentation |
| `MANIFEST.sha256` | checksums of every file in the archive |

Check the archive: `shasum -a 256 -c MANIFEST.sha256` (on Linux:
`sha256sum -c MANIFEST.sha256`).

## 2. Build the examples

Point `CC` at your compiler and `AROS_SDK` at the SDK's `Development`
directory:

    make CC=/opt/aros-v11/bin/x86_64-aros-gcc \
         AROS_SDK=/opt/aros-v11/SDK/Development \
         ABI=abiv11 examples

The programs land in `build/abiv11/`. If your compiler already knows its
sysroot, leave out `AROS_SDK`.

To compile the library from `src/` with your own toolchain instead of
using the prebuilt one, run the same command with `rebuild-lib`, then
`examples` again. `make clean` removes `build/` and keeps the library.

## 3. Run them on AROS

Copy `build/abiv11/` to the AROS machine, start Limpet, and run `hello`
from the Limpet window:

    Limpet SDK 0.3.0
    Output is a Limpet console.
      Colours:       24-bit colour (SGR 38;2;r;g;b)
      SGR 30-37:     Workbench screen pens; ANSI colours go as 38;5;0-15
      Bold, reset:   yes
      Rewrite line:  yes (CR and CSI K)
      Size:          117 columns x 37 rows, when asked
      Input modes:   bracketed paste (2004) resize reports (2048) wheel reports (2100)
      Window title:  settable (OSC 0/2)

In Limpet 0.2.x the last line reads
`Window title:  unknown (Limpet before 0.3.0 does not report it)`; in Limpet
0.1.0 the input modes are unknown as well.

Run it again in a normal AROS Shell window (`CON:`):

    Limpet SDK 0.3.0
    This console does not answer the Limpet query (CON: or another console).
    Nothing is known about it: plain text only.

And with output to a file: `hello >RAM:hello.txt`. The file says the
output is not a console and contains no escape sequences.

That is the whole pattern: one query, and output that reads correctly
whether or not it succeeds.

## 4. Your own program

```c
#include <limpet/limpet.h>
#include <proto/dos.h>
#include <stdio.h>

int main(void)
{
    LimpetPresentation p;
    char red[LIMPET_SEQUENCE_MAX];

    fflush(stdout);
    if (limpet_query_presentation(Output(), &p, NULL) == LIMPET_OK &&
        limpet_sgr_ansi(red, sizeof red, &p, 1, 0))
        printf("%serror:%s file not found\n", red, LIMPET_SGR_FG_DEFAULT);
    else
        printf("error: file not found\n");
    return RETURN_WARN;
}
```

Build it like the examples:

    x86_64-aros-gcc -O2 --sysroot=$AROS_SDK -I$AROS_SDK/include \
        -Ilimpet-sdk-0.2.0/include myprog.c -o myprog \
        -L$AROS_SDK/lib -Llimpet-sdk-0.2.0/lib/abiv11 -lLimpet

Or skip the library and compile the two files in `src/` into your program;
they need nothing but `dos.library` and the C library.

Three habits keep a program correct everywhere:

1. **Ask about `Output()`**, the handle you write to. Not `Input()`.
2. **Flush stdio before asking** when the answer decides how to print what
   follows.
3. **Say it in words too.** Colour is a second channel, never the only one.

## Next

- [Which interface to use](choosing.md) for your kind of program.
- [Capability catalogue](../capabilities.md): every feature, how to detect
  it, and how far it was checked.
- [API reference](api.md).
- [Examples](examples.md), with expected output and limits.
