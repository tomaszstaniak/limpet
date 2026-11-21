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

Keywords and allowed words come first, before files and volumes. A keyword that is already on the line is not offered again, and neither
is the name of the argument you are about to type (`DIR` after `CD`). A word in
quotes, a word with a path, and the word after `<` or `>` are always
completed as files. TAB never runs anything.

Limpet comes with definitions for 43 AmigaDOS commands (`Copy`, `Delete`,
`List`, `Assign`, `Protect`, `Search`, ...) and for `apkg`. The keywords
were read from the commands themselves on mainline AROS and on AROS One
1.3. A few mainline commands have keywords that ABIv11 does not; those are
not offered on either system.

## Your own definitions

To teach TAB the arguments of another program or script, put a text file
named after the command into `ENVARC:Limpet/Completions`. For a script
called `Backup`:

    # ENVARC:Limpet/Completions/Backup
    template FROM/A,TO/A,MODE/K,VERIFY/S,QUIET/S
    dirs FROM TO
    values MODE full changed

| Line | Meaning |
| --- | --- |
| `template ...` | the command's AmigaDOS template, as `Backup ?` prints it |
| `dirs NAME ...` | these arguments take a drawer |
| `values NAME word ...` | this argument takes one of the words |
| `first word ...` | for a program without a template: its first word is one of these |
| `# ...` | a comment |

The file's name is compared without case. Your file wins over the
definition that comes with Limpet. For an alias only your own file is used,
because an alias need not take the arguments of the command it is named
after.

A file Limpet cannot read as a definition (an unknown line, a name that is
not in the template, more than 2048 bytes) is ignored as a whole, and TAB
then offers files as it does for an unknown command.

## Limits

- Limpet does not find templates on its own. It never runs a command with
  `?` and does not read program files for them.
- Descriptions of the keywords are not shown.
- Keywords are offered in the form the template has (`TO`), also when you
  typed lower case.
- A positional argument is recognised by counting the words before it. A
  command line that mixes keywords and plain words in unusual ways can
  make Limpet offer only drawers where a file is meant, or the other way
  round. What TAB leaves out can still be typed.
