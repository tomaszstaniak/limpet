/* LimpetLearn: writes a TAB completion definition for a command from what
 * the command prints about itself.
 *
 *     LimpetLearn eza                 runs "eza --help"
 *     LimpetLearn Work:C/MyCopy       runs "Work:C/MyCopy ?"
 *
 * Limpet itself never runs a command to find its arguments. This program
 * does, once, when the user asks for it, and says what it runs. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <exec/memory.h>
#include <stdio.h>
#include <string.h>
#include "../src/completion_learn.h"
#include "../src/completion_args.h"

#ifndef LIMPET_VERSION
#define LIMPET_VERSION "0.0.0"
#endif
#ifndef LIMPET_DATE
#define LIMPET_DATE "1.1.2026"
#endif
static const char version[] __attribute__((used)) = "$VER: LimpetLearn " LIMPET_VERSION " (" LIMPET_DATE ")";

#define TEMPLATE "COMMAND/A,TEMPLATE/S,HELP/S,TO/K,REPLACE/S,SHOW/S"
enum { A_COMMAND, A_TEMPLATE, A_HELP, A_TO, A_REPLACE, A_SHOW, A_COUNT };
#define OUTPUT_MAX 65536

/* The program file the Shell would load: the path as typed, or the name
 * in the current directory, along the Shell's path, in C:. */
static BPTR find_program(const char *command)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct CommandLineInterface *cli = BADDR(me->pr_CLI);
    BPTR *path = cli ? BADDR(cli->cli_CommandDir) : NULL;
    BPTR found, old;
    char in_c[128];
    unsigned entries = 0;
    if ((found = Lock((CONST_STRPTR)command, SHARED_LOCK))) return found;
    if (strchr(command, ':') || strchr(command, '/')) return BNULL;
    while (path && ++entries <= 32) {
        old = CurrentDir(path[1]);
        found = Lock((CONST_STRPTR)command, SHARED_LOCK);
        CurrentDir(old);
        if (found) return found;
        path = BADDR(path[0]);
    }
    if (strlen(command) + 3 > sizeof in_c) return BNULL;
    strcpy(in_c, "C:");
    strcat(in_c, command);
    return Lock((CONST_STRPTR)in_c, SHARED_LOCK);
}

/* Whether the program file holds the text "--help": the sign of a program
 * with Unix-style options. */
static int knows_help(BPTR lock)
{
    static const char key[] = "--help";
    char *buffer = AllocVec(32768 + 8, MEMF_ANY);
    BPTR file = BNULL, old;
    LONG got;
    size_t keep = 0, i;
    int yes = 0;
    if (!buffer) return 0;
    /* The lock becomes the file handle; the caller keeps its own lock. */
    if ((old = DupLock(lock)) && !(file = OpenFromLock(old))) UnLock(old);
    if (!old) file = BNULL;
    if (!file) { FreeVec(buffer); return 0; }
    while (!yes && (got = Read(file, buffer + keep, 32768)) > 0) {
        size_t n = keep + (size_t)got;
        for (i = 0; i + 6 <= n; ++i)
            if (buffer[i] == '-' && !memcmp(buffer + i, key, 6)) { yes = 1; break; }
        keep = n < 5 ? n : 5;
        memmove(buffer, buffer + n - keep, keep);
    }
    Close(file);
    FreeVec(buffer);
    return yes;
}

static int make_drawers(void)
{
    static const char *const drawers[] = { "ENVARC:Limpet", "ENVARC:Limpet/Completions" };
    unsigned i;
    for (i = 0; i < 2; ++i) {
        BPTR lock = Lock((CONST_STRPTR)drawers[i], SHARED_LOCK);
        if (!lock) lock = CreateDir((CONST_STRPTR)drawers[i]);
        if (!lock) return 0;
        UnLock(lock);
    }
    return 1;
}

int main(void)
{
    SIPTR args[A_COUNT] = { 0 };
    struct RDArgs *rd = ReadArgs((CONST_STRPTR)TEMPLATE, args, NULL);
    const char *command, *name;
    char line[500], shown[340], temp_out[64], temp_in[64], to[256], *output = NULL, *definition = NULL;
    BPTR file, lock;
    LONG got = 0, result = 0;
    int asked = 0;
    size_t learned = 0, i, options = 0;
    int help, rc = RETURN_FAIL;
    XArgSpec spec;

    if (!rd) { PrintFault(IoErr(), (CONST_STRPTR)"LimpetLearn"); return RETURN_FAIL; }
    command = (const char *)args[A_COMMAND];
    name = (const char *)FilePart((CONST_STRPTR)command);
    if (!name[0] || strlen(name) >= 40 || strlen(command) > 300) { printf("LimpetLearn: \"%s\" is not a command name\n", command); goto done; }
    for (i = 0; command[i]; ++i)
        if (strchr(" \t\"*?#|<>;`$", command[i]) || (unsigned char)command[i] < 32) {
            printf("LimpetLearn: give the command's name alone, without arguments or patterns\n");
            goto done;
        }
    if (args[A_TEMPLATE] && args[A_HELP]) { printf("LimpetLearn: TEMPLATE and HELP exclude each other\n"); goto done; }

    /* Which question to ask the command. */
    if (args[A_HELP]) help = 1;
    else if (args[A_TEMPLATE]) help = 0;
    else {
        lock = find_program(command);
        help = lock ? knows_help(lock) : 0;
        if (lock) UnLock(lock);
    }

    if (args[A_TO]) {
        if (strlen((const char *)args[A_TO]) >= sizeof to) { printf("LimpetLearn: the TO name is too long\n"); goto done; }
        strcpy(to, (const char *)args[A_TO]);
    } else {
        strcpy(to, "ENVARC:Limpet/Completions/");
        strcat(to, name);
    }
    if (!args[A_SHOW] && !args[A_REPLACE] && (lock = Lock((CONST_STRPTR)to, SHARED_LOCK))) {
        UnLock(lock);
        printf("LimpetLearn: %s exists. Use REPLACE to write it again, or SHOW to look only.\n", to);
        rc = RETURN_WARN;
        goto done;
    }

    sprintf(temp_out, "T:LimpetLearn-%lx.out", (unsigned long)FindTask(NULL));
    sprintf(temp_in, "T:LimpetLearn-%lx.in", (unsigned long)FindTask(NULL));
    /* A command asked with "?" prints its template and then reads its
     * arguments. It gets one empty argument (an open quote): a command
     * that takes none refuses it, one that takes a name finds nothing
     * under it. With no line at all, a command that needs no arguments
     * would go on and do its work. */
    if (!(file = Open((CONST_STRPTR)temp_in, MODE_NEWFILE))) { PrintFault(IoErr(), (CONST_STRPTR)temp_in); goto done; }
    if (!help) Write(file, "\"\n", 2);
    Close(file);
    asked = 1;
    /* The Shell redirects, as it would for a line typed by hand. */
    sprintf(shown, "%s %s", command, help ? "--help" : "?");
    sprintf(line, "%s <%s >%s %s", command, temp_in, temp_out, help ? "--help" : "?");
    printf("Running: %s\n", shown);
    fflush(stdout);
    result = SystemTags((CONST_STRPTR)line, NP_StackSize, 262144, TAG_DONE);

    if (!(output = AllocVec(OUTPUT_MAX, MEMF_ANY)) || !(definition = AllocVec(XARGS_DEFINITION_MAX + 1, MEMF_ANY))) {
        printf("LimpetLearn: not enough memory\n");
        goto done;
    }
    if ((file = Open((CONST_STRPTR)temp_out, MODE_OLDFILE))) {
        got = Read(file, output, OUTPUT_MAX);
        Close(file);
    }
    if (got < 0) got = 0;
    learned = help ? completion_learn_help(output, (size_t)got, definition, XARGS_DEFINITION_MAX)
                   : completion_learn_template(output, (size_t)got, definition, XARGS_DEFINITION_MAX);
    if (!learned || !completion_args_parse(&spec, definition, learned)) {
        printf("LimpetLearn: \"%s\" printed nothing to learn from (return code %ld, %ld bytes of output).\n", shown, (long)result, (long)got);
        if (!args[A_TEMPLATE] && !args[A_HELP])
            printf("Try \"LimpetLearn %s %s\".\n", command, help ? "TEMPLATE" : "HELP");
        rc = RETURN_WARN;
        goto done;
    }
    options = spec.options;

    if (args[A_SHOW]) {
        fwrite(definition, 1, learned, stdout);
        rc = RETURN_OK;
        goto done;
    }
    if (!args[A_TO] && !make_drawers()) { PrintFault(IoErr(), (CONST_STRPTR)"ENVARC:Limpet/Completions"); goto done; }
    if (!(file = Open((CONST_STRPTR)to, MODE_NEWFILE))) { PrintFault(IoErr(), (CONST_STRPTR)to); goto done; }
    sprintf(line, "# %s: learned by LimpetLearn from \"%s %s\". Edit as you like.\n", name, name, help ? "--help" : "?");
    if (Write(file, line, (LONG)strlen(line)) < 0 || Write(file, definition, (LONG)learned) != (LONG)learned) {
        PrintFault(IoErr(), (CONST_STRPTR)to);
        Close(file);
        DeleteFile((CONST_STRPTR)to);
        goto done;
    }
    Close(file);
    if (help) printf("Learned %lu options of %s.\n", (unsigned long)options, name);
    else printf("Learned the template of %s: %lu arguments.\n", name, (unsigned long)spec.count);
    if (args[A_TO]) printf("Written to %s.\n", to);
    else printf("Written to %s. TAB in Limpet uses it from now on.\n", to);
    rc = RETURN_OK;

done:
    if (output) FreeVec(output);
    if (definition) FreeVec(definition);
    if (asked) { DeleteFile((CONST_STRPTR)temp_out); DeleteFile((CONST_STRPTR)temp_in); }
    FreeArgs(rd);
    return rc;
}
