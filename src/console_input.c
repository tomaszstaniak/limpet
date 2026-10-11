#include "console_input.h"
#include <string.h>
int console_input_stage(XConsoleInput *s,const char *p,size_t n)
{
    if(s->raw || n>=XCONSOLE_LINE_CAPACITY || (n && !p)) return 0;
    if(n) memmove(s->line,p,n);
    s->length=n; s->skip_lf=s->escape=0;
    return 1;
}
int console_input_submit(XConsoleInput *s,XSessionQueue *input)
{
    if(s->raw || XSESSION_CAPACITY-input->length<s->length+1) return 0;
    session_queue_write(input,s->line,s->length);
    session_queue_write(input,(const unsigned char *)"\n",1);
    s->length=0; s->skip_lf=s->escape=0;
    return 1;
}
int console_input_mode(XConsoleInput *s,int raw)
{
    if((raw!=0 && raw!=1) || (s->length && raw!=s->raw)) return -1;
    s->raw=raw; s->skip_lf=s->escape=0; return 0;
}
int console_input_byte(XConsoleInput *s,XSessionQueue *input,XSessionQueue *echo,unsigned char c)
{
    if(s->raw) return session_queue_write(input,&c,1)==1;
    if(s->escape) {
        if(s->escape==1) s->escape=(c=='[' || c=='O')?2:0;
        else if(c>=0x40 && c<=0x7e) s->escape=0;
        return 1;
    }
    if(c==27) { s->escape=1; return 1; }
    if(c=='\n' && s->skip_lf) { s->skip_lf=0; return 1; }
    if(c=='\r' || c=='\n') {
        if(XSESSION_CAPACITY-input->length<s->length+1 ||
           XSESSION_CAPACITY-echo->length<2) return 0;
        s->line[s->length]='\n';
        session_queue_write(input,s->line,s->length+1);
        session_queue_write(echo,(const unsigned char *)"\r\n",2);
        s->length=0; s->skip_lf=(c=='\r'); return 1;
    }
    if(c==8 || c==127) {
        if(s->length && s->line[s->length-1]<128) {
            if(XSESSION_CAPACITY-echo->length<3) return 0;
            --s->length;
            session_queue_write(echo,(const unsigned char *)"\b \b",3);
        }
        s->skip_lf=0; return 1;
    }
    if(c<32) { s->skip_lf=0; return 1; }
    if(s->length>=XCONSOLE_LINE_CAPACITY-1 || echo->length==XSESSION_CAPACITY) return 0;
    s->line[s->length++]=c; session_queue_write(echo,&c,1);
    s->skip_lf=0; return 1;
}
int console_interrupt_needs_line(int read_pending,int foreground_known,int command_running)
{
    return read_pending || !foreground_known || !command_running;
}
