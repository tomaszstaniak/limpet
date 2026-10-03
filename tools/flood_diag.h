#ifndef XTERM_FLOOD_DIAG_H
#define XTERM_FLOOD_DIAG_H
/* Compile-time hooks: ordinary binaries do not reference this diagnostic. */
#ifdef XTERM_FLOOD_DIAG
static int flood_diag_start(struct Window *);
static ULONG flood_diag_signals(void);
static int flood_diag_ready(void);
static void flood_diag_output(const unsigned char *,size_t);
static void flood_diag_queues(void);
static int flood_diag_step(struct Window *);
static void flood_diag_upload(struct Window *,unsigned,int);
static void flood_diag_event(ULONG);
#else
#define flood_diag_start(w) 1
#define flood_diag_signals() 0
#define flood_diag_ready() 0
#define flood_diag_output(b,n) ((void)0)
#define flood_diag_queues() ((void)0)
#define flood_diag_step(w) 0
#define flood_diag_upload(w,r,ok) ((void)0)
#define flood_diag_event(e) ((void)0)
#endif
#endif
