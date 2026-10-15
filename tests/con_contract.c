/* Guest probe: the console contract of a CON:-style handler, checked the same
 * way for CON: and Limpet's handler, LIMPET: (doc/limpet-handler.md). The program opens the
 * console itself, by the name it is given, as any program that opens CON:
 * does.
 *
 *   con-contract NAME LOG            checks that need no keys
 *   con-contract NAME LOG keys       then the ones that need someone typing
 *   con-contract NAME LOG shared     several readers and waiters at once (keys)
 *
 * Every line of LOG is "PASS id ...", "FAIL id ..." or "OBS id ..." (an
 * observation where the two consoles are allowed to differ). Pointers the
 * handler returns are printed, never dereferenced. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <exec/tasks.h>
#include <exec/io.h>
#include <devices/conunit.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static BPTR logfile;
static int failures;

static void say(const char *verdict, const char *id, const char *fmt, ...)
{
    char line[256]; int n; va_list ap;
    n = snprintf(line, sizeof line, "%s %s ", verdict, id);
    va_start(ap, fmt);
    vsnprintf(line + n, sizeof line - n, fmt, ap);
    va_end(ap);
    strncat(line, "\n", sizeof line - strlen(line) - 1);
    FPuts(logfile, (CONST_STRPTR)line); Flush(logfile);
    if (!strcmp(verdict, "FAIL")) ++failures;
}
#define CHECK(id, cond, ...) say((cond) ? "PASS" : "FAIL", id, __VA_ARGS__)

static ULONG now_ms(void)
{
    struct DateStamp d; DateStamp(&d);
    return (ULONG)d.ds_Minute * 60000UL + (ULONG)d.ds_Tick * 20UL;
}

/* Writers: each its own handle on the same console, from DupLockFromFH. */
#define WRITERS 3
#define WRITES 300
static BPTR writer_handle[WRITERS];
static volatile LONG writer_failed[WRITERS], writer_done[WRITERS];
static struct Task *parent;
static void writer(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    int k = (int)(IPTR)me->pr_Task.tc_UserData, i;
    char line[160];
    /* No C library in this child: dos.library only. */
    for (i = 0; i < WRITES; ++i) {
        int n = 0, j;
        const char *w = "writer ";
        while (*w) line[n++] = *w++;
        line[n++] = '0' + k; line[n++] = ' ';
        line[n++] = '0' + i / 100; line[n++] = '0' + i / 10 % 10; line[n++] = '0' + i % 10;
        for (j = 0; j < 120; ++j) line[n++] = '.';
        line[n++] = '\n';
        if (Write(writer_handle[k], line, n) != n) ++writer_failed[k];
    }
    writer_done[k] = 1;
    Signal(parent, SIGBREAKF_CTRL_F);
}


/* The console's unit as ACTION_DISK_INFO gives it: columns and rows, 0 if none. */
static void unit_size(BPTR h, int *cols, int *rows)
{
    struct FileHandle *fh = BADDR(h);
    struct InfoData info;
    *cols = *rows = 0;
    memset(&info, 0, sizeof info);
    if (DoPkt(fh->fh_Type, ACTION_DISK_INFO, (SIPTR)MKBADDR(&info), 0, 0, 0, 0) && info.id_InUse) {
        struct ConUnit *u = (struct ConUnit *)((struct IOStdReq *)info.id_InUse)->io_Unit;
        if (u) { *cols = u->cu_XMax + 1; *rows = u->cu_YMax + 1; }
    }
}

/* Send a query in raw mode and collect the reply a byte at a time (each
   Read takes one byte, so a reply split between reads is put together
   here). Returns its length; hex gets it for the log. */
static int ask(BPTR h, const char *query, int qn, char *got, int room, char *hex, int hexroom)
{
    LONG n = 0, k; int j = 0;
    Write(h, (APTR)query, qn);
    while (n < room - 1 && WaitForChar(h, n ? 100000 : 1000000)) {
        k = Read(h, got + n, 1);
        if (k != 1) break;
        n += k;
        if ((unsigned char)got[n - 1] >= 0x40 && n > 1) break;
    }
    got[n] = 0;
    for (k = 0; k < n && j < hexroom - 3; ++k)
        j += snprintf(hex + j, hexroom - j, "%02x", (unsigned char)got[k]);
    hex[j] = 0;
    return (int)n;
}

/* The window-bounds and cursor reports, compared with the console unit:
   AmigaOS and mainline AROS answer one-based (rows;columns, row;column),
   the console.device of ABIv11 one lower. */
static void reports(BPTR h, const char *tag)
{
    char got[64], hex[140], id[48];
    int n, a, b, c, d, cols, rows;
    SetMode(h, 1);
    unit_size(h, &cols, &rows);
    n = ask(h, "\x9b" "0 q", 4, got, sizeof got, hex, sizeof hex);
    snprintf(id, sizeof id, "report.bounds%s", tag);
    if (n && sscanf(got + 1, "%d;%d;%d;%d r", &a, &b, &c, &d) == 4 && (unsigned char)got[0] == 0x9b)
        say("OBS", id, "%s rows=%d columns=%d; unit %dx%d: %s", hex, c, d, rows, cols,
            c == rows && d == cols ? "one-based" : c == rows - 1 && d == cols - 1 ? "one lower (zero-based)" : "differs");
    else say(n ? "OBS" : "FAIL", id, "%s", n ? hex : "no answer in 1 s");
    Write(h, "\x9b" "3;5H", 5);
    n = ask(h, "\x9b" "6n", 3, got, sizeof got, hex, sizeof hex);
    snprintf(id, sizeof id, "report.cursor%s", tag);
    if (n && sscanf(got + 1, "%d;%dR", &a, &b) == 2 && (unsigned char)got[0] == 0x9b)
        say("OBS", id, "%s after CSI 3;5H: row=%d column=%d: %s", hex, a, b,
            a == 3 && b == 5 ? "one-based" : a == 2 && b == 4 ? "one lower (zero-based)" : "differs");
    else say(n ? "OBS" : "FAIL", id, "%s", n ? hex : "no answer in 1 s");
    Write(h, "\n", 1);
    SetMode(h, 0);
}

static void lf_mode(BPTR h);
static void quiet_checks(BPTR h, const char *name)
{
    struct FileHandle *fh = BADDR(h);
    struct MsgPort *port = fh->fh_Type;
    struct InfoData info;
    struct FileInfoBlock *fib;
    SIPTR r; LONG e; ULONG t0;
    BPTR lock, dup;
    struct Process *me = (struct Process *)FindTask(NULL);

    CHECK("interactive", IsInteractive(h), "IsInteractive=%ld", (long)IsInteractive(h));
    r = Write(h, "con-contract: hello\n", 20);
    CHECK("write", r == 20, "Write=%ld", (long)r);

    memset(&info, 0, sizeof info);
    SetIoErr(0);
    r = DoPkt(port, ACTION_DISK_INFO, (SIPTR)MKBADDR(&info), 0, 0, 0, 0); e = IoErr();
    CHECK("disk_info", r != 0, "res=%ld err=%ld", (long)r, (long)e);
    say("OBS", "disk_info.fields", "type=%08lx window=%s in_use=%s",
        (unsigned long)info.id_DiskType,
        info.id_VolumeNode == (BPTR)(SIPTR)-1 ? "-1" : info.id_VolumeNode ? "set" : "0",
        info.id_InUse ? "set" : "0");
    /* posixc's TIOCGWINSZ reads through id_InUse to a ConUnit
       (compiler/crt/posixc/ioctl.c); so does this, only when it is set. */
    if (info.id_InUse) {
        struct ConUnit *u = (struct ConUnit *)((struct IOStdReq *)info.id_InUse)->io_Unit;
        CHECK("disk_info.unit", u && u->cu_XMax >= 0 && u->cu_YMax >= 0, "columns=%d rows=%d window=%s",
              u ? u->cu_XMax + 1 : 0, u ? u->cu_YMax + 1 : 0,
              u && u->cu_Window && (BPTR)(IPTR)u->cu_Window == info.id_VolumeNode ? "same" : "other");
    } else say("FAIL", "disk_info.unit", "id_InUse is 0: TIOCGWINSZ reads through a null pointer");

    r = WaitForChar(h, 0);
    CHECK("wait_char.poll", r == 0, "WaitForChar(0)=%ld (no key pressed)", (long)r);
    t0 = now_ms();
    r = WaitForChar(h, 300000);
    t0 = now_ms() - t0;
    CHECK("wait_char.timeout", r == 0, "WaitForChar(300 ms)=%ld after %lu ms", (long)r, (unsigned long)t0);

    r = SetMode(h, 1); e = IoErr();
    CHECK("set_mode.raw", r != 0, "SetMode(1)=%ld err=%ld", (long)r, (long)e);
    r = SetMode(h, 0); e = IoErr();
    CHECK("set_mode.cooked", r != 0, "SetMode(0)=%ld err=%ld", (long)r, (long)e);

    t0 = now_ms();
    r = Seek(h, 0, OFFSET_CURRENT); e = IoErr();
    t0 = now_ms() - t0;
    CHECK("seek", r == -1 && t0 < 1000, "Seek=%ld err=%ld in %lu ms", (long)r, (long)e, (unsigned long)t0);

    fib = AllocDosObject(DOS_FIB, NULL);
    if (fib) {
        r = ExamineFH(h, fib); e = IoErr();
        CHECK("examine_fh", !r && e == ERROR_ACTION_NOT_KNOWN, "ExamineFH=%ld err=%ld", (long)r, (long)e);
        FreeDosObject(DOS_FIB, fib);
    }

    lock = DupLockFromFH(h); e = IoErr();
    CHECK("dup_lock", lock != 0, "DupLockFromFH=%s err=%ld", lock ? "lock" : "0", (long)e);
    if (lock) {
        dup = OpenFromLock(lock); e = IoErr();
        CHECK("open_from_lock", dup != 0, "OpenFromLock=%s err=%ld", dup ? "fh" : "0", (long)e);
        if (dup) {
            r = Write(dup, "con-contract: through a duplicate\n", 34);
            CHECK("dup.write", r == 34, "Write=%ld", (long)r);
            CHECK("dup.interactive", IsInteractive(dup), "IsInteractive=%ld", (long)IsInteractive(dup));
            Close(dup);
        } else UnLock(lock);
    }

    {   /* "*" and CONSOLE: on this console, as stdiowin and Sashimi do. */
        struct MsgPort *old = SetConsoleTask(port);
        BPTR star = Open((CONST_STRPTR)"*", MODE_NEWFILE);
        BPTR cons = Open((CONST_STRPTR)"CONSOLE:", MODE_OLDFILE);
        CHECK("open_star", star && BADDR(star) && ((struct FileHandle *)BADDR(star))->fh_Type == port,
              "Open(*)=%s same port=%d", star ? "fh" : "0",
              star ? ((struct FileHandle *)BADDR(star))->fh_Type == port : 0);
        if (star) { r = Write(star, "con-contract: through *\n", 24); CHECK("star.write", r == 24, "Write=%ld", (long)r); Close(star); }
        CHECK("open_console", cons != 0, "Open(CONSOLE:)=%s err=%ld", cons ? "fh" : "0", (long)IoErr());
        if (cons) Close(cons);
        SetConsoleTask(old);
    }

    {   /* Break target: the old one comes back in Res2. */
        SIPTR old;
        r = DoPkt(port, ACTION_CHANGE_SIGNAL, fh->fh_Arg1, (SIPTR)me, 0, 0, 0);
        old = IoErr();
        CHECK("change_signal", r != 0, "res=%ld", (long)r);
        say("OBS", "change_signal.old", "%s", old == (SIPTR)me ? "this process" : old ? "another" : "0");
    }

    reports(h, "");
    lf_mode(h);

    {   /* Writers at once: none may fail. */
        int k; LONG failed = 0; ULONG got = 0;
        parent = FindTask(NULL);
        SetSignal(0, SIGBREAKF_CTRL_F);
        for (k = 0; k < WRITERS; ++k) {
            BPTR l = DupLockFromFH(h);
            writer_handle[k] = l ? OpenFromLock(l) : 0;
            if (l && !writer_handle[k]) UnLock(l);
            writer_failed[k] = writer_done[k] = 0;
            if (!writer_handle[k] || !CreateNewProcTags(NP_Entry, (IPTR)writer, NP_Name, (IPTR)"con-contract writer",
                    NP_UserData, (IPTR)k, NP_Output, 0, NP_Input, 0, NP_CloseOutput, FALSE, NP_CloseInput, FALSE,
                    NP_CurrentDir, 0, NP_HomeDir, 0, TAG_DONE)) {
                writer_failed[k] = WRITES; writer_done[k] = 1;
            }
        }
        for (;;) {
            int all = 1;
            for (k = 0; k < WRITERS; ++k) all &= writer_done[k] != 0;
            if (all) break;
            Wait(SIGBREAKF_CTRL_F);
        }
        for (k = 0; k < WRITERS; ++k) { failed += writer_failed[k]; got += WRITES - writer_failed[k];
            if (writer_handle[k]) Close(writer_handle[k]); }
        CHECK("writers", !failed, "%d writers x %d writes: %ld failed, %lu written", WRITERS, WRITES,
              (long)failed, (unsigned long)got);
    }
    (void)name;
}

/* Line feed mode (console.device M_LNM, RKM: on by default; CSI 20 h sets
   it, CSI 20 l clears it): with it a line feed also returns the carriage.
   Measured with the cursor report after a line feed from column 10. */
static int column_after_lf(BPTR h)
{
    char got[64], hex[140]; int n, row, col;
    Write(h, "\x9b" "1;10H\n", 7);
    n = ask(h, "\x9b" "6n", 3, got, sizeof got, hex, sizeof hex);
    if (n && sscanf(got + 1, "%d;%dR", &row, &col) == 2) return col;
    return -1;
}
static void lf_mode(BPTR h)
{
    int c1, c2, c3;
    SetMode(h, 1);
    c1 = column_after_lf(h);
    Write(h, "\x9b" "20l", 4);
    c2 = column_after_lf(h);
    Write(h, "\x9b" "20h", 4);
    c3 = column_after_lf(h);
    Write(h, "\r\n", 2);
    SetMode(h, 0);
    /* Column 1 (0 on ABIv11's CON:, which reports one lower) is the left. */
    say("OBS", "lf.default", "column %d after LF from 10: %s", c1, c1 <= 1 && c1 >= 0 ? "returns (LF mode on)" : "stays (LF mode off)");
    say("OBS", "lf.reset", "after CSI 20l: column %d: %s", c2, c2 >= 9 ? "stays" : "returns");
    say("OBS", "lf.set", "after CSI 20h: column %d: %s", c3, c3 <= 1 && c3 >= 0 ? "returns" : "stays");
    CHECK("lf.control", c1 >= 0 && c1 <= 1 && c2 >= 9 && c3 >= 0 && c3 <= 1,
          "on by default, CSI 20l clears it, CSI 20h sets it (%d/%d/%d)", c1, c2, c3);
}

/* Several clients reading and waiting at once. Each child has its own
   handle on the console (DupLockFromFH), as Run and SystemTags make them. */
#define SHARED_MAX 2
static BPTR shared_handle[SHARED_MAX];
static volatile LONG shared_result[SHARED_MAX], shared_done[SHARED_MAX];
static volatile char shared_text[SHARED_MAX][16];
static volatile int shared_kind[SHARED_MAX]; /* 0 read, 1 wait 3 s */
static void sharer(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    int k = (int)(IPTR)me->pr_Task.tc_UserData;
    if (shared_kind[k]) shared_result[k] = WaitForChar(shared_handle[k], 3000000);
    else {
        char buf[16]; LONG n = Read(shared_handle[k], buf, sizeof buf - 1), i;
        shared_result[k] = n;
        for (i = 0; i < n && i < 15; ++i) shared_text[k][i] = buf[i];
    }
    shared_done[k] = 1;
    Signal(parent, SIGBREAKF_CTRL_F);
}
static int start_sharer(BPTR h, int k, int kind)
{
    BPTR l = DupLockFromFH(h);
    shared_handle[k] = l ? OpenFromLock(l) : 0;
    if (l && !shared_handle[k]) UnLock(l);
    shared_done[k] = 0; shared_result[k] = -9; shared_kind[k] = kind;
    memset((char *)shared_text[k], 0, sizeof shared_text[k]);
    return shared_handle[k] && CreateNewProcTags(NP_Entry, (IPTR)sharer, NP_Name, (IPTR)"con-contract sharer",
        NP_UserData, (IPTR)k, NP_Output, 0, NP_Input, 0, NP_CloseOutput, FALSE, NP_CloseInput, FALSE,
        NP_CurrentDir, 0, NP_HomeDir, 0, TAG_DONE) != NULL;
}
/* Wait for a child up to ticks/50 s; 0 if it never came back. */
static int await_sharer(int k, LONG ticks)
{
    while (!shared_done[k] && ticks > 0) { Delay(10); ticks -= 10; }
    return shared_done[k] != 0;
}
static void shared_reads(BPTR h)
{
    char buf[32]; LONG n; int ok;
    parent = FindTask(NULL);
    /* A child reads first, then this process: the first line is the child's. */
    ok = start_sharer(h, 0, 0);
    Delay(25);
    FPuts(h, (CONST_STRPTR)"\nTwo readers: type one, Return, then two, Return: "); Flush(h);
    n = Read(h, buf, sizeof buf - 1);
    if (n >= 0) buf[n] = 0;
    ok = ok && await_sharer(0, 500);
    CHECK("readers.fifo", ok && shared_result[0] == 4 && !memcmp((char *)shared_text[0], "one\n", 4) && n == 4 && !memcmp(buf, "two\n", 4),
          "child Read=%ld \"%.6s\", this Read=%ld \"%.6s\"%s", (long)shared_result[0], (char *)shared_text[0],
          (long)n, n > 0 ? buf : "", ok ? "" : ", child never answered");
    if (shared_done[0]) Close(shared_handle[0]);

    /* A waiter and a reader at once: a key ends both. */
    ok = start_sharer(h, 1, 1);
    Delay(10);
    FPuts(h, (CONST_STRPTR)"Waiter and reader: type w and Return within 3 s: "); Flush(h);
    n = Read(h, buf, sizeof buf - 1);
    ok = ok && await_sharer(1, 500);
    CHECK("wait.with_read", ok && shared_result[1] != 0 && n == 2,
          "child WaitForChar=%ld, this Read=%ld%s", (long)shared_result[1], (long)n, ok ? "" : ", child never answered");
    if (shared_done[1]) Close(shared_handle[1]);

    /* Two waiters, no key: both time out and both are answered. CON: keeps
       one waiting packet and loses the first. */
    ok = start_sharer(h, 1, 1);
    Delay(10);
    n = WaitForChar(h, 1000000);
    ok = ok && await_sharer(1, 400);
    CHECK("wait.two", ok && shared_result[1] == 0 && n == 0,
          "child WaitForChar=%ld, this WaitForChar(1 s)=%ld%s", (long)shared_result[1], (long)n,
          ok ? "" : ", the child's wait was never answered (left out)");
    if (shared_done[1]) Close(shared_handle[1]);
}

static void key_checks(BPTR h)
{
    char buf[80]; LONG n, m, o;
    FPuts(h, (CONST_STRPTR)"\nType abc and press Return: "); Flush(h);
    n = Read(h, buf, sizeof buf - 1);
    if (n >= 0) buf[n] = 0;
    CHECK("cooked.read", n == 4 && !memcmp(buf, "abc\n", 4), "Read=%ld \"%.20s\"", (long)n, n > 0 ? buf : "");

    SetMode(h, 1);
    Write(h, "Raw mode: press x\n", 18);
    n = Read(h, buf, 1);
    CHECK("raw.read", n == 1 && buf[0] == 'x', "Read=%ld byte=%d", (long)n, n == 1 ? (unsigned char)buf[0] : -1);
    n = WaitForChar(h, 0);
    CHECK("raw.no_echo_left", n == 0, "WaitForChar(0)=%ld", (long)n);
    SetSignal(0, SIGBREAKF_CTRL_D);
    Write(h, "Raw mode: press Ctrl-D\n", 23);
    n = Read(h, buf, 1);
    CHECK("raw.ctrl_d", n == 1 && buf[0] == 4 && !(SetSignal(0, SIGBREAKF_CTRL_D) & SIGBREAKF_CTRL_D),
          "Read=%ld byte=%d, no signal", (long)n, n == 1 ? (unsigned char)buf[0] : -1);
    Write(h, "Raw mode: resize the window, then press y\n", 42);
    n = Read(h, buf, 1);
    SetMode(h, 0);
    reports(h, ".after_resize");

    {   /* Ctrl-C, D, E, F in cooked mode: the signal to this reader. */
        static const char *const names[] = {"break", "break_d", "break_e", "break_f"};
        unsigned i;
        for (i = 0; i < 4; ++i) {
            ULONG sig = SIGBREAKF_CTRL_C << i;
            char prompt[48];
            SetSignal(0, sig);
            snprintf(prompt, sizeof prompt, "Press Ctrl-%c, then Return: ", 'C' + i);
            FPuts(h, (CONST_STRPTR)prompt); Flush(h);
            n = Read(h, buf, sizeof buf - 1);
            CHECK(names[i], (SetSignal(0, sig) & sig) != 0, "Ctrl-%c reached the reader; Read=%ld", 'C' + i, (long)n);
        }
    }

    /* End of input from the keyboard: what was typed first, then one 0,
       then reads wait as before. */
    FPuts(h, (CONST_STRPTR)"Type xy, then end input (Ctrl-\\): "); Flush(h);
    n = Read(h, buf, sizeof buf - 1);
    if (n >= 0) buf[n] = 0;
    m = Read(h, buf + 40, 39);
    CHECK("eof.partial", n == 2 && !memcmp(buf, "xy", 2), "first Read=%ld \"%.10s\"", (long)n, n > 0 ? buf : "");
    CHECK("eof.zero", m == 0, "second Read=%ld", (long)m);
    FPuts(h, (CONST_STRPTR)"\nInput again: type z and press Return: "); Flush(h);
    o = Read(h, buf, sizeof buf - 1);
    CHECK("eof.once", o == 2 && buf[0] == 'z', "third Read=%ld", (long)o);
}

int main(int argc, char **argv)
{
    BPTR h;
    if (argc < 3) { PutStr((CONST_STRPTR)"Usage: con-contract NAME LOG [keys]\n"); return 20; }
    logfile = Open((CONST_STRPTR)argv[2], MODE_NEWFILE);
    if (!logfile) return 20;
    FPrintf(logfile, (CONST_STRPTR)"con-contract format=1 name=%s\n", (IPTR)argv[1]);
    h = Open((CONST_STRPTR)argv[1], MODE_NEWFILE);
    CHECK("open", h != 0, "Open=%s err=%ld", h ? "fh" : "0", (long)IoErr());
    if (h) {
        quiet_checks(h, argv[1]);
        if (argc > 3 && !strcmp(argv[3], "keys")) key_checks(h);
        if (argc > 3 && !strcmp(argv[3], "shared")) shared_reads(h);
        Close(h);
        say("PASS", "close", "Close returned");
    }
    FPrintf(logfile, (CONST_STRPTR)"END failures=%ld\n", (long)failures);
    Close(logfile);
    return failures ? 5 : 0;
}
