#include "aros_hyperlink.h"
#include "hyperlink.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <libraries/openurl.h>
#include <defines/openurl.h>
#include <string.h>
#include <proto/workbench.h>

static void worker(void)
{
    ArosHyperlinkJob *j=(ArosHyperlinkJob *)FindTask(NULL)->tc_UserData;
    struct Library *OpenURLBase;
    struct TagItem tags[]={{URL_Launch,TRUE},{TAG_DONE,0}};
    Wait(SIGBREAKF_CTRL_F);
    {
        char path[XHYPERLINK_LIMIT];
        if(hyperlink_file_path(j->url,path,sizeof path)) {
            struct Library *WorkbenchBase;
            /* workbench.library queues the start and reports a failed one
               to nobody (handler.c "report error somehow?"), so a missing
               file is caught here. NP_WindowPtr -1: no requester. */
            BPTR lock=Lock((CONST_STRPTR)path,ACCESS_READ);
            if(lock) UnLock(lock);
            else {
                LONG error=IoErr();
                /* A file that exists but is in use is still Workbench's to try. */
                if(error==ERROR_OBJECT_NOT_FOUND || error==ERROR_DIR_NOT_FOUND ||
                   error==ERROR_DEVICE_NOT_MOUNTED) { j->result=-3; return; }
            }
            WorkbenchBase=OpenLibrary((CONST_STRPTR)"workbench.library",0);
            j->result=-1;
            if(WorkbenchBase) {
                struct TagItem local_tags[]={{TAG_DONE,0}};
                j->result=OpenWorkbenchObjectA((STRPTR)path,local_tags)?0:-2;
                CloseLibrary(WorkbenchBase);
            }
            return;
        }
    }
    OpenURLBase=OpenLibrary((CONST_STRPTR)"openurl.library",0);
    j->result=-1;
    if(OpenURLBase) {
        j->result=URL_OpenA((STRPTR)j->url,tags)?0:-2;
        CloseLibrary(OpenURLBase);
    }
}
int aros_hyperlink_start(ArosHyperlinkJob *j,const char *url)
{
    struct Process *child;
    if(j->task_id || !hyperlink_open_safe(url)) return -1;
    memcpy(j->url,url,strlen(url)+1);
    j->file=!strncmp(url,"file:",5);
    j->result=-2;
    child=CreateNewProcTags(NP_Entry,(IPTR)worker,NP_UserData,(IPTR)j,
        NP_Name,(IPTR)"AROS XTerm open link",NP_StackSize,65536,
        NP_NotifyOnDeath,TRUE,NP_Input,0,NP_Output,0,NP_Error,0,
        NP_WindowPtr,(IPTR)-1,
#ifdef XTERM_PROCESS_PORT_CLIENTS
        /* Explicit dirs: by default CreateNewProc calls Lock("") and
         * DupLock(pr_HomeDir) in the parent, which are synchronous DOS
         * calls on its process port (ABIv11 rom/dos/createnewproc.c). */
        NP_CurrentDir,0,NP_HomeDir,0,
#endif
        TAG_DONE);
    if(!child) return -1;
    j->task_id=GetETaskID(&child->pr_Task);
    Signal(&child->pr_Task,SIGBREAKF_CTRL_F);
    return 0;
}
int aros_hyperlink_poll(ArosHyperlinkJob *j)
{
    if(!j->task_id || ChildStatus(j->task_id)!=CHILD_EXITED) return 0;
    ChildFree(j->task_id); j->task_id=0;
    return 1;
}
