/* Standalone DOS client: deliberately no private TTY headers or objects. */
#include <proto/dos.h>
#include <string.h>
#ifdef XTY_CONTROL_PROBE
#include "../src/aros_tty_client.h"
#endif
#define SAY(s) do { if(Write(Output(),s,sizeof(s)-1)!=(LONG)(sizeof(s)-1)) return 20; } while(0)
#define REQUIRE(e) do { if(!(e)) { SAY("PEER_FAIL\n"); return 20; } } while(0)
int main(void)
{
    unsigned char b[16];
#ifdef XTY_CONTROL_PROBE
    {
        XtyControl c;
        memset(&c,0,sizeof c);c.version=1;c.operation=XTY_CAPS;
        REQUIRE(aros_tty_control(Input(),&c)==DOSTRUE && (c.values[0]&15)==15);
        c.operation=XTY_GET_GEOMETRY;
        REQUIRE(aros_tty_control(Output(),&c)==DOSTRUE);
        REQUIRE(c.values[0]==31 && c.values[1]==109);
        memset(&c,0,sizeof c);c.version=1;c.operation=XTY_SET_GEOMETRY;
        c.values[0]=32;c.values[1]=110;c.values[2]=880;c.values[3]=544;
        REQUIRE(aros_tty_control(Input(),&c)==DOSTRUE);
        c.operation=XTY_GET_GEOMETRY;
        REQUIRE(aros_tty_control(Output(),&c)==DOSTRUE && c.values[0]==32 && c.values[1]==110);
        memset(&c,0,sizeof c);c.version=1;c.operation=XTY_SET_CONFIG;c.values[3]=4;
        REQUIRE(aros_tty_control(Input(),&c)==DOSTRUE);
        c.operation=XTY_GET_CONFIG;
        REQUIRE(aros_tty_control(Output(),&c)==DOSTRUE && c.values[2]==0 && c.values[3]==4);
    }
#endif
    REQUIRE(IsInteractive(Input()) && IsInteractive(Output()));
    REQUIRE(SetMode(Input(),1)==DOSTRUE);
    REQUIRE(WaitForChar(Input(),100000)==DOSFALSE && IoErr()==0);
    SAY("RAW_READY\n");
    REQUIRE(WaitForChar(Input(),5000000)==DOSTRUE);
    REQUIRE(Read(Input(),b,3)==3 && !memcmp(b,"A\0B",3));
    REQUIRE(SetMode(Input(),0)==DOSTRUE);
    SAY("COOKED_READY\n");
    REQUIRE(WaitForChar(Input(),5000000)==DOSTRUE);
    REQUIRE(Read(Input(),b,sizeof b)==4 && !memcmp(b,"one\n",4));
    SAY("EOF_READY\n");
    REQUIRE(Read(Input(),b,sizeof b)==0);
    SAY("RESTART_READY\n");
    REQUIRE(WaitForChar(Input(),5000000)==DOSTRUE);
    REQUIRE(Read(Input(),b,sizeof b)==4 && !memcmp(b,"two\n",4));
    REQUIRE(SetMode(Input(),1)==DOSTRUE);
    SAY("PEER_PASS\n");
    return 0;
}
