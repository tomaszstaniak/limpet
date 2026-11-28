#!/usr/bin/env python3
"""Turns a fish completion file into a Limpet definition.

    python3 tools/completions/from-fish.py eza.fish >Completions/eza

Reads the plain `complete -c <command> ...` lines: -s and -l (the option's
spellings), -a (the words its value may be), -r and -x (it takes a value),
-F and -f (the value is a file, or is not), and -n with
__fish_use_subcommand (the subcommands) or __fish_seen_subcommand_from
(the options of those subcommands). Lines that call fish functions
or other programs cannot be turned into a definition: their options are
written with <value>, and the script says on stderr what it left out.
The format is described in doc/prompt-and-completion.md."""
import re
import shlex
import sys

NAME = re.compile(r"^[A-Za-z0-9_.-]+$")


def statements(text):
    """`complete` statements; a quoted string may run over several lines."""
    out, current = [], ""
    for line in text.splitlines():
        if not current and not line.lstrip().startswith("complete "):
            continue
        current += line + "\n"
        if current.rstrip().endswith("\\"):
            current = current.rstrip()[:-1] + " "
            continue
        try:
            words = shlex.split(current, posix=True)
        except ValueError:      # an open quote: the statement goes on
            continue
        out.append(words)
        current = ""
    return out


def values(text, dropped):
    # Descriptions go first: they may hold anything, brackets too.
    text = re.sub(r"\\t(?:'(?:[^'\\]|\\.)*'|\S*)", " ", text)
    if "(" in text:
        return None
    words = text.split()
    good = [w for w in words if NAME.match(w) and not w.startswith("-")]
    dropped += [w for w in words if w not in good]
    return good


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    first, dropped, skipped, unknown = [], [], 0, 0
    parts = {(): []}        # the subcommands a part belongs to -> its option lines
    for words in statements(open(sys.argv[1], encoding="utf-8").read()):
        names, value, takes, files, condition = [], None, False, True, ""
        i = 1
        while i < len(words):
            w = words[i]
            arg = words[i + 1] if i + 1 < len(words) else ""
            if w in ("-n", "--condition"):
                condition = arg; i += 2
            elif w in ("-c", "--command", "-d", "--description", "-w", "--wraps"):
                i += 2
            elif w in ("-s", "--short-option", "-o", "--old-option"):
                names.append("-" + arg); i += 2
            elif w in ("-l", "--long-option"):
                names.append("--" + arg); i += 2
            elif w in ("-a", "--arguments"):
                value = values(arg, dropped); takes = True; i += 2
            elif w in ("-x", "--exclusive"):
                takes = True; files = False; i += 1
            elif w in ("-r", "--require-parameter"):
                takes = True; i += 1
            elif w in ("-f", "--no-files"):
                files = False; i += 1
            else:
                i += 1
        # The subcommands the line is for: "__fish_seen_subcommand_from a b".
        part = ()
        seen = re.fullmatch(r"\s*__fish_seen_subcommand_from\s+(.+?)\s*", condition)
        if seen:
            part = tuple(w for w in seen.group(1).split() if NAME.match(w))
        elif condition and "__fish_use_subcommand" not in condition and "not __fish_seen_subcommand_from" not in condition:
            unknown += 1
        good = [n for n in names if NAME.match(n)]
        dropped += [n for n in names if n not in good]
        if not good:
            # A line without an option, for the first word: the subcommands.
            if value and not part and condition:
                first += [w for w in value if w not in first]
            else:
                skipped += 1
            continue
        # Long spellings first: they are the ones TAB offers.
        good.sort(key=lambda n: not n.startswith("--"))
        line = "option " + " ".join(good)
        if value:
            line += " " + " ".join(value)
        elif takes:
            line += " <file>" if files and value is None and "-a" not in words else " <value>"
        parts.setdefault(part, []).append(line)
    out = (["first " + " ".join(first)] if first else []) + parts.pop(())
    for part, lines in parts.items():
        out += ["in " + " ".join(part)] + lines
    print("\n".join(out))
    if dropped:
        print("left out: " + " ".join(dropped), file=sys.stderr)
    if skipped:
        print("%d complete lines without an option" % skipped, file=sys.stderr)
    if unknown:
        print("%d lines with a condition this script does not know: their options are offered everywhere" % unknown, file=sys.stderr)


main()
