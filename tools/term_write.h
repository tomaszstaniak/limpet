/* Output helpers shared by the Kitty demo producers.

   write_all() retries the unwritten suffix: a DOS Write() may accept fewer
   bytes than asked (a cancelled or interrupted request reports the prefix
   it sent), and resending the whole buffer would repeat that prefix. Zero
   or an error ends the program's output: the terminal has gone or broken
   the request, and term_failed says so. term_bytes counts what was accepted,
   not what was attempted. */
#ifndef AROS_XTERM_TERM_WRITE_H
#define AROS_XTERM_TERM_WRITE_H
#ifndef HOST_DOS_SHIM
#include <proto/dos.h>
#endif
#include <string.h>

static unsigned long long term_bytes;
static int term_failed;
static unsigned long term_writes;

static int write_all(const void *data,size_t length)
{
    const char *p=data;
    if(term_failed) return 0;
    while(length) {
        LONG n=Write(Output(),(APTR)p,(LONG)length);
        ++term_writes;
        if(n<=0) { term_failed=1;return 0; }
        term_bytes+=(unsigned long long)n;p+=n;length-=(size_t)n;
    }
    return 1;
}
static int write_text(const char *text) { return write_all(text,strlen(text)); }

/* Append one Kitty APC sequence (ESC _ G keys ; payload ESC \) to out and
   return its length. The caller sizes out for keys, payload and 5 bytes. */
static size_t kitty_sequence(char *out,const char *keys,const char *payload,size_t length)
{
    size_t k=strlen(keys);
    out[0]='\033';out[1]='_';out[2]='G';memcpy(out+3,keys,k);out[3+k]=';';
    memcpy(out+4+k,payload,length);
    out[4+k+length]='\033';out[5+k+length]='\\';
    return 6+k+length;
}
#endif
