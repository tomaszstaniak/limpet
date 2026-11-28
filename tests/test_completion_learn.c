#include "../src/completion_learn.h"
#include "../src/completion_args.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char out[XARGS_DEFINITION_MAX + 1];
static const char *learn(const char *help)
{
    size_t n = completion_learn_help(help, strlen(help), out, XARGS_DEFINITION_MAX);
    XArgSpec spec;
    out[n] = 0;
    if (n) assert(completion_args_parse(&spec, out, n));    /* what is learned is a definition */
    return out;
}
static int has(const char *line) { return strstr(out, line) != NULL; }

int main(void)
{
    size_t n;
    /* clap, as eza prints it. */
    learn("Usage: eza [OPTIONS] [FILES]...\n\n"
          "Arguments:\n  [FILES]...  \n\n"
          "META OPTIONS:\n"
          "      --stdin    read file names from stdin\n"
          "  -?, --help     Print help\n"
          "  -v, --version  Print help\n\n"
          "LAYOUT OPTIONS:\n"
          "  -1, --oneline          display one entry per line\n"
          "  -L, --level <DEPTH>    limit the depth of recursion\n"
          "      --color [<WHEN>]   When to use colours. [default: auto] [possible values: always,\n"
          "                         auto, never]\n"
          "  -s, --sort <SORT_FIELD>  which field to sort by\n"
          "          [possible values: name, Name, size]\n"
          "  -I, --ignore-glob <GLOBS>  glob patterns (pipe-separated) of files to ignore\n");
    assert(has("option --stdin\n") && has("option --help\n") && has("option --version -v\n"));
    assert(has("option --oneline -1\n") && has("option --level -L <value>\n"));
    assert(has("option --color always auto never\n"));
    assert(has("option --sort -s name Name size\n"));
    assert(has("option --ignore-glob -I <value>\n"));
    assert(!has("FILES") && !has("Usage"));

    /* GNU style and a few others. */
    learn("Usage: tool [OPTION]... FILE\n"
          "  -a, --all                  do not ignore entries\n"
          "      --block-size=SIZE      scale sizes by SIZE\n"
          "  -o FILE, --output=FILE     write to FILE\n"
          "      --color[=WHEN]         colorize the output\n"
          "  -C DIR                     change to directory DIR\n"
          "  -m <fast|slow|safe>        the mode\n"
          "  --format {json,text}       output format\n"
          "  -h,--help <?command>       Show help\n"
          "  --thread <?bool>           Use threads\n"
          "  -q --quiet                 say nothing\n"
          "  -n NUM                     a number\n"
          "  -x                         a switch\n"
          "  --colo[u]r=WHEN            an odd spelling\n"
          "  -- ends the options\n"
          "Report bugs to <bugs@example.org>.\n");
    assert(has("option --all -a\n") && has("option --block-size <value>\n"));
    assert(has("option --color <value>\n") && has("option -C <dir>\n"));
    assert(has("option -m fast slow safe\n") && has("option --format json text\n"));
    assert(has("option --help -h <value>\n") && has("option --thread true false\n"));
    assert(has("option --quiet -q\n") && has("option -n <value>\n") && has("option -x\n"));
    assert(has("option --output -o <file>\n"));
    assert(!has("colo[u]r") && !has("bugs"));

    /* Colour sequences are skipped. */
    learn("  \033[1m-c\033[m, \033[1m--config\033[m \033[3m<config>\033[m   the file\n");
    assert(has("option --config -c <value>\n"));

    /* Nothing to learn. */
    assert(!*learn("") && !*learn("usage: tool FILE\nno options here\n"));
    /* A full buffer ends at a whole line. */
    n = completion_learn_help("  --alpha\n  --beta\n", 18, out, 16);
    assert(n == 15 && !memcmp(out, "option --alpha\n", 15));

    /* AmigaDOS templates. */
    n = completion_learn_template("FROM/M/A,TO/A,ALL/S,QUIET/S: ", 29, out, sizeof out);
    out[n] = 0;
    assert(!strcmp(out, "template FROM/M/A,TO/A,ALL/S,QUIET/S\n"));
    {
        static const char said[] = "DIR/M,P=PAT/K,KEYS/S,ALL/S: List: object not found\n";
        n = completion_learn_template(said, sizeof said - 1, out, sizeof out);
        out[n] = 0;
        assert(!strcmp(out, "template DIR/M,P=PAT/K,KEYS/S,ALL/S\n"));
    }
    n = completion_learn_template("NAME,TARGET/M\n", 14, out, sizeof out);
    out[n] = 0;
    assert(!strcmp(out, "template NAME,TARGET/M\n"));
    assert(!completion_learn_template("Usage: tool [options] file\n", 27, out, sizeof out));
    n = completion_learn_template("DIR: ", 5, out, sizeof out);
    assert(n == 13 && !memcmp(out, "template DIR\n", 13));
    assert(!completion_learn_template("", 0, out, sizeof out));
    assert(!completion_learn_template("tool: unknown option ?\n", 23, out, sizeof out));
    puts("completion_learn: ok");
    return 0;
}
