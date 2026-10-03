/* Query the inherited Output() without reopening the console or reading input. */
#include "../src/aros_tty_client.h"
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    XtyControl c;LONG error;
    memset(&c,0,sizeof c);c.version=XTY_CONTROL_VERSION;c.operation=XTY_GET_PRESENTATION;
    if(!IsInteractive(Output())) { puts("PRESENTATION plain: noninteractive output");return 0; }
    if(!aros_tty_control(Output(),&c)) {
        error=IoErr();printf("PRESENTATION plain: query error=%ld\n",(long)error);return 5;
    }
    printf("PRESENTATION known=%lu colors=%lu styles=%lu lines=%lu rows=%lu columns=%lu interpretation=%lu\n",
           (unsigned long)c.values[0],(unsigned long)c.values[1],(unsigned long)c.values[2],
           (unsigned long)c.values[3],(unsigned long)c.values[4],(unsigned long)c.values[5],
           (unsigned long)c.values[6]);
    return 0;
}
