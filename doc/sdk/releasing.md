# Building and releasing the SDK

## Layout in the repository

| Path | Role |
| --- | --- |
| `sdk/include/limpet/tty_control.h` | protocol header; the terminal includes it through `src/tty_control.h` |
| `sdk/include/limpet/limpet.h` | SDK API |
| `sdk/src/limpet_query.c` | the DOS side: `limpet_control`, `limpet_query_presentation`, `limpet_write_dos` |
| `sdk/src/limpet_format.c` | pure code: decoding, decisions, sequence builders |
| `sdk/examples/` | example programs |
| `sdk/tests/test_sdk.c` | host contract tests, run against the terminal's reply code |
| `sdk/tests/host/` | stand-ins for `<dos/dos.h>` and `<proto/dos.h>` with x86_64 AROS type widths |
| `sdk/Makefile` | library and examples for one ABI, plus a host compile check |
| `tools/make-sdk.sh` | builds the release archive |

The terminal and SDK share one protocol header. Keep a single source of
truth for packet layouts and constants.

## Checks before a release

1. `make test` passes. It includes `test-sdk`: the contract tests under
   AddressSanitizer and UndefinedBehaviorSanitizer, and the SDK's host
   compile check (C and C++).
2. The examples build for ABIv11 with `-Wall -Wextra -Werror` and have no
   undefined symbols (`x86_64-aros-nm -u`).
3. Each example has been run on AROS ABIv11 in Limpet and in `CON:`, and
   with output redirected to a file in both. Record the date, system, archive
   hashes and screenshots in a verification record like
   [verification-0.2.0.md](verification-0.2.0.md).
4. `LIMPET_SDK_VERSION_*` in `limpet.h` and `sdk/CHANGES.md` name the new
   version.

## Building the archive

    ABIV11_GCC=/path/to/x86_64-aros-gcc \
    ABIV11_SDK=/path/to/SDK/Development \
    tools/make-sdk.sh

The script:

- reads the version from `limpet.h`;
- runs `make test-sdk` and stops on failure;
- copies headers, sources, examples, host stand-ins, the programmer
  documentation and the licence into
  `dist/sdk/<version>/stage/limpet-sdk-<version>/`;
- rewrites documentation links for the archive layout; a link to a file
  that is not in the archive becomes a GitHub link at the tag
  `sdk-v<version>`, so the tag must be pushed with the release;
- builds `lib/abiv11/libLimpet.a` inside that staged tree, with
  deterministic `ar` flags;
- refuses to continue if any staged file contains a host path or a file
  type that is not on its list;
- writes `MANIFEST.sha256` inside the archive and `SHA256SUMS` beside it,
  with fixed timestamps, so the same tree gives the same archive.

## Checking the archive

Unpack it outside the repository and build the examples from it, exactly
as [getting started](getting-started.md) says:

    mkdir /tmp/sdk-check && cd /tmp/sdk-check
    unzip /path/to/limpet-sdk-0.3.0.zip
    cd limpet-sdk-0.3.0
    shasum -a 256 -c MANIFEST.sha256
    make CC=... AROS_SDK=... ABI=abiv11 examples
    make host-check

The example binaries must be byte-identical to the ones that were tested.

## The terminal archive

The terminal and SDK have independent versions and tags (`v<version>` and
`sdk-v<version>`). Release a new SDK only when its public interface changes. Build the terminal
from the tagged commit with a fixed date, so the build can be repeated:

    make abiv11-frontend LIMPET_VERSION=0.5.0 LIMPET_DATE=15.10.2026 \
         ABIV11_GCC=... ABIV11_SDK=...

The archive `Limpet-<version>-abiv11.zip` holds the `Limpet` drawer:
the binary, `README.txt`, fonts, themes, licences and the icons. Its
`README.txt` names the version and lists what is new.

## Publishing on GitHub

1. Merge into `main`. Tag each component being released: `v<version>` for
   the terminal or `sdk-v<version>` for the SDK.
2. Push `main` and the release tags.
3. Create one release per tag, each with its archive and `SHA256SUMS`,
   and a user-facing description of the changes. Keep test logs, binary
   comparisons and acceptance results in verification records, not release notes.
4. Wait for the CI run of the pushed commit to pass.
5. Download every asset from GitHub and compare its SHA-256 with
   `SHA256SUMS`.

## Continuous integration

`.github/workflows/sdk.yml` runs the host tests and the host compile check
on every push. It has no AROS toolchain, so it **does not** prove that the
SDK builds or runs on AROS. Cross builds and runs on AROS are done locally
and recorded in a verification record.
