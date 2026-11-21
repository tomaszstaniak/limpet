#!/usr/bin/env python3
"""Writes src/completion_defs.c from the definition files in data/completions.

    python3 tools/completions/make-defs.py

One file per command, named as the command; the format is in
src/completion_args.h. Comment lines are left out of the program."""
import pathlib

root = pathlib.Path(__file__).resolve().parents[2]
names = sorted((p for p in (root / "data/completions").iterdir() if p.is_file() and not p.name.startswith(".") and p.suffix != ".md"),
               key=lambda p: p.name.lower())
out = ['/* Made by tools/completions/make-defs.py from data/completions. Do not edit. */',
       '#include "completion_defs.h"', '#include <stddef.h>', '',
       'static const struct { const char *name, *text; } definitions[] = {']
for path in names:
    lines = [l.strip() for l in path.read_text().splitlines()]
    lines = [l for l in lines if l and not l.startswith("#")]
    for l in lines:
        if '"' in l or "\\" in l:
            raise SystemExit("%s: a quote or backslash in %r" % (path.name, l))
    out.append('    { "%s", "%s" },' % (path.name, "\\n".join(lines) + "\\n"))
out += ['};', '',
        'static unsigned fold(unsigned c) { return c >= \'a\' && c <= \'z\' ? c - 32 : c; }', '',
        'const char *completion_definition(const char *name)', '{',
        '    size_t i, k;',
        '    for (i = 0; i < sizeof definitions / sizeof definitions[0]; ++i) {',
        '        const char *have = definitions[i].name;',
        '        for (k = 0; have[k] && fold((unsigned char)have[k]) == fold((unsigned char)name[k]); ++k) {}',
        '        if (!have[k] && !name[k]) return definitions[i].text;',
        '    }', '    return NULL;', '}', '',
        'const char *completion_definition_name(unsigned n)', '{',
        '    return n < sizeof definitions / sizeof definitions[0] ? definitions[n].name : NULL;', '}', '']
(root / "src/completion_defs.c").write_text("\n".join(out))
print("%d definitions" % len(names))
