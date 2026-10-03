#include "../src/clipboard_text.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned char raw[CLIP_RAW_LIMIT+4], output[CLIP_TEXT_LIMIT+4];
static size_t offset;
static void be32(unsigned char *p, uint32_t value)
{ p[0]=(unsigned char)(value>>24); p[1]=(unsigned char)(value>>16); p[2]=(unsigned char)(value>>8); p[3]=(unsigned char)value; }
static void start(void)
{ memcpy(raw,"FORM\0\0\0\0FTXT",12); offset=12; }
static void chunk(const char *id,const unsigned char *data,size_t n)
{
    assert(offset+8+n+(n&1)<=sizeof raw);
    memcpy(raw+offset,id,4); be32(raw+offset+4,(uint32_t)n); offset+=8;
    if(n) memcpy(raw+offset,data,n);
    offset+=n;
    if(n&1) raw[offset++]=0;
    be32(raw+4,(uint32_t)(offset-8));
}
static void decode_ok(const unsigned char *want,size_t n,unsigned expected_format)
{
    size_t written=99; unsigned format=99;
    assert(clip_decode_text(raw,offset,output,sizeof output,&written,&format)==CLIP_OK);
    assert(written==n && format==expected_format);
    assert(!n || !memcmp(output,want,n));
}
static void decode_error(int want)
{
    size_t n=99,i; unsigned format=99;
    memset(output,0xa5,sizeof output);
    assert(clip_decode_text(raw,offset,output,sizeof output,&n,&format)==want);
    assert(n==0 && format==0);
    for(i=0;i<sizeof output;++i) assert(output[i]==0xa5);
}

static void encode(void)
{
    static const unsigned char expected[]={
        'F','O','R','M',0,0,0,30,'F','T','X','T',
        'C','H','R','S',0,0,0,3,'A',0xe9,'?',0,
        'U','T','F','8',0,0,0,5,'A',0xc3,0xa9,0xc4,0x85,0
    };
    const unsigned char text[]="A\xc3\xa9\xc4\x85";
    size_t n=0,i; unsigned lossy=0;
    assert(clip_encode_utf8(text,sizeof text-1,raw,sizeof raw,&n,&lossy)==CLIP_OK);
    assert(n==sizeof expected && lossy==1 && !memcmp(raw,expected,n));
    offset=n; decode_ok(text,sizeof text-1,CLIP_FORMAT_UTF8);
    memset(raw,0xa5,sizeof raw);
    assert(clip_encode_utf8(text,sizeof text-1,raw,sizeof expected-1,&n,&lossy)==CLIP_ERR_CAPACITY);
    assert(n==0 && lossy==0);
    for(i=0;i<sizeof raw;++i) assert(raw[i]==0xa5);
    assert(clip_encode_utf8(NULL,0,raw,sizeof raw,&n,&lossy)==CLIP_OK && n==28 && !lossy);
    offset=n; decode_ok(NULL,0,CLIP_FORMAT_UTF8);
    assert(clip_encode_utf8((const unsigned char *)"\0",1,raw,sizeof raw,&n,&lossy)==CLIP_ERR_NUL);
    assert(clip_encode_utf8((const unsigned char *)"\xc0\xaf",2,raw,sizeof raw,&n,&lossy)==CLIP_ERR_UTF8);
}
static void decode(void)
{
    const unsigned char latin[]={0xe9,'\n'};
    const unsigned char cs_utf8[]={0,0,0,106},cs_latin[]={0,0,0,4},cs_bad[]={0,0,0,5};
    const unsigned char cs_default[]={0,0,0,0};
    start(); chunk("CSET",cs_default,4); chunk("CHRS",latin,2);
    decode_ok((const unsigned char *)"\xc3\xa9\n",3,CLIP_FORMAT_LATIN1);
    start(); chunk("CHRS",(const unsigned char *)"A",1);
    chunk("JUNK",(const unsigned char *)"xyz",3); chunk("CHRS",latin,2);
    decode_ok((const unsigned char *)"A\xc3\xa9\n",4,CLIP_FORMAT_LATIN1);
    start(); chunk("CHRS",(const unsigned char *)"old",3);
    chunk("UTF8",(const unsigned char *)"\xf0\x9f",2);
    chunk("JUNK",NULL,0); chunk("CHRS",(const unsigned char *)"old2",4);
    chunk("UTF8",(const unsigned char *)"\x98\x80",2);
    decode_ok((const unsigned char *)"\xf0\x9f\x98\x80",4,CLIP_FORMAT_UTF8);
    start(); chunk("CHRS",(const unsigned char *)"old",3); chunk("UTF8",NULL,0);
    decode_ok(NULL,0,CLIP_FORMAT_UTF8);
    start(); chunk("CSET",cs_utf8,4); chunk("CHRS",(const unsigned char *)"\xc4",1);
    chunk("CHRS",(const unsigned char *)"\x85",1);
    decode_ok((const unsigned char *)"\xc4\x85",2,CLIP_FORMAT_CSET_UTF8);
    chunk("CSET",cs_latin,4); chunk("CHRS",latin,2);
    decode_ok((const unsigned char *)"\xc4\x85\xc3\xa9\n",5,CLIP_FORMAT_MIXED);
    start(); chunk("CSET",cs_bad,4); chunk("CHRS",latin,2); decode_error(CLIP_ERR_CHARSET);
    chunk("UTF8",(const unsigned char *)"ok",2); decode_ok((const unsigned char *)"ok",2,CLIP_FORMAT_UTF8);
    start(); be32(raw+4,4); decode_ok(NULL,0,CLIP_FORMAT_EMPTY);
    start(); chunk("JUNK",NULL,0); decode_ok(NULL,0,CLIP_FORMAT_EMPTY);
    start(); chunk("CHRS",NULL,0); decode_ok(NULL,0,CLIP_FORMAT_LATIN1);
}
static void invalid(void)
{
    static const unsigned char bad[][4]={{0xc0,0xaf,0,0},{0xed,0xa0,0x80,0},{0xf4,0x90,0x80,0x80},{0x80,0,0,0},{0xe2,0x82,0,0}};
    const unsigned lengths[]={2,3,4,1,2}; unsigned i;
    for(i=0;i<sizeof lengths/sizeof lengths[0];++i) {
        start(); chunk("CHRS",(const unsigned char *)"fallback",8);
        chunk("UTF8",bad[i],lengths[i]); decode_error(CLIP_ERR_UTF8);
    }
    start(); chunk("UTF8",(const unsigned char *)"a\0b",3); decode_error(CLIP_ERR_NUL);
    start(); chunk("CHRS",(const unsigned char *)"a\0b",3); decode_error(CLIP_ERR_NUL);
    start(); chunk("CHRS",(const unsigned char *)"abc",3); raw[8]='I'; decode_error(CLIP_ERR_FORMAT);
    start(); chunk("CHRS",(const unsigned char *)"abc",3); --offset; decode_error(CLIP_ERR_FORMAT);
    be32(raw+4,(uint32_t)(offset-8)); decode_error(CLIP_ERR_FORMAT); /* Missing padding. */
    start(); chunk("CHRS",(const unsigned char *)"abc",3); be32(raw+16,0xffffffff); decode_error(CLIP_ERR_FORMAT);
    start(); chunk("CHRS",(const unsigned char *)"abc",3); raw[offset++]=0; decode_error(CLIP_ERR_FORMAT);
    start(); chunk("CSET",(const unsigned char *)"abc",3); decode_error(CLIP_ERR_FORMAT);
    offset=0; decode_error(CLIP_ERR_FORMAT);
}
static void bounds(void)
{
    size_t n=99,i; unsigned value=99;
    memset(output,'x',sizeof output);
    assert(clip_encode_utf8(output,CLIP_TEXT_LIMIT+1,raw,sizeof raw,&n,&value)==CLIP_ERR_LIMIT);
    assert(clip_encode_utf8(output,CLIP_TEXT_LIMIT,raw,sizeof raw,&n,&value)==CLIP_ERR_LIMIT);
    /* 28 bytes of headers + two copies of 32754 ASCII bytes fill 64 KiB. */
    assert(clip_encode_utf8(output,32754,raw,sizeof raw,&n,&value)==CLIP_OK && n==CLIP_RAW_LIMIT);
    offset=n;
    memset(output,0xa5,sizeof output);
    assert(clip_decode_text(raw,offset,output,sizeof output,&n,&value)==CLIP_OK);
    assert(n==32754 && value==CLIP_FORMAT_UTF8);
    for(i=0;i<n;++i) assert(output[i]=='x');
    start(); chunk("UTF8",(const unsigned char *)"hello",5);
    memset(output,0xa5,sizeof output);
    assert(clip_decode_text(raw,offset,output,4,&n,&value)==CLIP_ERR_CAPACITY && !n && !value);
    assert(output[0]==0xa5 && output[4]==0xa5);
    memset(output,0xff,sizeof output); start(); chunk("CHRS",output,32769); decode_error(CLIP_ERR_LIMIT);
    offset=CLIP_RAW_LIMIT+1; decode_error(CLIP_ERR_LIMIT);
}
int main(int argc,char **argv)
{
    /* Separate modes make both encoder and decoder red failures observable. */
    if(argc==1 || !strcmp(argv[1],"encode")) encode();
    if(argc==1 || !strcmp(argv[1],"decode")) { decode(); invalid(); bounds(); }
    puts("clipboard text tests passed (FTXT/UTF8/CHRS/CSET, validation, bounds, atomic errors)");
    return 0;
}
