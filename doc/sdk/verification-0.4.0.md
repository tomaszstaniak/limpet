# Verification record: SDK 0.4.0

What was checked for SDK 0.4.0, where, and what was not. The terminal side
(Limpet 0.6.0) is in [verification-0.6.0.md](../verification-0.6.0.md).

SDK 0.4.0 adds one constant, `XTY_INPUT_MODIFIED_ENTER`, and the line for it
in `hello`. The library sources, `LimpetPresentation` and the link names are
those of SDK 0.3.0.

## Environment

| Item | Value |
| --- | --- |
| Date | 2026-10-18 |
| System | AROS One 1.3 (ABIv11, x86_64), QEMU (TCG), 1024x768 |
| Terminal under test | Limpet 0.6.0, from its release archive |
| Compiler, AROS SDK | `x86_64-aros-gcc` 10.5.0, ABIv11 2026.09 Developer SDK |

## Example binaries

Built with `make ... ABI=abiv11 rebuild-lib examples`.

| Program | SHA-256 |
| --- | --- |
| `hello` | `f6a2506c1286ea59cb1d8d244868b1ce66f5474a0bfc1e0f3e6e36da79efd2cb` |
| `colors` | `1f06fff7f6994fd089850a41c018174e48b13518d964c933668aa2baedd70319` |
| `progress` | `758326a02b02c07c5fe3e600f1f5e7e2219a9ed62e6f92f4cc80ce6e96ec199a` |
| `geometry` | `a1ccdafab1c617584e41b891361499138bdc671a66fa75565f4d175754776bc8` |
| `hyperlink` | `ce19324058455995a4821f4eabbd463874d31031d2217a1584abc6b2becdfb09` |
| `image` | `fffef951abfa4fc687e813dfa80a81c1bc17f4d0f6c8108327581347a8b3d285` |
| `keys` | `81f738e8be465e901f7d05558df81a1b9b25da07af40960f8b65651410d65e92` |
| `title` | `c83f7877e340afeda31197ba2ed248129363557d1f6ab0a7dc631a8f025f2bcd` |

Every example except `hello` is byte-identical to the SDK 0.3.0 build
recorded in [verification-0.3.0.md](verification-0.3.0.md): the library and
those sources did not change.

## Checks

| Check | Result |
| --- | --- |
| Host: `make test-sdk` | contract tests under ASan and UBSan pass; `limpet.h` compiles as C and C++; SDK 0.1.0, 0.2.0 and 0.3.0 clients read the current reply as they read their own terminal's, counting only the input bits each knew; objects built with the 0.1 and 0.2 headers fail to link against the 0.4.0 library, an object built with the 0.3.0 header links and decodes the reply |
| `hello` in Limpet 0.6.0 | input modes: bracketed paste, resize reports, wheel reports and Shift+Enter (`CSI > 4 ; 1 m`); the other fields as with 0.3.0 |
| `hello` in `CON:` | "This console does not answer the Limpet query"; plain text |
| `hello` with output to a file | "Output is not a console"; no escape sequences |
| A raw-mode client using the bit (Bishop) in Limpet 0.6.0 | Shift+Enter offered and working; see the terminal's record |

## Not checked

`hello` in Limpet 0.5.0 or earlier with this SDK (the bit is clear there,
as the host tests show for an old reply). The unchanged examples were not
run again. Not run on ABIv11 2026.09, mainline AROS or real hardware.
