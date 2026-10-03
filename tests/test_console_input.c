#include "../src/console_input.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static XConsoleInput line;
static XSessionQueue input,echo;
static void send(const char *s) {
    while(*s) assert(console_input_byte(&line,&input,&echo,(unsigned char)*s++));
}
static void reset(void) {
    memset(&line,0,sizeof line); memset(&input,0,sizeof input); memset(&echo,0,sizeof echo);
}
int main(void) {
    unsigned char bytes[64];
    reset(); send("abc"); assert(!input.length);
    send("\bD\r\n");
    assert(session_queue_read(&input,bytes,sizeof bytes)==4);
    assert(!memcmp(bytes,"abD\n",4));
    assert(session_queue_read(&echo,bytes,sizeof bytes)==9);
    assert(!memcmp(bytes,"abc\b \bD\r\n",9));
    reset(); send("x\033[A\033[D\r");
    assert(session_queue_read(&input,bytes,sizeof bytes)==2 && !memcmp(bytes,"x\n",2));
    reset(); send("é\b\r");
    assert(session_queue_read(&input,bytes,sizeof bytes)==3 && !memcmp(bytes,"é\n",3));
    reset(); send("x"); assert(console_input_mode(&line,1)==-1);
    send("\r"); assert(console_input_mode(&line,1)==0);
    send("\033[A\r\b");
    assert(session_queue_read(&input,bytes,sizeof bytes)==7 && !memcmp(bytes,"x\n\033[A\r\b",7));
    assert(console_input_mode(&line,2)==-1);
    reset(); send("ab"); input.length=XSESSION_CAPACITY-1;
    assert(!console_input_byte(&line,&input,&echo,'\r') && line.length==2);
    input.length=0; echo.length=XSESSION_CAPACITY;
    assert(!console_input_byte(&line,&input,&echo,'c') && line.length==2);
    echo.length=0; send("\r");
    assert(session_queue_read(&input,bytes,sizeof bytes)==3 && !memcmp(bytes,"ab\n",3));
    reset();
    { size_t i; for(i=0;i<XCONSOLE_LINE_CAPACITY-1;++i) send("a"); }
    assert(!console_input_byte(&line,&input,&echo,'b'));
    send("\r"); assert(input.length==XCONSOLE_LINE_CAPACITY);
    reset();
    assert(console_input_stage(&line,"Echo 中",strlen("Echo 中")));
    assert(!input.length && !echo.length);
    assert(console_input_mode(&line,1)==-1);
    input.length=XSESSION_CAPACITY-1;
    assert(!console_input_submit(&line,&input));
    assert(line.length==strlen("Echo 中"));
    input.length=0;
    assert(console_input_submit(&line,&input));
    assert(!line.length && !echo.length);
    assert(session_queue_read(&input,bytes,sizeof bytes)==strlen("Echo 中\n"));
    assert(!memcmp(bytes,"Echo 中\n",strlen("Echo 中\n")));
    assert(console_input_mode(&line,1)==0);
    assert(!console_input_stage(&line,"x",1));
    assert(!console_input_submit(&line,&input));
    puts("console input PASS: line commit, CRLF, erase, UTF-8 preservation, raw, bounds, backpressure");
    return 0;
}
