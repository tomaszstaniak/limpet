# Verification record: Limpet 0.15.0

Checks of 2026-11-28. All targets were built from the same source with
version 0.15.0 and date 28.11.2026, with the toolchains, SDKs and
aros-crypto kit 1 of 0.14.0. `make test` passes, with the new
`tests/test_completion_learn.c` (help texts and templates) and new cases
for subcommand parts and `apkg --root` in `tests/test_completion_args.c`.
All runtime checks used QEMU.

## What changed

- `LimpetLearn`, a new Shell command in the archive.
- `in` lines in definitions: options of one subcommand.
- `apkg --root DRAWER run <package>` uses the definition under that drawer.
- The SSH programs are rebuilt with the new version number; their source
  is unchanged.

## LimpetLearn

| Step | AROS One 1.3 (ABIv11) | mainline x86_64, nightly 2026-11-24 | Raspberry Pi, nightly 2026-11-24 (QEMU raspi3b) |
| --- | --- | --- | --- |
| `LimpetLearn eza`: runs `eza --help`, 65 options | pass | pass (`SHOW`) | pass |
| `LimpetLearn fastfetch`: 105 options, file written | pass | not run | pass |
| `LimpetLearn Copy`, `List`, `Delete`, `Wait`: runs `Command ?`, template written or shown | pass | pass (`Copy`, `List`) | pass (`Copy`) |
| A second run refuses to write over the file; `REPLACE` writes | pass | not run | not run |
| A name that is no command; `eza TEMPLATE`; `Copy HELP`: nothing learned, nothing written, no files left in `T:` or `RAM:` | pass | not run | not run |
| TAB in Limpet completes from the learned definition | pass | not run | pass |

ABIv11 and mainline ran builds of the same source made before packaging;
the Pi ran the released archive. On mainline, `LimpetLearn List` shows the
keyword `OWNER`, which ABIv11 does not have: a learned template is the one
of the system it was learned on.

## Subcommand parts and apkg --root

On AROS One 1.3 and on the Pi, with a definition holding `in build run`
and `in test` parts: `tool build --` offers the options of `build` and the
common one, `tool test --` those of `test`, `tool run --target ` the
target's words. On AROS One 1.3, `apkg --root RAM:LP run eza --so`
completed `--sort` and its values from `RAM:LP/eza/Completions/eza`.

## Not checked

- LimpetLearn with programs other than eza and Fastfetch and the system's
  own commands.
- The Preferences program and the SSH programs, which did not change.
- A real Raspberry Pi.
