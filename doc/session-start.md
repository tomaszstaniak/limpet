# A command for every new Shell window

Limpet can run one command each time it opens a new Shell window: Fastfetch,
to show the system at a glance, or any command of your own. Nothing runs
until you choose it.

## Choosing it

1. In a Limpet window, choose **Project > Preferences...**. Limpet
   Preferences opens (it is the program `LimpetPrefs` in Limpet's drawer,
   and can also be started from there).
2. Under **When a new Shell window opens**, set **Run** to:
   - **Nothing**: the Shell starts as before.
   - **Fastfetch**: see below.
   - **My own command**: type a Shell command line, or press
     **Choose program...** to pick a program in a file requester.
3. **Command line** shows exactly what will run.
4. Press **Run now** to try it in the window you opened Preferences from.
5. Press **Use** to keep the choice until the system restarts, **Save** to
   keep it for good, or **Cancel** to leave everything as it was.

A window that is already open is not changed: the command runs in the next
window you open.

### Fastfetch

Fastfetch is a separate program. The looks below are presets of its AROS
package (`aros-compact` and `aros-logo`).

| Setting | Choices |
| --- | --- |
| **Fastfetch** | the program; empty means `fastfetch` from the Shell's search path. The button opens a file requester. |
| **Look** | **Compact** (small logo, the details in one list), **AROS logo picture** (the logo as a picture, the details in groups), **My configuration file** (a Fastfetch configuration of your own, chosen with the requester next to **File**) |
| **Show** | System, Hardware, Memory and disks, Network: clear a box to leave that group out |
| **Logo** | **As in the look**, **Text** (the small text logo, also in the picture look), **None** |

The looks are Fastfetch's own presets (`aros-compact`, `aros-logo`); Limpet
Preferences does not copy them, so a newer Fastfetch brings its newer
presets. Your own configuration file is only read, never changed. The
**Show** boxes work with files that name their groups the way the presets
do (`"group": "network"` in a module).

The picture needs a terminal that shows pictures: Limpet does, from 0.8.0.
In `CON:` and in output written to a file, Fastfetch uses the text logo.

## When it runs

- Once, in each new Shell window that Limpet opens, after
  `S:Shell-Startup` and before the first prompt.
- Not in a window started with `--from FILE` or `--no-startup` (programs
  use these to start their own sessions), not in a `--command` window, and
  not in Shells started inside the window (`NewShell`, `Execute`).
- The Shell runs it as the last line of its startup script; nothing is
  typed into the window for it.
- Ctrl-C stops it, as any command. If the program is missing or fails, the
  Shell says so and carries on as usual.
- If a command in `S:Shell-Startup` fails (return code at or above the
  Shell's `FailAt`), the Shell stops reading the script there and the line
  does not run either.
- It runs as a script line: a program that asks whether it runs at an
  interactive prompt is told it does not.
- After F4 (new session in the same window) it runs again.

## Run now

**Run now** runs the line in the Limpet window you opened Preferences from,
as if you had typed it at the prompt. It runs only when that window's Shell
waits at its prompt with nothing typed. Otherwise Preferences says why it
did not run:

- "a program is running in the Limpet window" (a command, or a full-screen
  program such as an editor),
- "something is being typed in the Limpet window",
- "the Limpet window has no Shell", or "The Limpet window is closed".

Preferences started from the Shell or Workbench, not from a window's menu,
has no window to run the line in; **Run now** is then unavailable.

## Where it is kept

The choices are kept in `Session.conf`, beside `Limpet.conf`: **Use**
writes `ENV:Limpet/Session.conf`, **Save** writes
`ENVARC:Limpet/Session.conf` as well. `Limpet.conf` (font, theme,
scrollback, history) is not touched, so a Limpet older than 0.8.0 keeps
reading it as before. Lines of your own in `Session.conf` (comments, for
example) stay where they are; a key at its default value is not written.

| Key | Value |
| --- | --- |
| `session_start` | `off` (default), `fastfetch` or `command` |
| `session_command` | the command line, up to 399 bytes |
| `fastfetch` | path of the Fastfetch program; default `fastfetch` |
| `fastfetch_look` | `compact` (default), `picture` or `file` |
| `fastfetch_config` | path of your configuration file, for `file`; it must end in `.jsonc`, `.json` or `.json5` |
| `fastfetch_hide` | groups left out, separated by commas: `system`, `hardware`, `memory-disks`, `network` |
| `fastfetch_logo` | `look` (default), `text` or `none` |

A `Session.conf` with a mistake in it runs nothing; the window's title bar
names the line, as for `Limpet.conf`.
