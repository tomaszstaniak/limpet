# Verification record: SDK 0.5.0

What was checked for SDK 0.5.0, where, and what was not. The terminal side
(Limpet 0.7.0) is in [verification-0.7.0.md](../verification-0.7.0.md).

SDK 0.5.0 adds constants only: the style bits `XTY_STYLE_DIM_RESET` to
`XTY_STYLE_STRIKE_RESET` and the feature bits `XTY_FEATURE_OSC8`,
`XTY_FEATURE_NERD_ICONS_EZA` and `XTY_FEATURE_ICON_WIDTH_ONE`. The library
sources, `LimpetPresentation` and the link names are those of SDK 0.4.0.

## Environment

| Item | Value |
| --- | --- |
| Date | 2026-10-24 |
| System | AROS One 1.3 (ABIv11, x86_64), QEMU (TCG), 1024x768 |
| Terminal under test | Limpet 0.7.0, from its release archive |
| Compiler, AROS SDK | `x86_64-aros-gcc` 10.5.0, ABIv11 2026.09 Developer SDK |

## Example binaries

Built with `make -C sdk ... ABI=abiv11 examples`.

| Program | SHA-256 |
| --- | --- |
| `hello` | `b12fb11987fdcfd10d55e198fa4ac1edeccf7c8ca5bb1b94b2ec9ed3fff28654` |
| `colors` | `1f06fff7f6994fd089850a41c018174e48b13518d964c933668aa2baedd70319` |
| `progress` | `758326a02b02c07c5fe3e600f1f5e7e2219a9ed62e6f92f4cc80ce6e96ec199a` |
| `geometry` | `a1ccdafab1c617584e41b891361499138bdc671a66fa75565f4d175754776bc8` |
| `hyperlink` | `ce19324058455995a4821f4eabbd463874d31031d2217a1584abc6b2becdfb09` |
| `image` | `fffef951abfa4fc687e813dfa80a81c1bc17f4d0f6c8108327581347a8b3d285` |
| `keys` | `81f738e8be465e901f7d05558df81a1b9b25da07af40960f8b65651410d65e92` |
| `title` | `c83f7877e340afeda31197ba2ed248129363557d1f6ab0a7dc631a8f025f2bcd` |

Every example except `hello` is byte-identical to the SDK 0.4.0 build
recorded in [verification-0.4.0.md](verification-0.4.0.md); `hello` differs
by the version it prints.

## Checks

| Check | Result |
| --- | --- |
| Host: `make test-sdk` | contract tests under ASan and UBSan pass; `limpet.h` compiles as C and C++; SDK 0.1.0 to 0.4.0 clients read the current reply as they read their own terminal's; objects built with the 0.1 and 0.2 headers fail to link against the 0.5.0 library, objects built with the 0.3.0 and 0.4.0 headers link and decode the reply |
| `hello` in Limpet 0.7.0 | "Limpet SDK 0.5.0", "Output is a Limpet console", the same fields as with 0.4.0 |
| `hello` in `CON:` | "This console does not answer the Limpet query"; plain text |
| `hello` with output to a file | "Output is not a console"; rc 0 |
| eza 0.23.5 (reads the new bits) in Limpet 0.7.0 | icons and links offered with the bundled font; no icons with Vera Mono (bits clear) |

## Not checked

The unchanged examples were not run again. Not run on ABIv11 2026.09,
mainline AROS or real hardware.
