#ifndef XTERM_COMPLETION_ARGS_H
#define XTERM_COMPLETION_ARGS_H
#include "completion.h"
#include <stddef.h>
/* What TAB offers after a command's name: the command's keywords, only
 * drawers where it takes a drawer, only the allowed words where it takes
 * one of a few. The knowledge is a small text, one per command:
 *
 *     template FROM/M,TO/A,ALL/S,QUIET/S     the command's AmigaDOS template
 *     dirs TO                                these arguments take a drawer
 *     values MODE fast slow                  this argument takes one of these
 *     first install remove show              the first word is one of these
 *
 * Lines starting with # and blank lines are skipped. "first" is for
 * programs without a template whose first word is a subcommand. Nothing is
 * ever run to find this out, and a line this model does not understand is
 * left alone: TAB then offers files, as it does without a definition. */
#define XARGS_MAX 40
#define XARGS_NAME 24
#define XARGS_WORDS 48
#define XARGS_DEFINITION_MAX 2048

enum { XARG_SWITCH = 1, XARG_KEYWORD = 2, XARG_MULTI = 4, XARG_REST = 8, XARG_TOGGLE = 16 };
enum { XARG_ANY, XARG_DIR, XARG_VALUES };

typedef struct {
    char name[XARGS_NAME], alias[XARGS_NAME];
    unsigned char flags, kind;
    unsigned char values_at, values_count;
} XArg;

typedef struct {
    XArg args[XARGS_MAX];
    unsigned count;
    char words[XARGS_WORDS][XARGS_NAME];
    unsigned word_count;
    unsigned char first_at, first_count;
} XArgSpec;

/* Reads a definition. Returns 1, or 0 for a text that is not a definition
 * (too long, a line that is none of the four, a name that is not in the
 * template, too many entries); *spec is then empty. */
int completion_args_parse(XArgSpec *spec, const char *text, size_t length);

/* The command a word belongs to: line is the text before the word (its
 * first `start` bytes are looked at). The name is the command's file name
 * without its path, as typed; *from is where its arguments begin. Returns 0
 * when the word is the command itself, follows a redirection sign, or the
 * name does not fit. */
int completion_args_command(const char *line, size_t start, char *name, size_t size, size_t *from);

/* What the word at `start` may be, given the arguments typed before it
 * (line[from..start)). */
typedef struct {
    int kind;            /* XARG_ANY, XARG_DIR or XARG_VALUES */
    int names;           /* keyword names may be offered */
    const XArg *value_of; /* the argument whose value this is, or NULL */
    int first;           /* the first word of a program with "first" words */
    unsigned used[XARGS_MAX / 32 + 1];
} XArgContext;
void completion_args_context(const XArgSpec *spec, const char *line, size_t from, size_t start, XArgContext *out);

/* Adds what the definition offers for the transaction's word to its
 * candidates and says which files belong beside them: 0 files and drawers,
 * 1 drawers only, 2 none. Keyword names are offered once a letter is typed,
 * so that TAB on an empty word stays a list of files, and never the name
 * of the argument the word itself would fill. */
int completion_args_offer(XCompletion *c, const XArgSpec *spec);
#endif
