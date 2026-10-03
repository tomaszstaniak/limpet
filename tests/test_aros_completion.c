#include "../src/aros_completion.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/rdargs.h>
#include <dos/var.h>
#include <stdio.h>
#include <string.h>
static XCompletion c;
static XLineEditor e;
static int has(const char *name)
{ size_t i; for(i=0;i<c.count;i++) if(!strcmp(c.names[i],name)) return 1; return 0; }
static int prepare(const char *text)
{
    memset(&e,0,sizeof e);
    return line_editor_insert(&e,text,strlen(text)) && completion_prepare(&c,&e);
}
static int collect(const char *text,struct Process *p)
{
    struct Process *me=(struct Process *)FindTask(NULL);
    BPTR cwd=me->pr_CurrentDir,home=me->pr_HomeDir;
    APTR window=me->pr_WindowPtr;
    int result;
    if(!prepare(text)) return 0;
    result=aros_completion_collect(&c,p);
    printf("collect [%s]: result=%d matches=%lu\n",text,result,(unsigned long)c.count);
    return result && me->pr_CurrentDir==cwd && me->pr_HomeDir==home && me->pr_WindowPtr==window;
}
#define CHECK(expr) do { if(!(expr)) { printf("FAIL line %d: %s (IoErr=%ld)\n",__LINE__,#expr,(long)IoErr()); goto cleanup; } } while(0)
int main(void)
{
    struct Process *me=(struct Process *)FindTask(NULL);
    struct CommandLineInterface *cli=Cli();
    char dir[128],assign[80],command[200],alias[80];
    BPTR root=BNULL,old=me->pr_CurrentDir,file,path[2],oldpath=cli?cli->cli_CommandDir:BNULL;
    unsigned attempt;
    int created=0,myfile=0,myfolder=0,second=0,assigned=0,aliased=0,result=20;
    assign[0]=0;
    for(attempt=0;attempt<100;attempt++) {
        snprintf(dir,sizeof dir,"RAM:xterm-completion-%lu-%u",(unsigned long)me->pr_TaskNum,attempt);
        root=CreateDir((CONST_STRPTR)dir);
        if(root) { created=1; break; }
        CHECK(IoErr()==ERROR_OBJECT_EXISTS || IoErr()==ERROR_OBJECT_IN_USE);
    }
    CHECK(root);
    /* CreateDir returns an exclusive lock, unsuitable for independent reads. */
    UnLock(root); root=Lock((CONST_STRPTR)dir,SHARED_LOCK); CHECK(root);
    CurrentDir(root);
    file=Open((CONST_STRPTR)"My File",MODE_NEWFILE); CHECK(file); myfile=1; Close(file);
    file=CreateDir((CONST_STRPTR)"My Folder"); CHECK(file); myfolder=1; UnLock(file);
    CHECK(collect("Type My",me)); CHECK(has("My File") && has("My Folder/"));
    CHECK(completion_apply(&c,&e,0)); CHECK(!strcmp(e.text,"Type \"My File\" "));
    snprintf(command,sizeof command,"Type %s/My",dir);
    CHECK(collect(command,me)); CHECK(has("My File") && has("My Folder/"));
    /* ':' means this reader's volume root, not the GUI/boot volume. */
    snprintf(command,sizeof command,"Type %s/My",strchr(dir,':'));
    CHECK(collect(command,me)); CHECK(has("My File") && has("My Folder/"));
    /* Isolate the codec from provider matches such as the FONTS: assign. */
    { const char *names[]={"foo=bar","a*\"b","日本😀","My File"};
      size_t i;
      for(i=0;i<sizeof names/sizeof names[0];i++) {
          struct CSource source; char decoded[128]; LONG item;
          CHECK(prepare("Type ")); CHECK(completion_add(&c,names[i],0));
          CHECK(completion_apply(&c,&e,0));
          memset(&source,0,sizeof source); source.CS_Buffer=(UBYTE *)e.text+5;
          source.CS_Length=strlen(e.text+5);
          item=ReadItem((STRPTR)decoded,sizeof decoded,&source);
          CHECK(item==ITEM_QUOTED || item==ITEM_UNQUOTED);
          CHECK(!strcmp(decoded,names[i]));
      }
    }
    CHECK(collect("CD ra",me)); CHECK(has("RAM:"));
    CHECK(collect("ec",me)); CHECK(has("Echo"));
    CHECK(collect("Type C:Ec",me)); CHECK(has("Echo"));
    CHECK(cli);
    path[0]=oldpath; path[1]=root; cli->cli_CommandDir=MKBADDR(path);
    CurrentDir(old);
    CHECK(collect("My F",me)); CHECK(!has("My File"));
    CHECK(collect("\"My F",me)); CHECK(has("My File"));
    cli->cli_CommandDir=oldpath; CurrentDir(root);
    /* Use a private assign; never replace an existing name. */
    snprintf(assign,sizeof assign,"XTComp-%lu-%u",(unsigned long)me->pr_TaskNum,attempt);
    { ULONG flags=LDF_READ|LDF_ASSIGNS|LDF_DEVICES|LDF_VOLUMES;
      struct DosList *list=LockDosList(flags);
      int exists=FindDosEntry(list,(CONST_STRPTR)assign,flags)!=NULL;
      UnLockDosList(flags); CHECK(!exists); }
    file=DupLock(root); CHECK(file);
    if(!AssignLock((CONST_STRPTR)assign,file)) { UnLock(file); CHECK(0); }
    assigned=1;
    file=Open((CONST_STRPTR)"My Folder/Second File",MODE_NEWFILE); CHECK(file);
    second=1; Close(file);
    file=Lock((CONST_STRPTR)"My Folder",SHARED_LOCK); CHECK(file);
    if(!AssignAdd((CONST_STRPTR)assign,file)) { UnLock(file); CHECK(0); }
    snprintf(command,sizeof command,"Type %s:",assign);
    CHECK(collect(command,me)); CHECK(has("My File") && has("Second File"));
    snprintf(alias,sizeof alias,"XTCompAlias%lu_%u",(unsigned long)me->pr_TaskNum,attempt);
    CHECK(!FindVar((CONST_STRPTR)alias,LV_ALIAS));
    CHECK(SetVar((CONST_STRPTR)alias,(CONST_STRPTR)"Echo alias",-1,GVF_LOCAL_ONLY|LV_ALIAS));
    aliased=1;
    CHECK(collect(alias,me)); CHECK(has(alias));
    snprintf(command,sizeof command,"Type %s",alias);
    CHECK(collect(command,me)); CHECK(!has(alias));
    snprintf(command,sizeof command,"RAM:%s",alias);
    CHECK(collect(command,me)); CHECK(!has(alias));
    result=0;
cleanup:
    /* Failed checks must not leave the invoking Shell in a locked test
     * directory or pointing at this test's stack-based Path entry. */
    if(cli) cli->cli_CommandDir=oldpath;
    if(aliased && !DeleteVar((CONST_STRPTR)alias,GVF_LOCAL_ONLY|LV_ALIAS)) result=20;
    if(assigned && !AssignLock((CONST_STRPTR)assign,BNULL)) result=20;
    if(root) {
        CurrentDir(root);
        if(second && !DeleteFile((CONST_STRPTR)"My Folder/Second File")) result=20;
        if(myfile && !DeleteFile((CONST_STRPTR)"My File")) result=20;
        if(myfolder && !DeleteFile((CONST_STRPTR)"My Folder")) result=20;
        CurrentDir(old); UnLock(root);
    } else CurrentDir(old);
    if(created && !DeleteFile((CONST_STRPTR)dir)) result=20;
    if(!result) puts("AROS completion: PASS (cwd, roots, quoting, Path, multiassign, state restoration)");
    return result;
}
