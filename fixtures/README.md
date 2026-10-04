# Replay fixture contract (format 2)

The manifest records **input bytes**, their SHA-256, terminal geometry, and
independently authored **expected snapshots**. `make test` validates the data,
generates C vectors, then executes them through the production screen model.

The C replay runner is compiled unchanged for the host and AROS mainline x86_64.
Python is needed only to generate vectors on the host. The guest binary embeds
the data and expectations; it does not need Python or a JSON parser.

## Execution

```sh
make test
make AROS_GCC=x86_64-aros-gcc build/test_replay_aros
```

Run the native binary from Shell with output redirected to a distinct file:

```text
RAM:replay-test >RAM:replay-results
Type RAM:replay-results
```

Every ready fixture is run once for every two-chunk split, including empty first
and last chunks, and once byte by byte. After feeding input, the runner compares
every visible cell's character, foreground, background, and bold flag, plus
width (1 normal / 2 lead / 0 continuation), cluster length and suffix scalars,
geometry, cursor, current style,
pending wrap, history count, and history offset.
It then performs each declared resize or scroll action and checks that snapshot.
A mismatch exits nonzero and identifies the fixture, split, step, and difference.

The final FNV-1a snapshot hash encodes each compared value as four bytes in
little-endian order. This avoids differences from structure padding, native
endianness, or pointer size. It is a regression fingerprint, not a security hash.
Expected cells are compared directly; passing does not depend on hash equality.

## Manifest

Each output fixture has:

- `id`: unique lowercase ASCII name using letters, digits, and hyphens.
- `status`: `ready` or `deferred`.
- `kind`: `output` for parser bytes; `input` for future keyboard events.
- `reason`: mandatory for deferred fixtures.
- `size`: initial `[rows, columns]`.
- `history_lines`: optional initial history capacity, default zero.
- Exactly one of `input_hex` or `input` (a relative binary file).
- `sha256`: digest of decoded input bytes, not the hexadecimal text.
- `expected`: relative JSON snapshot path.
- `actions`: optional list of `resize: [rows, columns]` or `scroll: delta`,
  each with an `expected` snapshot path. Positive scroll moves into history.

All paths remain inside the fixture directory. Limits: 64 fixtures, 8192 input
bytes each, 80 rows, 160 columns, 256 initial history lines, and 32 actions.

## Snapshot

```json
{
  "lines": ["red     ", "        "],
  "cursor": [0, 3],
  "cells": [
    {"row": 0, "column": 0, "fg": 1},
    {"row": 0, "column": 1, "fg": 1},
    {"row": 0, "column": 2, "fg": 1}
  ]
}
```

Coordinates are **zero-based**. Lines must include trailing spaces to exactly
match the declared geometry. Cells default to foreground 7, background 0, and
bold 0. Sparse `cells` overrides replace attributes at the given coordinate.
`style` specifies the current output attributes and has the same defaults.
Optional `widths` is one string per row, exactly one `0`, `1` or `2` per cell;
it defaults to all `1`. A `2` must be followed by `0` on the same row, and a
`0` must follow `2`, contain a literal space, and have matching attributes.
The authored lines include this continuation placeholder, e.g. `"A中 B"` with
`"1201"`. The generator does not infer widths from the production classifier.
Wrong but structurally valid widths must fail in the C runner.

Each lead cell may have a `cluster` string in its sparse override. It must
contain 1..16 scalars and its first scalar must match the character in `lines`.
For example, `{"row":0,"column":0,"cluster":"é"}` stores U+0065 followed
by U+0301 while `lines` contains only `e` at that column. Non-space characters
default to length 1. Spaces default to erased blanks (length 0); explicitly
printed spaces use `"cluster":" "` (length 1). Continuations have length 0
and cannot carry clusters. Unused suffix slots are zero and checked by the
runner; the fingerprint hashes length and every occupied suffix scalar.

Colours are indices 0–7; bold is integer 0 or 1. Optional `history_count`,
`view_offset`, and `wrap_pending` default to zero.

The lines describe the current **view**; cursor coordinates remain those of the
live screen even when viewing history. Each character in a JSON line represents
one cell (a scalar or an explicitly marked continuation placeholder); surrogate
pairs in JSON escapes decode to one scalar, while lone surrogates are rejected.
Clusters preserve all their scalars in the sparse overrides, including joiners
and variation selectors. Expected widths remain independently authored.
Input fixtures contain an `events` list and remain deferred until an input
adapter exists. Unknown fields, malformed geometry, and duplicate overrides fail
validation.

## Current corpus

Twenty-three ready cases cover plain text, split ESC/SGR, erase, style reset, history
ring eviction/navigation, grow/shrink, OSC discard, wrapping, large CSI parameters,
German and Polish text, Greek/Cyrillic/CJK/Hangul/Gothic scalars, emoji, combining
and emoji grapheme clusters, Unicode boundaries, and invalid UTF-8 recovery.
Seven dedicated wide cases exercise early wrap, pending wrap across SGR,
overlapping pairs, erase of a continuation, crop/grow, history navigation and
one-column fallback. Amiga keyboard CSI remains explicitly deferred.

The emoji cases include 😀🚀🦀, variation selectors, a skin-tone modifier,
woman-technologist ZWJ, regional indicators for Poland, and a keycap sequence.
They assert preservation of **all scalar values inside their clusters**, including
U+200D and U+FE0F, across every byte split. The cluster case occupies 11 columns:
one for the decomposed accent and two each for heart, modifier emoji, ZWJ emoji,
flag and keycap. It replaces the former 20-column scalar-only expectation while
preserving the original input bytes and SHA-256. Parser expectations do not
prove composed glyph appearance, font coverage or colour rendering.
Boundary cases include U+07FF/U+0800, the surrogate gap, U+FFFF, U+10000 and
U+10FFFF. Noncharacters are valid scalar values and are retained. Invalid cases
cover overlong forms, encoded surrogates, out-of-range scalars and recovery into
ASCII/SGR. Native visual evidence is tracked separately from parser snapshots.

LF preserves the cursor column. Alternate-screen behaviour is covered by
separate screen-model tests rather than this replay corpus.

The Python integration test deliberately supplies a wrong glyph, cursor, colour,
width or cluster suffix and confirms that the compiled C runner rejects each. This prevents a
validator-only success from being mistaken for a parser test.
