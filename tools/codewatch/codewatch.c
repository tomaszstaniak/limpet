/*
 * codewatch: who writes into, or frees, a running program's code?
 *
 *   codewatch ADDR                      print the 16 bytes at ADDR
 *   Run >NIL: codewatch ADDR LOG        watch ADDR (see below)
 *   codewatch OVERLAP N [ADDR...]       tasks whose stack range overlaps a hunk
 *                                       of Shell process N's command; decode ADDR
 *   Run >NIL: codewatch CLI N SYMBOL LOG
 *       find the code section of Shell process N's command whose name contains
 *       SYMBOL, watch it, and copy every .text section of that command; the
 *       copies are compared every two ticks and the first difference is logged
 *       (section, offset, old and new bytes, the new words decoded), after
 *       which the watchpoints move to the changed place
 *
 * Watching: x86 write watchpoints on 16 bytes (DR0/DR1 through Supervisor(), a
 * kernel.resource handler for vector 1; one CPU) and an exec FreeMem patch
 * that notices a free covering the address. Each event appends the task, the
 * writer's RIP and registers or the freeing call, and the decoded stack to
 * LOG. The process stays resident until reset; the patch is never removed.
 *
 * Diagnostic only, for a disposable test machine. Made for Limpet 0.9.0 on
 * AROS One (ABIv11), whose code was found overwritten with stack-frame-like
 * words. Patching exec Forbid/Permit to catch a stack pointer inside the
 * watched code was tried and stalled the system; it is not part of this tool.
 */
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <libraries/debug.h>
#include <utility/tagitem.h>
#include <aros/asmcall.h>
#include <aros/libcall.h>
#include <aros/x86_64/cpucontext.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/debug.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

APTR KernelBase;
struct Library *DebugBase;

#define KrnAddExceptionHandler(irq, h, d1, d2) \
    AROS_LC4(void *, KrnAddExceptionHandler, \
        AROS_LCA(uint8_t, (irq), D0), AROS_LCA(void *, (h), A0), \
        AROS_LCA(void *, (d1), A1), AROS_LCA(void *, (d2), A2), \
        APTR, KernelBase, 14, Kernel)

#define STACK_WORDS 192
#define EVENTS 8

enum { EV_WRITE = 1, EV_FREE };

struct event {
    int kind;
    struct Task *task;
    char name[40];
    IPTR rip, caller, mem, size;
    struct ExceptionContext ctx;
    UBYTE bytes[16];
    IPTR stack[STACK_WORDS];
    int words;
};

static IPTR watch;
static struct Task *me;
static struct event events[EVENTS];
static volatile int nevents;
static volatile IPTR dr_value0, dr_value1, dr_value7;
static APTR old_FreeMem;

extern void dr_load(void);
__asm__(
    "    .text\n"
    "    .globl dr_load\n"
    "dr_load:\n"
    "    movq dr_value0(%rip), %rax\n"
    "    movq %rax, %dr0\n"
    "    movq dr_value1(%rip), %rax\n"
    "    movq %rax, %dr1\n"
    "    xorq %rax, %rax\n"
    "    movq %rax, %dr6\n"
    "    movq dr_value7(%rip), %rax\n"
    "    movq %rax, %dr7\n"
    "    iretq\n");

static void copy_name(char *dst, struct Task *t)
{
    const char *s = t && t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "?";
    int i;
    for (i = 0; i < 39 && s[i]; i++)
        dst[i] = s[i];
    dst[i] = 0;
}

static struct event *new_event(int kind)
{
    struct event *e;
    if (nevents >= EVENTS)
        return NULL;
    e = &events[nevents];
    memset(e, 0, sizeof *e);
    e->kind = kind;
    e->task = FindTask(NULL);
    copy_name(e->name, e->task);
    memcpy(e->bytes, (void *)watch, 16);
    return e;
}

static void take_stack(struct event *e, IPTR *sp, APTR upper)
{
    int k;
    for (k = 0; k < STACK_WORDS && (!upper || (APTR)(sp + k) < upper); k++)
        e->stack[k] = sp[k];
    e->words = k;
}

static int debug_handler(struct ExceptionContext *ctx, void *d1, void *d2)
{
    IPTR dr6;
    struct event *e;
    (void)d1; (void)d2;
    __asm__ __volatile__("movq %%dr6, %0" : "=r"(dr6));
    if (!(dr6 & 3))
        return 0;
    __asm__ __volatile__("movq %0, %%dr7; movq %0, %%dr6" :: "r"(0UL));
    e = new_event(EV_WRITE);
    if (e) {
        e->ctx = *ctx;
        e->rip = ctx->rip;
        take_stack(e, (IPTR *)ctx->rsp, NULL);
        nevents++;
    }
    Signal(me, SIGBREAKF_CTRL_F);
    return 1;
}

AROS_UFH3(void, new_FreeMem,
    AROS_UFHA(APTR, mem, A1),
    AROS_UFHA(IPTR, size, D0),
    AROS_UFHA(struct ExecBase *, sysbase, A6))
{
    AROS_USERFUNC_INIT
    if (mem && (IPTR)mem <= watch && watch < (IPTR)mem + size) {
        struct event *e;
        Disable();
        e = new_event(EV_FREE);
        if (e) {
            IPTR *sp = (IPTR *)__builtin_frame_address(0);
            e->caller = (IPTR)__builtin_return_address(0);
            e->mem = (IPTR)mem;
            e->size = size;
            take_stack(e, sp, FindTask(NULL)->tc_SPUpper);
            nevents++;
        }
        Enable();
        Signal(me, SIGBREAKF_CTRL_F);
    }
    AROS_UFC3NR(void, old_FreeMem, AROS_UFCA(APTR, mem, A1), AROS_UFCA(IPTR, size, D0),
        AROS_UFCA(struct ExecBase *, sysbase, A6));
    AROS_USERFUNC_EXIT
}


static BPTR module;

static void decode(BPTR fh, IPTR addr)
{
    STRPTR module = NULL, symbol = NULL;
    IPTR segstart = 0, symstart = 0;
    if (!DebugBase || !addr)
        return;
    if (DecodeLocation((APTR)addr, DL_ModuleName, (IPTR)&module, DL_SegmentStart, (IPTR)&segstart,
                       DL_SymbolName, (IPTR)&symbol, DL_SymbolStart, (IPTR)&symstart, TAG_DONE) && module)
        FPrintf(fh, (CONST_STRPTR)" %s+%lx %s+%lx", (IPTR)module, addr - segstart,
                (IPTR)(symbol ? symbol : (STRPTR)"?"), symbol ? addr - symstart : 0);
}

static void put_bytes(BPTR fh, const UBYTE *b)
{
    int i;
    for (i = 0; i < 16; i++)
        FPrintf(fh, (CONST_STRPTR)" %02lx", (ULONG)b[i]);
    FPrintf(fh, (CONST_STRPTR)"\n");
}

static void write_event(const char *log, struct event *e, int n)
{
    BPTR fh = Open((CONST_STRPTR)log, MODE_READWRITE);
    int k;
    if (!fh)
        return;
    Seek(fh, 0, OFFSET_END);
    if (e->kind == EV_WRITE) {
        FPrintf(fh, (CONST_STRPTR)"event %ld WRITE task %lx (%s) rip %lx", (LONG)n, (IPTR)e->task, (IPTR)e->name, e->rip);
        decode(fh, e->rip);
        FPrintf(fh, (CONST_STRPTR)"\n  rax %lx rbx %lx rcx %lx rdx %lx rsi %lx rdi %lx rbp %lx rsp %lx\n",
                e->ctx.rax, e->ctx.rbx, e->ctx.rcx, e->ctx.rdx, e->ctx.rsi, e->ctx.rdi, e->ctx.rbp, e->ctx.rsp);
        FPrintf(fh, (CONST_STRPTR)"  r8 %lx r9 %lx r10 %lx r11 %lx r12 %lx r13 %lx r14 %lx r15 %lx\n",
                e->ctx.r8, e->ctx.r9, e->ctx.r10, e->ctx.r11, e->ctx.r12, e->ctx.r13, e->ctx.r14, e->ctx.r15);
    } else {
        FPrintf(fh, (CONST_STRPTR)"event %ld FREE task %lx (%s) FreeMem(%lx, %lx) from %lx", (LONG)n, (IPTR)e->task,
                (IPTR)e->name, e->mem, e->size, e->caller);
        decode(fh, e->caller);
        FPrintf(fh, (CONST_STRPTR)"\n");
    }
    FPrintf(fh, (CONST_STRPTR)"  bytes at %lx:", watch);
    put_bytes(fh, e->bytes);
    for (k = 0; k < e->words; k++) {
        STRPTR module = NULL;
        if (DebugBase && DecodeLocation((APTR)e->stack[k], DL_ModuleName, (IPTR)&module, TAG_DONE) && module) {
            FPrintf(fh, (CONST_STRPTR)"  [%ld] %lx", (LONG)k, e->stack[k]);
            decode(fh, e->stack[k]);
            FPrintf(fh, (CONST_STRPTR)"\n");
        }
    }
    Close(fh);
}

/* Start of TASK's loaded segment whose symbol contains SYMBOL, or 0. */

/* debug.library's name and start of the section loaded in the hunk at START.
   A code hunk begins with the loader's jump to the aligned code, which
   debug.library does not count as the segment, so a few offsets are tried. */
static STRPTR seg_info(IPTR start, IPTR *code)
{
    IPTR off;
    for (off = 0; off <= 64; off += 8) {
        STRPTR name = NULL;
        *code = 0;
        if (DecodeLocation((APTR)(start + off), DL_SegmentName, (IPTR)&name, DL_SegmentStart, (IPTR)code, TAG_DONE)
            && name && *code)
            return name;
    }
    return NULL;
}


static IPTR find_segment(LONG cli_number, const char *symbol, LONG *index)
{
    struct Process *p;
    struct CommandLineInterface *cli;
    BPTR seg;
    LONG k = 0, decoded = 0;
    p = FindCliProc(cli_number);
    if (!p || !p->pr_CLI) {
        Printf((CONST_STRPTR)"codewatch: no Shell process %ld\n", cli_number);
        return 0;
    }
    cli = BADDR(p->pr_CLI);
    module = cli->cli_Module;
    if (!module)
        Printf((CONST_STRPTR)"codewatch: Shell process %ld runs no command\n", cli_number);
    for (seg = module; seg && k < 100000; seg = *(BPTR *)BADDR(seg), k++) {
        IPTR code = 0;
        STRPTR name = seg_info((IPTR)BADDR(seg) + sizeof(BPTR), &code);
        if (name) {
            decoded++;
            if (strstr((char *)name, symbol)) {
                *index = k;
                return code;
            }
        }
    }
    Printf((CONST_STRPTR)"codewatch: %ld segments walked, %ld decoded\n", k, decoded);
    return 0;
}


static char obuf[16384];
static size_t olen;

static void oadd(const char *kind, const char *which, struct Task *t, IPTR sp, IPTR lo, IPTR hi, LONG k)
{
    const char *name = t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "?";
    if (olen < sizeof obuf - 200)
        olen += snprintf(obuf + olen, sizeof obuf - olen, "%s %s task %lx '%.40s' sp %lx stack %lx-%lx hunk %ld\n",
                         kind, which, (unsigned long)(IPTR)t, name, (unsigned long)sp, (unsigned long)lo,
                         (unsigned long)hi, (long)k);
}

static void overlap_list(struct List *l, const char *which)
{
    struct Node *n;
    for (n = l->lh_Head; n->ln_Succ; n = n->ln_Succ) {
        struct Task *t = (struct Task *)n;
        IPTR lo = (IPTR)t->tc_SPLower, hi = (IPTR)t->tc_SPUpper, sp = (IPTR)t->tc_SPReg;
        BPTR seg;
        LONG k = 0;
        if (sp && (sp < lo || sp > hi))
            oadd("OUTSIDE", which, t, sp, lo, hi, -1);
        for (seg = module; seg && k < 100000; seg = *(BPTR *)BADDR(seg), k++) {
            IPTR start = (IPTR)BADDR(seg) - sizeof(ULONG);
            IPTR end = start + *(ULONG *)start;
            if (sp >= start && sp < end)
                oadd("SP-IN-HUNK", which, t, sp, lo, hi, k);
            if (lo < end && start < hi)
                oadd("OVERLAP", which, t, sp, lo, hi, k);
        }
    }
}

static int overlap_mode(LONG cli_number, int argc, char **argv)
{
    struct Process *p = FindCliProc(cli_number);
    int i;
    if (!p || !p->pr_CLI || !(module = ((struct CommandLineInterface *)BADDR(p->pr_CLI))->cli_Module)) {
        PutStr((CONST_STRPTR)"codewatch: no command in that Shell process\n");
        return 20;
    }
    /* Printf inside Disable() is not allowed: collect under Forbid, which
       keeps the task lists stable enough for a snapshot of this kind. */
    olen = 0;
    obuf[0] = 0;
    Forbid();
    overlap_list(&SysBase->TaskReady, "ready");
    overlap_list(&SysBase->TaskWait, "wait");
    Permit();
    PutStr((CONST_STRPTR)obuf);
    Printf((CONST_STRPTR)"overlap check done (%ld bytes of findings)\n", (LONG)olen);
    Printf((CONST_STRPTR)"current task %lx stack %lx-%lx\n", (IPTR)FindTask(NULL),
           (IPTR)FindTask(NULL)->tc_SPLower, (IPTR)FindTask(NULL)->tc_SPUpper);
    for (i = 3; i < argc; i++) {
        IPTR a = (IPTR)strtoull(argv[i], NULL, 16);
        STRPTR module_name = NULL, symbol = NULL, segname = NULL;
        IPTR segstart = 0, symstart = 0;
        if (DecodeLocation((APTR)a, DL_ModuleName, (IPTR)&module_name, DL_SegmentName, (IPTR)&segname,
                           DL_SegmentStart, (IPTR)&segstart, DL_SymbolName, (IPTR)&symbol,
                           DL_SymbolStart, (IPTR)&symstart, TAG_DONE) && module_name)
            Printf((CONST_STRPTR)"%lx: %s %s+%lx %s+%lx\n", a, (IPTR)module_name, (IPTR)(segname ? segname : (STRPTR)"?"),
                   a - segstart, (IPTR)(symbol ? symbol : (STRPTR)"?"), symbol ? a - symstart : 0);
        else
            Printf((CONST_STRPTR)"%lx: not decoded\n", a);
    }
    return 0;
}

/* Copies of the command's .text segments. */
#define SNAP_MAX 4096
struct snap { IPTR start; ULONG size; UBYTE *copy; };
static struct snap snaps[SNAP_MAX];
static int nsnaps;

static void take_snapshots(void)
{
    BPTR seg;
    for (seg = module; seg && nsnaps < SNAP_MAX; seg = *(BPTR *)BADDR(seg)) {
        UBYTE *hunk = (UBYTE *)BADDR(seg) - sizeof(ULONG);   /* struct hunk { ULONG size; BPTR next; } */
        ULONG size = *(ULONG *)hunk;
        IPTR start = (IPTR)BADDR(seg) + sizeof(BPTR);
        IPTR code = 0;
        STRPTR segname;
        if (size <= 12 || size > 16 * 1024 * 1024)
            continue;
        segname = seg_info(start, &code);
        if (!segname || strncmp((char *)segname, ".text", 5) != 0)
            continue;
        snaps[nsnaps].start = start;
        snaps[nsnaps].size = size - 12;
        snaps[nsnaps].copy = AllocVec(size - 12, MEMF_ANY);
        if (!snaps[nsnaps].copy)
            break;
        CopyMem((APTR)start, snaps[nsnaps].copy, size - 12);
        nsnaps++;
    }
}

/* First differing segment, or -1. */
static int compare_snapshots(ULONG *offset)
{
    int i;
    for (i = 0; i < nsnaps; i++) {
        if (memcmp((APTR)snaps[i].start, snaps[i].copy, snaps[i].size)) {
            ULONG o = 0;
            while (o < snaps[i].size && ((UBYTE *)snaps[i].start)[o] == snaps[i].copy[o])
                o++;
            *offset = o;
            return i;
        }
    }
    return -1;
}

static void log_difference(const char *log, int i, ULONG o, ULONG ticks)
{
    BPTR fh = Open((CONST_STRPTR)log, MODE_READWRITE);
    ULONG k, end = o + 32 < snaps[i].size ? o + 32 : snaps[i].size;
    if (!fh)
        return;
    Seek(fh, 0, OFFSET_END);
    FPrintf(fh, (CONST_STRPTR)"CHANGED .text segment %ld at %lx (size %lx), offset %lx, after %lu ticks:",
            (LONG)i, snaps[i].start, (IPTR)snaps[i].size, (IPTR)o, ticks);
    decode(fh, snaps[i].start + o);
    FPrintf(fh, (CONST_STRPTR)"\n  was:");
    for (k = o; k < end; k++)
        FPrintf(fh, (CONST_STRPTR)" %02lx", (ULONG)snaps[i].copy[k]);
    FPrintf(fh, (CONST_STRPTR)"\n  now:");
    for (k = o; k < end; k++)
        FPrintf(fh, (CONST_STRPTR)" %02lx", (ULONG)((UBYTE *)snaps[i].start)[k]);
    FPrintf(fh, (CONST_STRPTR)"\n");
    for (k = o & ~7UL; k + 8 <= end; k += 8) {
        IPTR v = *(IPTR *)((UBYTE *)snaps[i].start + k);
        FPrintf(fh, (CONST_STRPTR)"  word +%lx = %lx", (IPTR)k, v);
        decode(fh, v);
        FPrintf(fh, (CONST_STRPTR)"\n");
    }
    Close(fh);
}

int main(int argc, char **argv)
{
    const char *log;
    BPTR fh;
    int done = 0;

    DebugBase = OpenLibrary((CONST_STRPTR)"debug.library", 0);
    if (argc >= 3 && strcmp(argv[1], "OVERLAP") == 0)
        return overlap_mode(atol(argv[2]), argc, argv);
    if (argc == 5 && strcmp(argv[1], "CLI") == 0) {
        LONG index = -1;
        watch = DebugBase ? find_segment(atol(argv[2]), argv[3], &index) : 0;
        Printf((CONST_STRPTR)"codewatch CLI %s %s: segment %ld at %lx\n", (IPTR)argv[2], (IPTR)argv[3], index, watch);
        if (!watch)
            return 20;
        argv[2] = argv[4];
        argc = 3;
    } else if (argc < 2 || argc > 3) {
        PutStr((CONST_STRPTR)"usage: codewatch ADDR [LOG] | codewatch CLI N SYMBOL LOG\n");
        return 20;
    } else
        watch = (IPTR)strtoull(argv[1], NULL, 16) & ~(IPTR)7;
    if (!watch || TypeOfMem((APTR)watch) == 0) {
        PutStr((CONST_STRPTR)"codewatch: address not in memory\n");
        return 20;
    }
    {
        UBYTE b[16];
        int i;
        memcpy(b, (void *)watch, 16);
        Printf((CONST_STRPTR)"codewatch %lx:", watch);
        for (i = 0; i < 16; i++)
            Printf((CONST_STRPTR)" %02lx", (ULONG)b[i]);
        Printf((CONST_STRPTR)"\n");
    }
    if (argc == 2)
        return 0;
    log = argv[2];
    me = FindTask(NULL);
    KernelBase = OpenResource((CONST_STRPTR)"kernel.resource");
    if (!KernelBase || !KrnAddExceptionHandler(1, debug_handler, NULL, NULL)) {
        PutStr((CONST_STRPTR)"codewatch: no debug exception handler\n");
        return 20;
    }
    fh = Open((CONST_STRPTR)log, MODE_NEWFILE);
    if (fh) {
        UBYTE b[16];
        memcpy(b, (void *)watch, 16);
        FPrintf(fh, (CONST_STRPTR)"codewatch %lx, armed; bytes:", watch);
        put_bytes(fh, b);
        Close(fh);
    }
    Forbid();
    old_FreeMem = SetFunction((struct Library *)SysBase, -35 * LIB_VECTSIZE, (APTR)AROS_ASMSYMNAME(new_FreeMem));
    Permit();
    dr_value0 = watch;
    dr_value1 = watch + 8;
    /* L0, L1; each write-only over 8 bytes */
    dr_value7 = 3UL | (0xDUL << 16) | (0xDUL << 20);
    Supervisor((ULONG_FUNC)dr_load);

    if (module) {
        take_snapshots();
    }
    fh = Open((CONST_STRPTR)log, MODE_READWRITE);
    if (fh) {
        Seek(fh, 0, OFFSET_END);
        FPrintf(fh, (CONST_STRPTR)"%ld .text segments copied\n", (LONG)nsnaps);
        Close(fh);
    }
    {
        ULONG ticks = 0;
        int reported = 0;
        for (;;) {
            Delay(2);
            ticks += 2;
            SetSignal(0, SIGBREAKF_CTRL_F);
            while (done < nevents) {
                write_event(log, &events[done], done);
                done++;
            }
            if (!reported && nsnaps) {
                ULONG o;
                int i = compare_snapshots(&o);
                if (i >= 0) {
                    log_difference(log, i, o, ticks);
                    reported = 1;
                    /* The same place is likely written again (a stack frame):
                       watch it to name the writer. */
                    watch = (snaps[i].start + o) & ~(IPTR)7;
                    dr_value0 = watch;
                    dr_value1 = watch + 8;
                    dr_value7 = 3UL | (0xDUL << 16) | (0xDUL << 20);
                    Supervisor((ULONG_FUNC)dr_load);
                    fh = Open((CONST_STRPTR)log, MODE_READWRITE);
                    if (fh) {
                        Seek(fh, 0, OFFSET_END);
                        FPrintf(fh, (CONST_STRPTR)"watch moved to %lx\n", watch);
                        Close(fh);
                    }
                }
            }
        }
    }
    return 0;
}
