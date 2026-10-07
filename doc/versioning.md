# Versions and stability

Limpet and the SDK have separate version numbers and separate tags.

| | Version | Archive | Git tag |
| --- | --- | --- | --- |
| Limpet (the terminal) | 0.2.0 | `Limpet-0.2.0-abiv11.zip` | `v0.2.0` |
| Limpet SDK | 0.2.0 | `limpet-sdk-0.2.0.zip` | `sdk-v0.2.0` |
| Control protocol | 1 | (`XtyControl.version`) | |

Limpet 0.1.0 (tag `v0.1.0`) is the earlier terminal release. An SDK 0.1.0
was prepared but never published; its sources are kept in
`sdk/tests/compat-0.1.0` for the compatibility test.

## Three kinds of compatibility

### Protocol: what goes over the DOS packet

Fixed at version 1, and unchanged by Limpet 0.2.0:

- packet type numbers (`XTY_ACTION_*`) and `XTY_PACKET_MAGIC`;
- operation numbers, including 13 for the presentation query;
- the 64-byte block layout;
- the meaning of every defined bit and value;
- the rule that a clear known bit means unknown.

New features use bits and values that were zero before, where an older
client already ignores them. Limpet 0.2.0 added known bit 32 and
`values[7]` (input modes) this way. An incompatible layout would get a new
`version`, and a terminal answers a version it does not know with an
error, never with a reinterpreted block.

What this means in practice, checked by tests and on AROS:

| Client | Terminal | Result |
| --- | --- | --- |
| SDK 0.1 sources | Limpet 0.2.0 | decodes the reply exactly as it decoded 0.1.0's (`sdk/tests/test_sdk_compat.sh`) |
| SDK 0.2.0 | Limpet 0.1.0 | everything as before; `input` stays unknown (`hello`, AROS ABIv11, 2026-10-07) |
| SDK 0.2.0 | Limpet 0.2.0 | all six fields |
| any | `CON:` | `LIMPET_UNSUPPORTED`, plain text |

### Source: whether your code compiles

Code written for SDK 0.1 compiles unchanged with SDK 0.2.0. Within 0.x,
functions, constants and status values are only added; a change to an
existing one would be listed under "Changed" in `CHANGES.md` and raise the
minor version.

### Binary: whether old objects link with a new library

Not promised in 0.x. Structures such as `LimpetPresentation` may grow at the
end (0.2.0 added `input`), so every object that uses the SDK must be
compiled with the same SDK version as the library it links with. Rebuild
after updating the SDK.

When a public structure grows, the functions that fill it get new link
names in the header (in 0.2.0: `limpet_query_presentation_v2`,
`limpet_presentation_decode_v2`). A forgotten rebuild then fails at link
time with an undefined symbol, instead of writing past the end of an old
caller's struct at run time.

## Terminal and SDK together

| SDK | Terminal | What a program can use |
| --- | --- | --- |
| 0.2.0 | Limpet 0.2.0 | everything in the [catalogue](capabilities.md) |
| 0.2.0 | Limpet 0.1.0 | everything marked "Since 0.1.0"; the input modes are reported as unknown, so a program does not turn them on |
| 0.2.0 | a future Limpet | everything above; newer fields are ignored until a newer SDK decodes them |

The query reports what the running terminal does. A program that follows
it never sends a feature the terminal did not confirm.

## Version 1.0 of the SDK

The SDK will be called 1.0 when its API has been used by programs other
than the examples, apkg and hardcode on more than one ABI. From then on,
semantic versioning applies, including binary compatibility within a
major version.

## Published versus development

The `main` branch can contain changes that are not in any release. Each
archive carries the documentation that matches it. The releases page lists
what is published.
