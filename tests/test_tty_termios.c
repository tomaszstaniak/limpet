#include "../src/tty_termios.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static XtyControl snapshot(void)
{
    XtyControl c;memset(&c,0,sizeof c);c.version=1;c.operation=XTY_GET_CONFIG;
    c.reserved[0]=XTY_PACKET_MAGIC;c.values[3]=4;return c;
}
static void rejected(const struct termios *t,int action,int error)
{
    XtyControl out,before;memset(&out,0xa5,sizeof out);before=out;
    assert(xty_termios_encode(t,action,&out)==error);
    assert(!memcmp(&out,&before,sizeof out));
}
static void input_signal_codec(void)
{
    XtyControl c;struct termios t,decoded;unsigned value;
    memset(&c,0,sizeof c);c.version=1;c.operation=XTY_GET_CONFIG;
    c.reserved[0]=XTY_PACKET_MAGIC;c.values[3]=4;
    assert(!xty_termios_decode(&c,&t));
    t.c_lflag=ISIG|NOFLSH;
    for(value=0;value<256;++value) {
        t.c_cc[VINTR]=(cc_t)value;
        assert(!xty_termios_encode(&t,TCSANOW,&c));
        assert(c.values[2]==192 && c.values[9]==(value==(unsigned char)_POSIX_VDISABLE?0:value+1));
        c.operation=XTY_GET_CONFIG;c.reserved[0]=XTY_PACKET_MAGIC;
        assert(!xty_termios_decode(&c,&decoded));
        assert(decoded.c_cc[VINTR]==value && decoded.c_lflag==(ISIG|NOFLSH));
    }
    c.values[9]=257;assert(xty_termios_decode(&c,&decoded)==EIO);
    puts("TTY VINTR CODEC PASS: 256 encodings, ISIG/NOFLSH, disabled and invalid values");
}
int main(void)
{
    input_signal_codec();
    struct termios t,base,before;XtyControl c,out;unsigned bits,i;int action;
    static const int actions[]={TCSANOW,TCSADRAIN,TCSAFLUSH};
    static const unsigned operations[]={3,10,11};
    assert(!xty_termios_flow(TCOOFF,&out) && out.operation==12 && out.values[0]==1);
    assert(!xty_termios_flow(TCOON,&out) && out.values[0]==2);
    c=out;assert(xty_termios_flow(TCIOFF,&out)==ENOTSUP && !memcmp(&c,&out,sizeof c));
    assert(xty_termios_flow(TCION,&out)==ENOTSUP && !memcmp(&c,&out,sizeof c));
    assert(xty_termios_flow(99,&out)==EINVAL && !memcmp(&c,&out,sizeof c));
    assert(xty_termios_flow(TCOOFF,NULL)==EFAULT);
    for(bits=0;bits<128;++bits) {
        c=snapshot();c.values[0]=bits&7;c.values[1]=(bits>>3)&3;c.values[2]=(bits>>5)&3;
        memset(&t,0xa5,sizeof t);assert(xty_termios_decode(&c,&t)==0);
        assert(!!(t.c_iflag&ICRNL)==!!(bits&1));
        assert(!!(t.c_iflag&IGNCR)==!!(bits&2));
        assert(!!(t.c_iflag&INLCR)==!!(bits&4));
        assert(!!(t.c_oflag&OPOST)==!!(bits&8));
        assert(!!(t.c_oflag&ONLCR)==!!(bits&16));
        assert(!!(t.c_lflag&ICANON)==!!(bits&32));
        assert(!!(t.c_lflag&ECHO)==!!(bits&64));
        assert(t.c_cc[VEOF]==4 && t.c_cc[VMIN]==1 && t.c_cc[VTIME]==0);
        for(i=0;i<NCCS;++i) if(i!=VEOF && i!=VMIN && i!=VTIME)
            assert(t.c_cc[i]==(cc_t)_POSIX_VDISABLE);
        assert(cfgetispeed(&t)==B38400 && cfgetospeed(&t)==B38400);
        for(action=0;action<3;++action) {
            assert(xty_termios_encode(&t,actions[action],&out)==0);
            assert(out.version==1 && out.operation==operations[action]);
            assert(!out.reserved[0] && !out.reserved[1]);
            assert(!memcmp(out.values,c.values,sizeof out.values));
        }
    }
    c=snapshot();assert(!xty_termios_decode(&c,&base));
    t=base;t.c_cc[VMIN]=0;t.c_cc[VTIME]=2;
    assert(!xty_termios_encode(&t,TCSANOW,&out));
    assert(out.values[7]==1 && out.values[8]==2);
    t=base;t.c_lflag=ICANON|ECHO|ECHOE|ECHOK|ECHONL;
    t.c_cc[VERASE]=127;t.c_cc[VKILL]=21;t.c_cc[VEOL]=';';t.c_cc[VEOF]=(cc_t)_POSIX_VDISABLE;
    assert(!xty_termios_encode(&t,TCSADRAIN,&out));
    assert(out.values[2]==63 && out.values[3]==0 && out.values[4]==128 && out.values[5]==22 && out.values[6]==60);
    out.operation=XTY_GET_CONFIG;out.reserved[0]=XTY_PACKET_MAGIC;
    assert(!xty_termios_decode(&out,&before));
    assert(before.c_lflag==t.c_lflag && !memcmp(before.c_cc,t.c_cc,sizeof t.c_cc));
    {
        unsigned field,value;static const unsigned indices[]={VERASE,VKILL,VEOL};
        for(field=0;field<3;++field) for(value=0;value<=256;++value) {
            c=snapshot();c.values[4+field]=value;before=t;
            if(value && value-1==(unsigned char)_POSIX_VDISABLE) {
                assert(xty_termios_decode(&c,&t)==ENOTSUP && !memcmp(&t,&before,sizeof t));
            } else {
                assert(!xty_termios_decode(&c,&t));
                assert(t.c_cc[indices[field]]==(value?(cc_t)(value-1):(cc_t)_POSIX_VDISABLE));
                assert(!xty_termios_encode(&t,TCSANOW,&out) && out.values[4+field]==value);
            }
        }
        for(field=4;field<7;++field) {
            c=snapshot();c.values[field]=257;before=t;
            assert(xty_termios_decode(&c,&t)==EIO && !memcmp(&t,&before,sizeof t));
        }
        puts("TTY CANONICAL CODEC PASS: controls=3 wire_values=257 each, atomic invalid/unrepresentable rejection");
    }
    t=base;t.c_iflag|=IXON;rejected(&t,TCSANOW,ENOTSUP);
    t=base;t.c_oflag|=OCRNL;rejected(&t,TCSANOW,ENOTSUP);
    t=base;t.c_lflag|=IEXTEN;rejected(&t,TCSANOW,ENOTSUP);
    t=base;t.c_cflag|=PARENB;rejected(&t,TCSANOW,ENOTSUP);
    for(i=0;i<256;++i) {
        t=base;t.c_cc[VMIN]=(cc_t)i;t.c_cc[VTIME]=(cc_t)(255-i);
        assert(!xty_termios_encode(&t,TCSANOW,&out));
        out.operation=XTY_GET_CONFIG;out.reserved[0]=XTY_PACKET_MAGIC;
        assert(!xty_termios_decode(&out,&before));
        assert(before.c_cc[VMIN]==i && before.c_cc[VTIME]==255-i);
    }
    for(i=0;i<NCCS;++i) if(i!=VEOF && i!=VERASE && i!=VKILL && i!=VEOL && i!=VMIN && i!=VTIME && i!=VINTR) {
        t=base;t.c_cc[i]=7;rejected(&t,TCSANOW,ENOTSUP);
    }
    t=base;t.c_cc[VEOF]='\n';rejected(&t,TCSANOW,EINVAL);
    t=base;assert(!cfsetospeed(&t,B9600));rejected(&t,TCSANOW,ENOTSUP);
    t=base;assert(!cfsetispeed(&t,B9600));rejected(&t,TCSANOW,ENOTSUP);
    rejected(&base,99,EINVAL);rejected(NULL,TCSANOW,EFAULT);
    for(i=0;i<256;++i) if(i!='\n' && i!=(unsigned char)_POSIX_VDISABLE) {
        t=base;t.c_cc[VEOF]=(cc_t)i;
        assert(!xty_termios_encode(&t,TCSANOW,&out));assert(out.values[3]==i);
    }
    for(i=0;i<8;++i) {
        c=snapshot();
        switch(i) {
        case 0:c.version=99;break;case 1:c.operation=XTY_CAPS;break;
        case 2:c.reserved[0]=0;break;case 3:c.values[0]=8;break;
        case 4:c.values[1]=4;break;case 5:c.values[2]=256;break;
        case 6:c.values[3]=256;break;default:c.values[11]=1;break;
        }
        memset(&t,0xa5,sizeof t);before=t;
        assert(xty_termios_decode(&c,&t)==EIO);assert(!memcmp(&t,&before,sizeof t));
    }
    assert(!xty_termios_flush(TCIFLUSH,&out) && out.values[0]==1);
    assert(!xty_termios_flush(TCOFLUSH,&out) && out.values[0]==2);
    assert(!xty_termios_flush(TCIOFLUSH,&out) && out.values[0]==3);
    c=out;assert(xty_termios_flush(99,&out)==EINVAL && !memcmp(&c,&out,sizeof out));
    c=snapshot();c.values[3]=(unsigned char)_POSIX_VDISABLE;before=t;
    assert(xty_termios_decode(&c,&t)==ENOTSUP && !memcmp(&t,&before,sizeof t));
    puts("TTY TERMIOS CODEC PASS: 128 flag combinations, 3 actions, 254 EOF values, disabled controls, strict rejection, atomic snapshots");
    return 0;
}
