# Versions and stability

Limpet and the SDK have separate version numbers.

| | Version | Format |
| --- | --- | --- |
| Limpet (the terminal) | 0.1.0 | `Limpet-<version>-<abi>.zip` |
| Limpet SDK | 0.1.0 | `limpet-sdk-<version>.zip` |
| Control protocol | 1 | `XtyControl.version` |

## What will not change

The control protocol is fixed at version 1:

- packet type numbers (`XTY_ACTION_*`) and `XTY_PACKET_MAGIC`;
- operation numbers, including 13 for the presentation query;
- the 64-byte block layout;
- the meaning of every defined bit and value;
- the rule that a clear known bit means unknown.

Since 0.1.0 the presentation reply also carries known bit 32 and
`values[7]` (input features). A 0.1.0 client ignores both, and
`sdk/tests/test_sdk_compat.sh` checks that it reads the new reply exactly
as it read the 0.1.0 one.

New features are added by using values and bits that are zero today,
where an older client already ignores them.
An incompatible layout would get a new `version`.

## What may change in SDK 0.x

- New functions and constants in `limpet.h`.
- New fields at the end of `LimpetPresentation`. Code that declares the
  struct and passes it to the SDK keeps compiling; code that relies on its
  exact size would need a rebuild.
- Behaviour fixes, recorded in `CHANGES.md`.

Within 0.x, existing function signatures and status values stay as they
are. A change to them would be listed under "Changed" in `CHANGES.md` and
raise the minor version.

## Version 1.0 of the SDK

The SDK will be called 1.0 when the API has been used by programs other
than the examples and apkg on more than one ABI. From then on, semantic
versioning applies: incompatible API changes only in a new major version.

## Terminal and SDK together

| SDK | Works with | Notes |
| --- | --- | --- |
| 0.1.0 | Limpet 0.1.0 and later | earlier terminals did not exist publicly |

A newer SDK may describe features an older Limpet does not have. That is
safe: the query reports what the running terminal does, and the SDK
returns "not confirmed" for the rest.

## Published versus development

The `main` branch can contain changes that are not in any release. The
README and this documentation describe the latest release unless a section
says otherwise. Release archives carry their own copy of the
documentation that matches them.
