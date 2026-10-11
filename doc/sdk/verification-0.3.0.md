# Verification record: SDK 0.3.0

What was checked for SDK 0.3.0, where, and what was not. The terminal side
(Limpet 0.3.0) is in [verification-0.3.0.md](../verification-0.3.0.md).

## Environment

| Item | Value |
| --- | --- |
| Date | 2026-10-11 |
| System | AROS One 1.3 (ABIv11, x86_64), QEMU (TCG), 1024x768 |
| Terminal under test | Limpet 0.3.0, SHA-256 `784a0c88726929d771395bb6926e493aa2b627859eff83ee19831512f3b0ed71` |
| Older terminal | Limpet 0.2.1, the published archive (SHA-256 `71dee9c638ba7676e8089f75d65511b71df9d066e0436feabd75ee27230c768d`) |
| Compiler, AROS SDK | `x86_64-aros-gcc` 10.5.0, ABIv11 2026.09 Developer SDK |
| Full-screen client | hardcode, development branch `limpet-title` (`aa259b6`, not in a hardcode release), built against this SDK |

## Example binaries

Built from the commit that adds this record with
`make ... ABI=abiv11 rebuild-lib examples`; the library sources are those
of commit `ba835d6`.

| Program | SHA-256 |
| --- | --- |
| `hello` | `f77cd2886cd012423c851fdc238b3a9ed5986dc1aa5fdf1268a9549cd8f8d5be` |
| `colors` | `1f06fff7f6994fd089850a41c018174e48b13518d964c933668aa2baedd70319` |
| `progress` | `758326a02b02c07c5fe3e600f1f5e7e2219a9ed62e6f92f4cc80ce6e96ec199a` |
| `geometry` | `a1ccdafab1c617584e41b891361499138bdc671a66fa75565f4d175754776bc8` |
| `hyperlink` | `ce19324058455995a4821f4eabbd463874d31031d2217a1584abc6b2becdfb09` |
| `image` | `fffef951abfa4fc687e813dfa80a81c1bc17f4d0f6c8108327581347a8b3d285` |
| `keys` | `81f738e8be465e901f7d05558df81a1b9b25da07af40960f8b65651410d65e92` |
| `title` | `c83f7877e340afeda31197ba2ed248129363557d1f6ab0a7dc631a8f025f2bcd` |

## Checks

The example binaries run on AROS were the ones listed above (same
SHA-256). The development builds of Limpet are earlier commits of the same
branch (`2b45230` for `hello`, `3f32949` for `image` and `hyperlink`); the
presentation reply and the title handling did not change after them.

| Check | Result |
| --- | --- |
| Host: `make test-sdk` | contract tests under ASan and UBSan pass; `limpet.h` compiles as C and C++; SDK 0.1.0 and 0.2.0 clients read the 0.3.0 reply as they read their own terminal's; objects built with their headers fail to link against the 0.3.0 library |
| `title` in Limpet 0.3.0 | the states shown in the window title, then `Shell` again |
| `hello` in a development build of Limpet 0.3.0 | reports `Window title: settable (OSC 0/2)` with the earlier fields |
| `image` in a development build of Limpet 0.3.0 | drawn in the reserved rows with `C=1`, deleted after a key |
| `hyperlink` in a development build of Limpet 0.3.0 | the target in the title while hovering |
| hardcode with this SDK in Limpet 0.2.1 | no title sent (the feature is not confirmed); hardcode works as before |
| hardcode with this SDK in `CON:` | no sequence printed |

## Not checked

`colors`, `progress`, `geometry` and `keys` were not run again (their
sources did not change). No example was run in `CON:` or with output
redirected to a file with this SDK. Not run on ABIv11 2026.09, mainline
AROS or real hardware.
