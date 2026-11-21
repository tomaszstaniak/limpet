# Argument definitions that come with Limpet

One file per command, named as the command. The format is described in
`src/completion_args.h` and, for users, in `doc/completion.md`.

The `template` lines are the commands' own AmigaDOS templates, read from the
commands (`Copy ?`) on mainline AROS x86_64 (nightly 2026-11-15) and on AROS
One 1.3 (ABIv11). Where mainline has more keywords than ABIv11 (`Assign`,
`List`, `Mount`, `Protect`), the file holds the ones both have, so that
nothing is offered that a system refuses. The `dirs` lines are ours.

After changing a file: `python3 tools/completions/make-defs.py`, then
`make test`.
