#ifndef XTERM_COMPLETION_LEARN_H
#define XTERM_COMPLETION_LEARN_H
#include <stddef.h>
/* Makes a completion definition (completion_args.h) from what a program
 * prints about itself. Nothing here runs a program: the caller did, on the
 * user's request, and hands over the text.
 *
 * Both return the length of the definition written to out (not
 * terminated), or 0 when the text holds nothing to learn from. */

/* The text of "program --help": every line that starts with an option
 * becomes an option line. Understood: "-s, --long", "-s,--long",
 * "-s --long", "--long=VALUE", "--long <value>", "--long [<value>]",
 * "--long VALUE", "[possible values: a, b]" in the lines of the option,
 * "<a|b|c>" and "{a,b,c}" as the value. A value named like a file or a
 * directory becomes <file> or <dir>, a bool the words true and false.
 * Colour sequences in the text are skipped. */
size_t completion_learn_help(const char *help, size_t length, char *out, size_t size);

/* The text of "Command ?": the AmigaDOS template on its first line, with
 * the ": " the Shell's prompt for arguments ends in. */
size_t completion_learn_template(const char *text, size_t length, char *out, size_t size);
#endif
