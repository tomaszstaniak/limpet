#ifndef AROS_XTERM_HYPERLINK_JOB_H
#define AROS_XTERM_HYPERLINK_JOB_H
#include <exec/types.h>
typedef struct {
    ULONG task_id;
    int result; /* 0 accepted, -1 no library, -2 refused/failed, -3 no such file */
    int file;   /* a file:/// link, opened through Workbench, not OpenURL */
    char url[2048];
} ArosHyperlinkJob;
int aros_hyperlink_start(ArosHyperlinkJob *,const char *);
int aros_hyperlink_poll(ArosHyperlinkJob *);
#endif
