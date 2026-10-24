/* Separate DOS executable: deliberately uses Read/Write, not terminal internals.
 * A diagnostic peer, not a shell or a replacement line discipline. */
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>
#include "tui_menu.h"
#include "flood_peer.h"
static int emit(const char *s)
{ size_t n=strlen(s); return Write(Output(),s,n)==(LONG)n; }
int main(int argc,char **argv)
{
    unsigned char c;
    char line[512]; size_t length=0; int skip_lf=0;
    if(argc==2 && !strcmp(argv[1],"--flood-check")) return flood_peer();
    if(argc==2 && !strcmp(argv[1],"--tui")) {
        TuiMenu menu={0,0};
        char frame[1024]; size_t n; LONG read_result=0; int failed=0,changed=1;
        if(!IsInteractive(Input()) || !IsInteractive(Output()) || !SetMode(Input(),1)) return 20;
        if(!emit(tui_menu_enter)) failed=1;
        while(!failed) {
            if(changed) {
                n=tui_menu_draw(&menu,frame,sizeof frame);
                if(!n || Write(Output(),frame,n)!=(LONG)n) { failed=1; break; }
            }
            read_result=Read(Input(),&c,1);
            if(read_result!=1) break;
            changed=tui_menu_input(&menu,c);
            if(changed<0) break;
        }
        if(!emit(tui_menu_leave)) failed=1;
        if(!SetMode(Input(),0)) failed=1;
        snprintf(frame,sizeof frame,"TUI demo ended (selection %u)\n",menu.selected+1);
        if(!emit(frame)) failed=1;
        return failed || read_result<0?20:0;
    }
    if(argc==2 && !strcmp(argv[1],"--console-check")) {
        char b[16];
        BPTR reopened;
        if(!IsInteractive(Input()) || !IsInteractive(Output())) return 20;
        reopened=Open((CONST_STRPTR)"*",MODE_NEWFILE);
        if(!reopened) return 20;
        if(Write(reopened,"STAR OUTPUT\n",12)!=12) { Close(reopened); return 20; }
        Close(reopened);
        reopened=Open((CONST_STRPTR)"*",MODE_READWRITE);
        if(!reopened) return 20;
        Close(reopened);
        emit("COOKED READY\r\n");
        if(Read(Input(),b,sizeof b)!=4 || memcmp(b,"abD\n",4)) return 20;
        if(!SetMode(Input(),1)) return 20;
        emit("RAW READY\r\n");
        if(Read(Input(),b,sizeof b)!=3 || memcmp(b,"\033[A",3)) return 20;
        SetSignal(0,SIGBREAKF_CTRL_C);
        emit("RAW CTRL-C READY\r\n");
        if(Read(Input(),b,sizeof b)!=1 || b[0]!=3 ||
           (SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C)) return 20;
        if(!SetMode(Input(),0)) return 20;
        emit("CONSOLE PASS\r\n"); return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--wait-check")) {
        LONG error;
        struct DateStamp before,after;
        LONG ticks;
        if(Seek(Input(),0,OFFSET_CURRENT)!=-1 || IoErr()!=ERROR_SEEK_ERROR) {
            emit("SEEK REJECTION FAILED\r\n"); return 20;
        }
        if(WaitForChar(Input(),0)) return 20;
        error=IoErr();
        if(error) { emit("WAIT UNSUPPORTED\r\n"); return 20; }
        if(!emit("WAIT TIMEOUT\r\n")) return 20;
        DateStamp(&before);
        if(WaitForChar(Input(),100000) || IoErr()) return 20;
        DateStamp(&after);
        ticks=(after.ds_Days-before.ds_Days)*24*60*60*TICKS_PER_SECOND+
            (after.ds_Minute-before.ds_Minute)*60*TICKS_PER_SECOND+
            after.ds_Tick-before.ds_Tick;
        /* Tolerate coarse timestamps, but reject an immediate false reply. */
        if(ticks<2) { emit("WAIT RETURNED TOO EARLY\r\n"); return 20; }
        if(!emit("WAIT INPUT\r\n")) return 20;
        if(!WaitForChar(Input(),5000000) || IoErr()) return 20;
        if(!WaitForChar(Input(),0) || Read(Input(),&c,1)!=1 || c!='X') return 20;
        if(!emit("WAIT EOF\r\n")) return 20;
        if(!WaitForChar(Input(),5000000) || IoErr()) return 20;
        if(Read(Input(),&c,1)!=0) return 20;
        emit("WAIT PASS\r\n"); return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lock-exit")) {
        /* Integration fault injection: deliberately abandon an endpoint lease
         * to test reclamation after this foreground process exits. */
        if(!DupLockFromFH(Input())) return 20;
        emit("LOCK LEASE\r\n"); return 0;
    }
    if(!emit("\033[1;36mExternal DOS session peer\033[0m\r\n"
             "Commands: help unicode ansi burst exit | F5: input EOF\r\npeer> ")) return 20;
    while(Read(Input(),&c,1)==1) {
        if(c=='\n' && skip_lf) { skip_lf=0; continue; }
        skip_lf=0;
        if(c=='\r' || c=='\n') {
            unsigned i;
            skip_lf=(c=='\r'); line[length]=0;
            if(!emit("\r\n")) return 20;
            if(!strcmp(line,"exit")) { emit("Bye\r\n"); return 0; }
            if(!strcmp(line,"unicode")) {
                if(!emit("Zażółć 中文 日本語 한글 😀 👨‍👩‍👧‍👦 🇵🇱 é\r\n")) return 20;
            } else if(!strcmp(line,"ansi")) {
                if(!emit("\033[31mRed \033[32mGreen \033[34mBlue\033[0m\r\n"
                         "stale suffix\rupdated\033[K\r\n")) return 20;
            } else if(!strcmp(line,"burst")) {
                for(i=0;i<256;++i) {
                    char row[100];
                    snprintf(row,sizeof row,"%03u: external process output under backpressure -- 0123456789\r\n",i);
                    if(!emit(row)) return 20;
                }
                if(!emit("BURST END\r\n")) return 20;
            } else if(!strcmp(line,"help")) {
                if(!emit("help | unicode | ansi | burst | exit\r\n")) return 20;
            } else if(length) {
                if(!emit("Received: ") || !emit(line) || !emit("\r\n")) return 20;
            }
            length=0;
            if(!emit("peer> ")) return 20;
        } else if(c==3) {
            length=0; if(!emit("^C\r\npeer> ")) return 20;
        } else if(c==8 || c==127) {
            /* Diagnostic editing is ASCII-only; don't pretend this is a
             * grapheme-aware shell editor. Never split a non-ASCII suffix. */
            if(length && (unsigned char)line[length-1]<128) {
                --length; if(!emit("\b \b")) return 20;
            }
        } else if(c>=32 && length<sizeof line-1) {
            line[length++]=c;
            if(Write(Output(),&c,1)!=1) return 20;
        }
    }
    emit("\r\nInput EOF\r\n");
    return 0;
}
