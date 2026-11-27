# Prompt and TAB completion

## Prompt

Limpet can set the prompt of the Shell windows it opens. Choose it in
**Project > Preferences**, on the **Shell** page, under **Prompt**:

| Choice | Looks like | The Shell command behind it |
| --- | --- | --- |
| As the system sets it | `1.Work:project> ` on a stock system | none |
| Current drawer | `Work:project> ` | `Prompt "%S> "` |
| Drawer and last return code | `Work:project [0]> ` | `Prompt "%S [%R]> "` |
| Drawer and return code, in colours | the same, the drawer bold in the theme's bright blue, the code in grey | `Prompt` with colour sequences |

The number in brackets is the return code of the last command: 0 after
success, 5 after a warning, 10 or 20 after a failure.

The choice applies to windows opened afterwards. It works like the command
set on the same page: Limpet adds one `Prompt` line to the end of the Shell's
startup script, after `S:Shell-Startup`. A `Prompt` command of your own,
typed later or run from a script, replaces it as usual. Windows started
with `--from` or `--no-startup`, and Shells started inside a window
(`NewShell`, `Execute`), keep the system's prompt.

The setting is `prompt=system`, `path`, `status` or `colour` in
`ENVARC:Limpet/Shell.conf`.

## What TAB completes

TAB completes the word at the cursor; TAB again goes to the next match,
Shift+TAB to the one before. The title bar shows how many there are.

- The first word of the line: commands (the Shell's path, `C:`, resident
  commands and aliases).
- Any other word: files and drawers, and volumes and assigns when the word
  has no path yet.
- After a command Limpet knows, also the command's keywords, once you have
  typed a letter: `Copy notes.txt t` and TAB gives `TO`.
- Where the command takes a drawer, only drawers are offered: after `CD`,
  `MakeDir`, `Assign NAME`, `Path`.
- Where it takes one of a few words, those words: `apkg ` and TAB goes
  through `install`, `remove`, `search` and the other apkg commands.
- After a program with Unix-style options, the options, once you have
  typed `-`: `eza --so` and TAB gives `--sort`. After an option that takes
  one of a few words, those words: `eza --sort ` and TAB goes through
  `name`, `size`, `time` and the others; `--sort=` works the same.

Keywords, options and allowed words come first, before files and volumes.
A keyword that is already on the line is not offered again, and neither is
the name of the argument you are about to type (`DIR` after `CD`). A word
in quotes, a word with a path, and the word after `<` or `>` are always
completed as files. TAB never runs anything.

## Where the definitions come from

What TAB knows about a command is a small text file, a definition. Limpet
looks for the definition of the command on the line in this order and uses
the first it finds:

1. `ENVARC:Limpet/Completions/<command>`: your own.
2. `Completions/<command>` in the drawer of the program: the one a program
   brings along. Limpet looks where the Shell would load the program from
   (the current drawer, the Shell's path, `C:`), or in the drawer you typed
   (`Work:Tools/eza`).
3. `SYS:Packages/<command>/Completions/<command>`: a package installed by
   apkg in its usual place.
4. The definitions that come with Limpet: 43 AmigaDOS commands (`Copy`,
   `Delete`, `List`, `Assign`, `Protect`, `Search`, ...) and `apkg`.

After `apkg run <package>`, TAB completes the arguments of the package's
program: `apkg run eza --so` and TAB gives `--sort`. Places 1 and 3 are
used, with the package's name.

The packages of eza (0.23.5 revision 3) and Fastfetch (2.70.0 revision 6)
bring their definitions. `apkg install eza` is all it takes.

The keywords of the AmigaDOS commands were read from the commands
themselves on mainline AROS and on AROS One 1.3. A few mainline commands
have keywords that ABIv11 does not; those are not offered on either
system.

## Your own definitions

To teach TAB the arguments of another program or script, put a text file
named after the command into `ENVARC:Limpet/Completions`. For a script
called `Backup`:

    # ENVARC:Limpet/Completions/Backup
    template FROM/A,TO/A,MODE/K,VERIFY/S,QUIET/S
    dirs FROM TO
    values MODE full changed

For a program with Unix-style options:

    # ENVARC:Limpet/Completions/mytool
    option -v --verbose
    option -o --output <file>
    option --cache <dir>
    option --level <value>
    option --colour --color always auto never

| Line | Meaning |
| --- | --- |
| `template ...` | the command's AmigaDOS template, as `Backup ?` prints it |
| `dirs NAME ...` | these arguments take a drawer |
| `values NAME word ...` | this argument takes one of the words |
| `first word ...` | for a program without a template: its first word is one of these |
| `option NAME ...` | an option without a value; all its spellings on one line |
| `option NAME ... <value>` | the option takes a value that TAB cannot guess |
| `option NAME ... <file>`, `<dir>` | the value is a file, or a drawer |
| `option NAME ... word ...` | the value is one of the words |
| `# ...` | a comment |

Option names are compared with case (`-l` is not `-L`). TAB offers an
option's long spellings; the short one only when the option has no long
one.

The file's name is compared without case. Your file wins over every other
definition. For an alias only your own file and the package's are used,
because an alias need not take the arguments of the command it is named
after.

A file Limpet cannot read as a definition (an unknown line, a name that is
not in the template, more than 16384 bytes) is ignored as a whole, and
Limpet looks in the next place.

## Definitions in a program's archive

If you publish a program for AROS, put its definition into a drawer
`Completions` beside the program, in a file named as the program:

    MyTool/
        mytool
        Completions/mytool

That is all: Limpet finds it when the drawer is on the Shell's path, when
the program is started with its path, and when apkg installed the package
(`SYS:Packages/mytool`). The file is plain text and no other program needs
it, so it does no harm on a system without Limpet.

For a program that already has a fish completion file, Limpet's source
has a converter:

    python3 tools/completions/from-fish.py mytool.fish >Completions/mytool

It takes the plain `complete -c` lines: the options and the words their
values may be. What a fish file computes by running the program cannot be
turned into a definition; such values become `<value>`.

## Limits

- Limpet does not find templates or options on its own. It never runs a
  command with `?` or `--help` and does not read program files for them.
- Descriptions of the keywords and options are not shown.
- Keywords are offered in the form the template has (`TO`), also when you
  typed lower case.
- A positional argument is recognised by counting the words before it. A
  command line that mixes keywords and plain words in unusual ways can
  make Limpet offer only drawers where a file is meant, or the other way
  round. What TAB leaves out can still be typed.
- Options that belong to one subcommand only (`tool build --release`)
  cannot be described: a definition's options are offered after every
  subcommand.
- Several short options in one word (`-la`) are not looked into; an option
  among them that takes a value is not seen.
- A package installed with `apkg --root` elsewhere than `SYS:Packages` is
  found through the Shell's path only.
