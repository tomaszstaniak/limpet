# Verification record: Limpet 0.5.0 candidate and XTERM: stage 1

Not published. Two tracks with their own acceptance: Find in output and
the settings file (the 0.5.0 candidate), and the experimental `XTERM:`
console handler (stage 1, outside the package).

## Build

| Item | Value |
| --- | --- |
| Sources | `main` after merging `search-config` and `xterm-console` (see `git log`) |
| Compiler | `x86_64-aros-gcc` 10.5.0 |
| SDK | ABIv11 2026.09 Developer SDK |
| Built with | `make abiv11-frontend abiv11-handler abiv11-xterm-console` |
| AROS sources read for the console contract | ABIv11 `deadwood2/AROS` master `e33ca77d` (2026-09-15); mainline `aros-development-team/AROS` `74a2d80b` (2026-09-04) |

## Host tests

`make test` passes. New:

- `tests/test_output_search.c` (with AddressSanitizer and UBSan): matches
  across a soft wrap and not across a hard line end, wide and combined
  characters as whole cells, no normalisation, ASCII case, older and newer
  steps with their ends, the view held while output arrives (scrolled back
  and at the bottom), Esc back to the earlier view, lines falling out of
  the history, overwritten text, a width change and back (the same match
  counted from the newest), the alternate screen refused and closing the
  search, label cut at a character, a match starting above the view;
- `tests/test_settings.c`: defaults, every key, CR LF and spaces, inclusive
  ranges, each kind of wrong line rejecting the whole file with its
  number, size limit, controls;
- `tests/test_line_editor.c`: a command history of 3 and of 1 entries;
- `tests/test_console_name.c`: CON:'s fields, empty fields, bad geometry,
  titles, keyword prefixes in CON:'s order, SCREEN and WINDOW values.

## On AROS

Pool slot v11-2, AROS One 1.3 (ABIv11), QEMU TCG, 2026-10-15, five boots.
Everything ran from `RAM:`.

### Find and settings

| Check | Result |
| --- | --- |
| Settings file with font_size=16, light theme, scrollback=1000, history=3 | window in the light theme with the larger font; Up after four commands reaches back three |
| Find `needle` after two `List`s | `4 of 4` at the newest, the match in selection colours, other matches underlined |
| Up three times while a background job prints two lines a second | `1 of 4 (oldest)`; the view stayed on the match while 16 more lines arrived |
| Esc | back at the bottom, the job's last lines shown |
| Window made narrower with Find open at `2 of 4` | after the rewrap still `2 of 4`, the same line shown |
| Find on the alternate screen | refused, the title says why |
| `scrollback=50` on line 2 | title: `Settings: Limpet.conf line 2: scrollback takes 100 to 1000; built-in settings used` |
| Project > Reload settings after changing the theme | the theme changed at once |
| A font that is not there | Vera Mono, the title says so |
| `ioctl(TIOCGWINSZ)` from a POSIX program | 21 rows, 49 columns, as the window. In Limpet 0.4.1 the same program crashed in the C library's `ioctl` |

Not run: pasting into the search text; Ctrl-C while Find is open; a
font other than Vera Mono.

<a id="xterm-stage-1"></a>
### XTERM: stage 1

`con-contract` opened `CON:` and `XTERM:` itself with a full name
(`...:20/420/640/260/Keys/CLOSE`). Every check passed on both, except:

| Check | CON: | XTERM: |
| --- | --- | --- |
| Window-bounds report, raw mode | `CSI 1;1;15;75 r` | no answer |
| Cursor report, raw mode | `CSI 3;0 R` | no answer |
| Ctrl-D to the reader | signalled | not signalled |
| Seek error code | 209 | 219 (both -1 at once) |

The first XTERM: build failed the four key checks (`Read` -1 on the handle
the program opened) and gave no window or console unit in
`ACTION_DISK_INFO`; both were fixed and the checks then passed.

| Scenario | Result |
| --- | --- |
| `Install-XTERM`, `NewShell WINDOW "XTERM:300/200/700/420/XT Shell/CLOSE"` | window at that place and size, title `XT Shell` |
| In that Shell: Execute a script, `>` and `<` redirection, `Status`, `Wait 20` with Ctrl-C, `Ask`, `Copy ?` | all as in CON: (`wait rc=5`, `ask rc=5`, ReadArgs prompt answered) |
| `Run >*` job printing while the Shell runs commands | both outputs arrive |
| `Search`, `apkg` | ran |
| `WAIT` | the window stayed after the program ended, until closed |
| `NOSIZE/NODEPTH/NODRAG` | the window had none of them |
| `EndCLI`, close gadget | window gone; no process left in `Status` |
| `Remove-XTERM` with a window open, then without | refused, then removed; `NewShell WINDOW XTERM:` then reports the device missing |
| 10 Shell windows opened and closed from a script | free memory 2,080,753,088 → 2,080,666,224 after 5 → 2,080,665,680 after 10 bytes |

After an independent review, fixes followed that were built and covered by
host tests but not run again on AROS: Remove-XTERM no longer frees the
device node or its segment and scans the task lists under `Disable`; a
name smaller than the margins gives a 1x1 grid; the WAIT message is set
once; the console unit is written in place; RAmiga+C/V work while Find is
open; a resize under the alternate screen keeps the line count Find uses.

Not run: `SCREEN`, `NOBORDER`, `BACKDROP`, `INACTIVE`; two readers on
one console; the write queue with a full output queue (the three-writer
check passed before and after that change); ABIv11 2026.09; mainline.
