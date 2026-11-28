#include "../src/completion_args.h"
#include "../src/completion_defs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static XLineEditor e;
static XCompletion c;
static XArgSpec spec;

static void define(const char *text) { assert(completion_args_parse(&spec, text, strlen(text))); }
static int refused(const char *text) { XArgSpec s; return !completion_args_parse(&s, text, strlen(text)); }

/* TAB on the line: what the definition adds, and which files go with it. */
static int tab(const char *line)
{
    memset(&e, 0, sizeof e);
    assert(line_editor_insert(&e, line, strlen(line)));
    assert(completion_prepare(&c, &e));
    return completion_args_offer(&c, &spec);
}
static int offered(const char *name)
{
    size_t i;
    for (i = 0; i < c.count; ++i)
        if (!strcmp(c.names[i], name)) return 1;
    return 0;
}

int main(void)
{
    char name[32];
    size_t from;
    unsigned i;

    /* Definitions. */
    define("template FROM/M/A,TO/A,PAT=PATTERN/K,BUF=BUFFER/K/N,ALL/S,QUIET/S\ndirs TO\n");
    assert(spec.count == 6);
    assert(!strcmp(spec.args[2].name, "PAT") && !strcmp(spec.args[2].alias, "PATTERN"));
    assert(spec.args[0].flags == XARG_MULTI && spec.args[2].flags == XARG_KEYWORD && spec.args[4].flags == XARG_SWITCH);
    assert(spec.args[1].kind == XARG_DIR);
    define("# a comment\r\n\r\ntemplate NAME,TARGET/M\r\n");
    assert(spec.count == 2);
    define("first install remove show\n");
    assert(spec.first_count == 3);
    assert(refused(""));
    assert(refused("template A/X\n"));                 /* an unknown modifier */
    assert(refused("template A,B\ndirs C\n"));         /* a name that is not there */
    assert(refused("template A/S\ndirs A\n"));         /* a switch takes no value */
    assert(refused("template A\ntemplate B\n"));
    assert(refused("template A\nrun something\n"));    /* nothing is ever run */
    assert(refused("template A\nvalues A\n"));
    {
        char big[XARGS_DEFINITION_MAX + 2];
        memset(big, '#', sizeof big);
        assert(!completion_args_parse(&spec, big, sizeof big));
    }

    /* Which command a word belongs to. */
    assert(completion_args_command("Copy a ", 7, name, sizeof name, &from) && !strcmp(name, "Copy") && from == 4);
    assert(completion_args_command("C:Copy a ", 9, name, sizeof name, &from) && !strcmp(name, "Copy"));
    assert(completion_args_command("List | Sort x ", 14, name, sizeof name, &from) && !strcmp(name, "Sort"));
    assert(!completion_args_command("Copy", 0, name, sizeof name, &from));          /* the command itself */
    assert(!completion_args_command("Copy a >", 8, name, sizeof name, &from));      /* a redirection */
    assert(!completion_args_command("Copy a > ", 9, name, sizeof name, &from));
    assert(!completion_args_command("Echo \"a ", 8, name, sizeof name, &from));     /* inside open quotes */
    assert(completion_args_command("Echo \"a|b\" ", 11, name, sizeof name, &from) && !strcmp(name, "Echo"));

    /* A copy command: its destination keyword, then a drawer for it. */
    define("template FROM/M/A,TO/A,PAT=PATTERN/K,BUF=BUFFER/K/N,ALL/S,QUIET/S\ndirs TO\n");
    assert(tab("Copy a t") == 0 && offered("TO") && c.count == 1);
    assert(tab("copy a T") == 0 && offered("TO"));
    assert(tab("Copy a TO ") == 1 && !c.count);         /* drawers only, no names */
    assert(tab("Copy a TO=") == 1);
    assert(tab("Copy a TO Work:pro") == 1);
    assert(tab("Copy a to wo") == 1 && !c.count);
    /* An empty word stays a list of files. */
    assert(tab("Copy ") == 0 && !c.count);
    /* A name that is used is not offered again; a switch does not eat a word. */
    assert(tab("Copy a ALL a") == 0 && !offered("ALL"));
    assert(tab("Copy a QUIET q") == 0 && !offered("QUIET"));
    assert(tab("Copy a PAT x p") == 0 && !offered("PAT"));
    assert(tab("Copy a PATTERN x p") == 0 && !offered("PAT"));
    /* The value of a keyword without a kind: files. */
    assert(tab("Copy a PAT ") == 0 && !c.count);
    /* A path is never mixed with names. */
    assert(tab("Copy RAM:t") == 0 && !c.count);
    /* The word after a redirection is a file. */
    assert(tab("Copy a >t") == 0 && !c.count);
    /* Quoted words are files. */
    assert(tab("Copy a \"t") == 0 && !c.count);
    /* The command word itself is left to the command search. */
    assert(tab("Cop") == 0 && !c.count);

    /* A drawer-only positional argument. */
    define("template DIR\ndirs DIR\n");
    assert(tab("CD ") == 1);
    assert(tab("CD d") == 1 && !c.count);               /* not its own name, "DIR" */
    assert(tab("CD Work:") == 1);
    assert(tab("CD a ") == 0);                          /* past the template: no claim */

    /* Positional arguments in order; /M takes the rest. */
    define("template NAME,TARGET/M,LIST/S\ndirs TARGET\n");
    assert(tab("Assign ") == 0);
    assert(tab("Assign X: ") == 1);
    assert(tab("Assign X: a ") == 1);
    assert(tab("Assign X: a l") == 1 && offered("LIST"));

    /* Allowed words. */
    define("template FILE/A,MODE/K,LEVEL\nvalues MODE fast slow safe\nvalues LEVEL low high\n");
    assert(tab("Tool f MODE ") == 2 && c.count == 3 && offered("fast"));
    assert(tab("Tool f MODE s") == 2 && c.count == 2 && offered("slow") && offered("safe"));
    assert(tab("Tool f MODE=s") == 2 && c.count == 2);
    assert(tab("Tool f ") == 0 && offered("low") && offered("high"));
    assert(tab("Tool f l") == 0 && offered("low") && !offered("LEVEL"));
    assert(tab("Tool f m") == 0 && offered("MODE"));

    /* Subcommands. */
    define("first install remove show\n");
    assert(tab("apkg ") == 2 && c.count == 3);
    assert(tab("apkg re") == 2 && c.count == 1 && offered("remove"));
    assert(completion_apply(&c, &e, 0) && !strcmp(e.text, "apkg remove "));
    assert(tab("apkg remove ") == 0 && !c.count);

    /* Keywords come before files and volumes, whatever their names. */
    define("template FROM/M/A,TO/A,ALL/S\n");
    assert(tab("Copy a t") == 0 && c.words == 1);
    assert(completion_add(&c, "T", 2) && completion_add(&c, "todo.txt", 0) && completion_add(&c, "tools", 1));
    assert(!completion_add(&c, "to", 0));              /* a file named as the keyword: one entry */
    assert(c.count == 4 && !strcmp(c.names[0], "TO") && !strcmp(c.names[1], "T:"));
    assert(completion_apply(&c, &e, 0) && !strcmp(e.text, "Copy a TO "));
    assert(completion_apply(&c, &e, 0) && !strcmp(e.text, "Copy a T:"));

    /* Options of a Unix-style program. */
    define("option -l --long\noption -L --level <value>\noption --color --colour always auto never\n"
           "option -1\noption --config <file>\noption --out <dir>\n");
    assert(spec.options == 6);
    assert(tab("eza --l") == 2 && c.count == 2 && offered("--long") && offered("--level"));
    assert(tab("eza -") == 2 && offered("--long") && offered("-1") && !offered("-l") && offered("--colour"));
    assert(tab("eza --color ") == 2 && c.count == 3 && offered("auto"));
    assert(tab("eza --color=a") == 2 && c.count == 2 && offered("always"));
    assert(completion_apply(&c, &e, 0) && !strcmp(e.text, "eza --color=always "));
    assert(completion_current(&c, &e));                 /* two matches: TAB goes on to the next */
    assert(tab("eza --lo") == 2 && c.count == 1 && completion_apply(&c, &e, 0) && !strcmp(e.text, "eza --long "));
    assert(!completion_current(&c, &e));                /* one match: TAB starts at the next word */
    assert(tab("eza --colour n") == 2 && c.count == 1 && offered("never"));
    assert(tab("eza -L ") == 2 && !c.count);            /* a value nobody can guess */
    assert(tab("eza -l ") == 0 && !c.count);            /* -l is not -L: files */
    assert(tab("eza --config ") == 0 && !c.count);
    assert(tab("eza --out ") == 1 && tab("eza --out Work:") == 1);
    assert(tab("eza --long ") == 0 && tab("eza --color auto ") == 0);
    assert(tab("eza --unknown ") == 0);
    /* A value that was left out: the next option is an option. */
    assert(tab("eza --color --l") == 2 && offered("--long"));
    assert(tab("eza --color --long ") == 0);
    define("option --sort ext Ext name\n");
    assert(tab("eza --sort e") == 2 && c.count == 2 && offered("ext") && offered("Ext"));
    define("option -l --long\noption -L --level <value>\noption --color --colour always auto never\n"
           "option -1\noption --config <file>\noption --out <dir>\n");
    assert(tab("eza \"-") == 0 && !c.count);
    assert(tab("eza ") == 0 && !c.count);
    assert(refused("option long\n"));                  /* a name starts with - */
    assert(refused("option -\n"));
    assert(refused("option --a <thing>\n"));
    assert(refused("option --a <file> x\n"));
    assert(refused("option --a x --b\n"));
    /* Options beside subcommands: they do not count as the first word. */
    define("first install remove\noption --root <dir>\noption --plain\n");
    assert(tab("apkg --plain ") == 2 && offered("install"));
    assert(tab("apkg --root RAM:x in") == 2 && offered("install") && c.count == 1);
    assert(tab("apkg --root ") == 1);
    assert(tab("apkg --p") == 2 && offered("--plain") && c.count == 1);
    assert(tab("apkg install ") == 0);
    /* Options of one subcommand. */
    define("first build test run\noption --verbose\nin build run\noption --release\noption --target arm x86\nin test\noption --nocapture\n");
    assert(spec.options == 4);
    assert(tab("cargo --") == 2 && c.count == 1 && offered("--verbose"));
    assert(tab("cargo build --") == 2 && c.count == 3 && offered("--release") && offered("--verbose") && !offered("--nocapture"));
    assert(tab("cargo --verbose test --") == 2 && c.count == 2 && offered("--nocapture"));
    assert(tab("cargo run --target ") == 2 && c.count == 2 && offered("arm"));
    assert(tab("cargo test --target ") == 0);           /* not an option of test: a plain word follows */
    assert(tab("cargo ") == 2 && c.count == 3);
    assert(refused("in\noption --a\n"));
    assert(refused("option --a\nin build\nfirst build\n"));   /* a part holds options only */
    assert(refused("in bu/ild\noption --a\n"));

    /* A long definition is one: the limit is 16 KB. */
    {
        static char big[XARGS_DEFINITION_MAX];
        size_t n = 0;
        while (n + 32 < sizeof big) n += (size_t)sprintf(big + n, "option --name-%04u a b\n", (unsigned)n);
        assert(completion_args_parse(&spec, big, n) && spec.options > 500);
    }

    /* The program as typed, and a package started through apkg. */
    {
        char path[64], id[32], root[64];
        size_t at;
        assert(completion_args_program("Work:Tools/eza -l ", 18, path, sizeof path) && !strcmp(path, "Work:Tools/eza"));
        assert(completion_args_program("List | eza ", 11, path, sizeof path) && !strcmp(path, "eza"));
        assert(!completion_args_program("eza", 0, path, sizeof path));
        assert(completion_args_command("apkg run eza --l", 13, name, sizeof name, &from));
        assert(completion_args_run("apkg run eza --l", from, 13, name, id, sizeof id, &at, root, sizeof root) && !strcmp(id, "eza") && at == 12);
        assert(!completion_args_run("apkg run ez", from, 9, name, id, sizeof id, &at, root, sizeof root));     /* the id is being typed */
        assert(!completion_args_run("apkg run --check eza ", from, 21, name, id, sizeof id, &at, root, sizeof root));
        assert(!completion_args_run("apkg show eza ", from, 14, name, id, sizeof id, &at, root, sizeof root));
        assert(!completion_args_run("Copy run eza ", 4, 13, "Copy", id, sizeof id, &at, root, sizeof root));
        assert(!root[0]);
        assert(completion_args_command("apkg --root RAM:pk run eza --l", 27, name, sizeof name, &from));
        assert(completion_args_run("apkg --root RAM:pk run eza --l", from, 27, name, id, sizeof id, &at, root, sizeof root));
        assert(!strcmp(id, "eza") && !strcmp(root, "RAM:pk") && at == 26);
        assert(completion_args_run("apkg --root=Work:P run eza ", 4, 27, "apkg", id, sizeof id, &at, root, sizeof root) && !strcmp(root, "Work:P"));
        assert(!completion_args_run("apkg --plain run eza ", 4, 21, "apkg", id, sizeof id, &at, root, sizeof root));
        assert(completion_args_run("apkg run eza --l", 4, 13, "apkg", id, sizeof id, &at, root, sizeof root) && at == 12);
        define("option -l --long\n");
        memset(&e, 0, sizeof e);
        assert(line_editor_insert(&e, "apkg run eza --l", 16) && completion_prepare(&c, &e));
        assert(completion_args_offer_from(&c, &spec, at) == 2 && offered("--long"));
    }

    /* Every definition that comes with Limpet is one. */
    for (i = 0; completion_definition_name(i); ++i) {
        const char *text = completion_definition(completion_definition_name(i));
        if (!text || !completion_args_parse(&spec, text, strlen(text))) {
            printf("bad definition: %s\n", completion_definition_name(i));
            return 1;
        }
    }
    assert(i > 0);
    assert(completion_definition("APKG") && completion_definition("apkg") && !completion_definition("apk"));
    puts("completion_args: ok");
    return 0;
}
