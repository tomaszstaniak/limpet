#ifndef XTERM_COMPLETION_DEFS_H
#define XTERM_COMPLETION_DEFS_H
/* The argument definitions that come with Limpet (completion_args.h), made
 * from the files in data/completions by tools/completions/make-defs.py.
 * The text for a command's name, compared without case, or NULL. */
const char *completion_definition(const char *name);
/* The n-th command that has one, NULL past the last. */
const char *completion_definition_name(unsigned n);
#endif
