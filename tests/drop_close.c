/* drop-close: Limpet's AppWindow close under drops (src/aros_drop.c), with
 * the order of events set by this program, not by a pointer.
 *
 * Senders are processes of this program that call workbench.library's
 * SendAppWindowMessage, as Wanderer does for a drop: it holds the
 * Workbench semaphore shared and waits for the reply. Checks:
 *
 *   1. a drop reaches the task, and its sender gets its reply;
 *   2. a drop sent after closing began (the last drain by the task done,
 *      RemoveAppWindow not yet called) is replied to without the task,
 *      and the close then ends; a drop after that finds no window;
 *   3. three senders sending without pause while the window closes:
 *      every drop a sender got TRUE for was taken by the task or answered
 *      during the close, the close ends, and no sender is left waiting.
 *
 * Built with -DOLD_DROP against the 0.4.0 candidate's aros_drop.c, check 2
 * shows the hang it fixes: the drop waits for a task that is about to
 * call RemoveAppWindow (the program then replies itself, to get out).
 *
 * Output: one line per check, PASS or FAIL, then a total. */
#include "aros_drop.h"
#include <exec/execbase.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
/* The senders' own opening: aros_drop_close closes WorkbenchBase and sets
   it to NULL while they may still send. */
static struct Library *sender_wb;
#define __WORKBENCH_LIBBASE sender_wb
#include <proto/workbench.h>
#include <stdio.h>
#include <string.h>

#define SENDERS 3
#define FILE_PATH "RAM:drop-close.txt"

static ArosDrop drop;
static struct Window *win;
static char *files[1] = { (char *)FILE_PATH };

/* Set by main before a sender starts; the sender reads only these. */
static volatile int stop;
static volatile int burst[SENDERS];           /* 1: one drop; 0: until stop */
static volatile ULONG sent_true[SENDERS], sent_false[SENDERS];
static volatile int finished[SENDERS];
static volatile int next_id;

static void sender(void)
{
    int id;
    Forbid();
    id = next_id++;
    Permit();
    do {
        if (SendAppWindowMessage(win, 1, files, 0, 10, 10, 0, 0)) sent_true[id]++;
        else sent_false[id]++;
        /* A pause, as between two drags: exec gives a shared request the
           semaphore at once while it is held shared, even with an exclusive
           one waiting, so senders that never pause starve RemoveAppWindow
           (rom/exec/semaphores.c), which no drop by hand can do. */
        if (!burst[id]) Delay(1);
    } while (!burst[id] && !stop);
    finished[id] = 1;
}

static int failures;

static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void start_sender(int id, int once)
{
    char name[32];
    snprintf(name, sizeof name, "drop-close sender %d", id);
    burst[id] = once;
    finished[id] = 0;
    sent_true[id] = sent_false[id] = 0;
    CreateNewProcTags(NP_Entry, (IPTR)sender, NP_Name, (IPTR)name, NP_StackSize, 16384,
                      NP_Output, 0, NP_Input, 0, NP_CloseOutput, FALSE, NP_CloseInput, FALSE,
                      TAG_END);
}

/* 1 when the flag came within ticks (1/50 s each). */
static int wait_flag(volatile int *flag, int ticks)
{
    while (!*flag && ticks-- > 0) Delay(1);
    return *flag;
}

/* A sender's process is gone, not only past its last statement: its code
   is this program's. */
static void wait_gone(int id)
{
    char name[32];
    struct Task *t;
    snprintf(name, sizeof name, "drop-close sender %d", id);
    for (;;) {
        Forbid();
        t = FindTask((STRPTR)name);
        Permit();
        if (!t) return;
        Delay(1);
    }
}

#ifdef OLD_DROP
#define close_begin(d) aros_drop_refuse(d)
#define close_end(d) aros_drop_close(d)
#else
#define close_begin(d) aros_drop_close_begin(d)
#define close_end(d) aros_drop_close_end(d)
#endif

int main(void)
{
    ArosDropped got;
    BPTR fh;
    int i;
#ifndef OLD_DROP
    ULONG taken = 0, total_true = 0, answered = 0;
#endif

    if (!(sender_wb = OpenLibrary((CONST_STRPTR)"workbench.library", 0))) { puts("FAIL no workbench.library"); return 20; }
    if (!(fh = Open((CONST_STRPTR)FILE_PATH, MODE_NEWFILE))) { puts("FAIL cannot create " FILE_PATH); return 20; }
    Close(fh);
    win = OpenWindowTags(NULL, WA_Left, 20, WA_Top, 20, WA_Width, 240, WA_Height, 60,
                         WA_Title, (IPTR)"drop-close", WA_DragBar, TRUE, TAG_END);
    if (!win) { puts("FAIL no window"); return 20; }

    /* 1 */
    check(aros_drop_open(&drop, win), "1 open: the window is an AppWindow");
    next_id = 0;
    start_sender(0, 1);
    for (i = 0; i < 150 && !aros_drop_take(&drop, &got); i++) Delay(1);
    check(i < 150 && !got.error && got.count == 1 && !strcmp(got.paths[0], "RAM Disk:drop-close.txt"),
          "1 a drop reaches the task with its full path");
    check(wait_flag(&finished[0], 150) && sent_true[0] == 1, "1 its sender has its reply");
    wait_gone(0);

    /* 2 */
    close_begin(&drop);
    next_id = 0;
    start_sender(0, 1);
    if (!wait_flag(&finished[0], 150)) {
        check(0, "2 a drop after closing began is answered without the task (it waits: RemoveAppWindow would hang)");
        while (!aros_drop_refuse(&drop)) Delay(1);
        wait_flag(&finished[0], 500);
    } else {
#ifndef OLD_DROP
        check(sent_true[0] == 1 && drop.answered == 1, "2 a drop after closing began is answered without the task");
#endif
    }
    wait_gone(0);
    close_end(&drop);
    check(1, "2 the close ended");
#ifndef OLD_DROP
    check(!drop.port && !drop.inbox, "2 both ports are gone");
#endif
    next_id = 0;
    start_sender(0, 1);
    check(wait_flag(&finished[0], 150) && sent_false[0] == 1 && sent_true[0] == 0,
          "2 a drop after the close finds no window");
    wait_gone(0);

#ifndef OLD_DROP
    /* 3 */
    check(aros_drop_open(&drop, win), "3 open again");
    stop = 0;
    next_id = 0;
    for (i = 0; i < SENDERS; i++) start_sender(i, 0);
    for (i = 0; i < 50; i++) {                  /* a second of drops taken as usual */
        while (aros_drop_take(&drop, &got)) taken++;
        Delay(1);
    }
    while (aros_drop_take(&drop, &got)) taken++;
    close_begin(&drop);
    Delay(10);                                  /* drops keep coming while closing */
    answered = close_end(&drop);                /* the ports are freed here */
    stop = 1;
    for (i = 0; i < SENDERS; i++) {
        check(wait_flag(&finished[i], 250), "3 a sender ended (none left waiting for a reply)");
        wait_gone(i);
        total_true += sent_true[i];
    }
    printf("     taken by the task %lu, answered in the close %lu, senders' TRUE %lu, FALSE after %lu %lu %lu\n",
           (unsigned long)taken, (unsigned long)answered, (unsigned long)total_true,
           (unsigned long)sent_false[0], (unsigned long)sent_false[1], (unsigned long)sent_false[2]);
    check(taken > 0 && answered > 0, "3 drops reached the task before the close and were answered during it");
    check(total_true == taken + answered, "3 every drop with a reply was taken or answered, none twice");
#endif

    CloseWindow(win);
    CloseLibrary(sender_wb);
    DeleteFile((CONST_STRPTR)FILE_PATH);
    printf("%s: %d failed\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 10 : 0;
}
