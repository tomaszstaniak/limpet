/* Print the current DateStamp as "days minutes ticks" (1 tick = 1/50 s).
 * C:Date shows whole seconds only; window-open timings under QEMU TCG need
 * finer steps. Usage: datestamp_tick >RESULTS:<new name> */
#include <proto/dos.h>
#include <stdio.h>

int main(void)
{
    struct DateStamp ds;
    DateStamp(&ds);
    printf("%ld %ld %ld\n", (long)ds.ds_Days, (long)ds.ds_Minute, (long)ds.ds_Tick);
    return 0;
}
