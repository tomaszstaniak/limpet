#ifndef LIMPET_CON_H
#define LIMPET_CON_H
/* What LimpetCON leaves for the handler's launcher (tools/xterm_handler.c):
 * the system console handler that the CON device node held before Limpet's
 * launcher took its place. A public port carries it, so that the launcher,
 * which has no globals, finds it by name. The port and this memory are
 * never removed or freed once made: a launcher may be reading them, and
 * LimpetCON ON uses them again. Read and write the fields under Forbid. */
#include <exec/ports.h>
#include <dos/dos.h>
#define LIMPETCON_PORT "LimpetCON.state"
#define LIMPETCON_MAGIC 0x4c434f4eUL
typedef struct {
    struct MsgPort port;
    ULONG magic;
    LONG active;          /* CON: starts Limpet's launcher now */
    BPTR limpet_seg;      /* the launcher's segment, loaded once and kept */
    BSTR limpet_handler;  /* its path, as the node's dn_Handler */
    /* The CON node's fields before the swap. seglist is resolved (never
       0 while active): the launcher starts the system console from it. */
    BSTR handler;
    BPTR seglist;
    ULONG stack;
    LONG priority;
    BPTR globvec;
    char name[sizeof LIMPETCON_PORT];
} LimpetConState;
#endif
