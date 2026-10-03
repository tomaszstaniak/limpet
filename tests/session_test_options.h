#ifndef XTERM_SESSION_TEST_OPTIONS_H
#define XTERM_SESSION_TEST_OPTIONS_H
#include <string.h>
typedef struct { unsigned repeat; int require_break; } SessionTestOptions;
static int session_test_options(int argc,const char *const *argv,SessionTestOptions *out)
{
    SessionTestOptions parsed={1,0};
    int i,seen_repeat=0;
    if(argc<1 || !argv || !out) return 0;
    for(i=1;i<argc;++i) {
        if(!argv[i]) return 0;
        if(!strcmp(argv[i],"--require-break")) {
            if(parsed.require_break) return 0;
            parsed.require_break=1;
        } else if(!strcmp(argv[i],"--repeat")) {
            const unsigned char *p; unsigned value=0;
            if(seen_repeat || ++i>=argc || !argv[i] || !*argv[i]) return 0;
            seen_repeat=1;
            p=(const unsigned char *)argv[i];
            for(;*p;++p) {
                if(*p<'0' || *p>'9' || value>(1000u-(*p-'0'))/10u) return 0;
                value=value*10u+(*p-'0');
            }
            if(!value) return 0;
            parsed.repeat=value;
        } else return 0;
    }
    *out=parsed; return 1;
}
#endif
