/* Made by tools/completions/make-defs.py from data/completions. Do not edit. */
#include "completion_defs.h"
#include <stddef.h>

static const struct { const char *name, *text; } definitions[] = {
    { "AddBuffers", "template DRIVE/A,BUFFERS/N\ndirs DRIVE\n" },
    { "Alias", "template NAME,STRING/F\n" },
    { "apkg", "first search show install list update outdated recent upgrade rollback remove verify info doctor run open self-update\n" },
    { "Assign", "template NAME,TARGET/M,LIST/S,EXISTS/S,DISMOUNT/S,DEFER/S,PATH/S,ADD=APPEND/S,PREPEND/S,REMOVE/S,VOLS/S,DIRS/S,DEVICES/S\ndirs TARGET\n" },
    { "Avail", "template CHIP/S,FAST/S,TOTAL/S,FLUSH/S,H=HUMAN/S\n" },
    { "Break", "template PROCESS/N,PORT,NAME/K,C/S,D/S,E/S,F/S,ALL/S\n" },
    { "CD", "template DIR\ndirs DIR\n" },
    { "ChangeTaskPri", "template PRI=PRIORITY/A/N,PROCESS/N\n" },
    { "Copy", "template FROM/M/A,TO/A,PAT=PATTERN/K,BUF=BUFFER/K/N,ALL/S,DIRECT/S,CLONE/S,DATES/S,NOPRO/S,COM=COMMENT/S,QUIET/S,NOREQ/S,ERRWARN/S,MAKEDIR/S,MOVE/S,DELETE/S,HARD=HARDLINK/S,SOFT=SOFTLINK/S,FOLNK=FORCELINK/S,FODEL=FORCEDELETE/S,FOOVR=FORCEOVERWRITE/S,DONTOVR=DONTOVERWRITE/S,FORCE/S,NEWER/S\n" },
    { "Date", "template DAY,DATE,TIME,TO=VER/K\n" },
    { "Delete", "template FILE/M/A,ALL/S,QUIET/S,FORCE/S,FOLLOWLINKS/S\n" },
    { "Dir", "template DIR,OPT/K,ALL/S,DIRS/S,FILES/S,INTER/S\ndirs DIR\n" },
    { "Echo", "template /M,NOLINE/S,FIRST/K/N,LEN/K/N,TO/K\n" },
    { "Eval", "template VALUE1/A,OP,VALUE2/M,TO/K,LFORMAT/K\n" },
    { "FailAt", "template RCLIM/N\n" },
    { "Fault", "template NUMBERS/N/M\n" },
    { "Filenote", "template FILE/A,COMMENT,ALL/S,QUIET/S\n" },
    { "Get", "template NAME/A\n" },
    { "GetEnv", "template NAME/A\n" },
    { "Info", "template DISKS/S,VOLS=VOLUMES/S,ALL/S,BLOCKS/S,DEVICES/M\n" },
    { "Join", "template FILE/M/A,AS=TO/K/A\n" },
    { "List", "template DIR/M,P=PAT/K,KEYS/S,DATES/S,NODATES/S,TO/K,SUB/K,SINCE/K,UPTO/K,QUICK/S,BLOCK/S,NOHEAD/S,FILES/S,DIRS/S,LFORMAT/K,ALL/S\n" },
    { "Lock", "template DRIVE/A,ON/S,OFF/S,PASSKEY\ndirs DRIVE\n" },
    { "MakeDir", "template NAME/M/A,ALL/S\ndirs NAME\n" },
    { "MakeLink", "template FROM/A,TO/A,HARD/S,FORCE/S\n" },
    { "Mount", "template DEVICE/M,FROM/K\n" },
    { "Path", "template PATH/M,ADD/S,SHOW/S,RESET/S,REMOVE/S,QUIET/S,HEAD/S\ndirs PATH\n" },
    { "Protect", "template FILE/A,FLAGS,ADD/S,SUB/S,ALL/S,QUIET/S\n" },
    { "Relabel", "template DRIVE/A,NAME/A\ndirs DRIVE\n" },
    { "Rename", "template FROM/A/M,TO=AS/A,QUIET/S\n" },
    { "Resident", "template NAME,FILE,REMOVE/S,ADD/S,REPLACE/S,PURE=FORCE/S,SYSTEM/S\n" },
    { "Search", "template FROM/M,SEARCH/A,ALL/S,NONUM/S,QUIET/S,QUICK/S,FILE/S,PATTERN/S,CASE/S,LINES/N\n" },
    { "SetDate", "template FILE/A,WEEKDAY,DATE,TIME,ALL/S\n" },
    { "SetEnv", "template NAME,SAVE/S,STRING/F\n" },
    { "Sort", "template FROM/A,TO/A,COLSTART/K,CASE/S,NUMERIC/S\n" },
    { "Stack", "template SIZE/N\n" },
    { "Touch", "template NAME/A\n" },
    { "Type", "template FROM/A/M,TO/K,OPT/K,HEX/S,NUMBER/S\n" },
    { "Unalias", "template NAME\n" },
    { "Unset", "template NAME\n" },
    { "UnsetEnv", "template NAME,SAVE/S\n" },
    { "Version", "template NAME/M,MD5SUM/S,VERSION/N,REVISION/N,FILE/S,FULL/S,RES/S,ARCH/S\n" },
    { "Wait", "template TIME/N,SEC=SECS/S,MIN=MINS/S,UNTIL/K\n" },
    { "Which", "template FILE/A,NORES/S,RES/S,ALL/S\n" },
};

static unsigned fold(unsigned c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }

const char *completion_definition(const char *name)
{
    size_t i, k;
    for (i = 0; i < sizeof definitions / sizeof definitions[0]; ++i) {
        const char *have = definitions[i].name;
        for (k = 0; have[k] && fold((unsigned char)have[k]) == fold((unsigned char)name[k]); ++k) {}
        if (!have[k] && !name[k]) return definitions[i].text;
    }
    return NULL;
}

const char *completion_definition_name(unsigned n)
{
    return n < sizeof definitions / sizeof definitions[0] ? definitions[n].name : NULL;
}
