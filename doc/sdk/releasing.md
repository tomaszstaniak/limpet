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

The protocol header has one copy. Never fork it into the SDK: the terminal
and every client must compile against the same bytes.

## Checks before a release

1. `make test` passes. It includes `test-sdk`: the contract tests under
   AddressSanitizer and UndefinedBehaviorSanitizer, and the SDK's host
   compile check (C and C++).
2. The examples build for ABIv11 with `-Wall -Wextra -Werror` and have no
   undefined symbols (`x86_64-aros-nm -u`).
3. Each example has been run on AROS ABIv11 in Limpet and in `CON:`, and
   with output redirected to a file in both. Record the date, system, archive
   hashes and screenshots in a verification record like
   [verification-0.1.0.md](verification-0.1.0.md).
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
    unzip /path/to/limpet-sdk-0.1.0.zip
    cd limpet-sdk-0.1.0
    shasum -a 256 -c MANIFEST.sha256
    make CC=... AROS_SDK=... ABI=abiv11 examples
    make host-check

The example binaries must be byte-identical to the ones that were tested.

## Publishing on GitHub

1. Commit, and tag the commit `sdk-v<version>` (separate from the terminal's
   `v<version>` tags).
2. Push the branch and the tag.
3. Create a release from the tag with the archive and `SHA256SUMS` as
   assets. Paste the relevant part of `sdk/CHANGES.md` into the
   description.
4. Download the asset from GitHub and compare its SHA-256 with
   `SHA256SUMS`.

## Continuous integration

`.github/workflows/sdk.yml` runs the host tests and the host compile check
on every push. It has no AROS toolchain, so it **does not** prove that the
SDK builds or runs on AROS. Cross builds and runs on AROS are done locally
and recorded in a verification record.
