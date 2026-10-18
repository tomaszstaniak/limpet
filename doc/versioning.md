# Versions and stability

Limpet and the SDK have separate version numbers and separate tags.

| | Version | Archive | Git tag |
| --- | --- | --- | --- |
| Limpet (the terminal) | 0.5.0 | `Limpet-0.5.0-abiv11.zip` | `v0.5.0` |
| Limpet SDK | 0.4.0 | `limpet-sdk-0.4.0.zip` | `sdk-v0.4.0` |
| Control protocol | 1 | (`XtyControl.version`) | |

Limpet 0.2.1 (tag `v0.2.1`), 0.2.0 (tag `v0.2.0`) and 0.1.0 (tag `v0.1.0`)
are the earlier terminal releases, and SDK 0.2.0 (tag `sdk-v0.2.0`) the
earlier SDK release. 0.2.1 changes only the bundled HarfBuzz; the protocol
and the SDK are the same as for 0.2.0. 0.3.0 adds the window title feature
bit to the presentation reply, and SDK 0.3.0 decodes it. An SDK 0.1.0
was prepared but never published; its sources are kept in
`sdk/tests/compat-0.1.0` for the compatibility test.

## Compatibility

### Packet protocol

Fixed at version 1, and unchanged by Limpet 0.2.0 and 0.3.0:

- packet type numbers (`XTY_ACTION_*`) and `XTY_PACKET_MAGIC`;
- operation numbers, including 13 for the presentation query;
- the 64-byte block layout;
- the meaning of every defined bit and value;
- the rule that a clear known bit means unknown.

New features use bits and values that were zero before, where an older
client already ignores them. Limpet 0.2.0 added known bit 32 and
`values[7]` (input modes) this way, and 0.3.0 known bit 64 and `values[8]`
(features: the window title). An incompatible layout would get a new
`version`, and a terminal answers a version it does not know with an
error, never with a reinterpreted block.

What this means in practice, checked by tests and on AROS:

| Client | Terminal | Result |
| --- | --- | --- |
| SDK 0.1 sources | Limpet 0.2.0 | decodes the reply exactly as it decoded 0.1.0's (`sdk/tests/test_sdk_compat.sh`) |
| SDK 0.2.0 | Limpet 0.1.0 | everything as before; `input` stays unknown (`hello`, AROS ABIv11, 2026-10-07) |
| SDK 0.2.0 | Limpet 0.2.0 | all six fields |
| SDK 0.1 and 0.2.0 sources | Limpet 0.3.0 | decode the reply exactly as they decoded their own terminal's (`sdk/tests/test_sdk_compat.sh`) |
| SDK 0.3.0 | Limpet 0.2.1 | everything as with SDK 0.2.0; the title is not confirmed, so nothing is sent (hardcode built with SDK 0.3.0 in Limpet 0.2.1, AROS One 1.3, 2026-10-11) |
| SDK 0.3.0 | Limpet 0.3.0 | all seven fields (`hello`, AROS One 1.3, 2026-10-11) |
| any | `CON:` | `LIMPET_UNSUPPORTED`, plain text |

### Source compatibility

Code written for SDK 0.1 or 0.2 compiles unchanged with SDK 0.3.0. Within 0.x,
functions, constants and status values are only added; a change to an
existing one would be listed under "Changed" in `CHANGES.md` and raise the
minor version.

### Binary compatibility

Binary compatibility is not guaranteed in SDK 0.x. Structures such as `LimpetPresentation` may grow at the
end (0.2.0 added `input`, 0.3.0 `features`; 0.4.0 added only a bit), so every object that uses the SDK must be
compiled with the same SDK version as the library it links with. Rebuild
after updating the SDK.

When a public structure grows, the functions that fill it get new link
names in the header (in 0.2.0: `limpet_query_presentation_v2`,
`limpet_presentation_decode_v2`; in 0.3.0: `..._v3`). A forgotten rebuild then fails at link
time with an undefined symbol, instead of writing past the end of an old
caller's struct at run time.

## Terminal and SDK together

| SDK | Terminal | What a program can use |
| --- | --- | --- |
| 0.2.0 | Limpet 0.2.0, 0.2.1 | colours, styles, geometry and input modes |
| 0.2.0 | Limpet 0.1.0 | everything marked "Since 0.1.0"; the input modes are reported as unknown, so a program does not turn them on |
| 0.2.0 | Limpet 0.3.0 | everything as with 0.2.1; the window title is not decoded, so a program does not send one |
| 0.3.0 | Limpet 0.3.0–0.6.0 | SDK 0.3.0 features, including window titles; the Shift+Enter bit is not named, so a program does not turn it on |
| 0.4.0 | Limpet 0.6.0 | every SDK 0.4.0 feature, including Shift+Enter |
| 0.4.0 | Limpet 0.3.0–0.5.0 | as SDK 0.3.0; `XTY_INPUT_MODIFIED_ENTER` stays clear |
| 0.3.0 | Limpet 0.2.x | as SDK 0.2.0; `limpet_can_set_title()` returns 0 |
| any | a future Limpet | everything the SDK knows; newer fields are ignored until a newer SDK decodes them |

The query reports what the running terminal does. Applications must check the relevant capability before using a reported
feature. A successful query is not confirmation of every terminal sequence.

## Version 1.0 of the SDK

The SDK will be called 1.0 when its API has been used by programs other
than the examples, apkg and hardcode on more than one ABI. From then on,
semantic versioning applies, including binary compatibility within a
major version.

## Published versus development

The `main` branch can contain changes that are not in any release. Each
archive carries the documentation that matches it. The releases page lists
what is published.
