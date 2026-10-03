#include "../src/aros_clipboard.h"
#include "../src/clipboard_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
static unsigned char raw[65536],text[65536];
static uint32_t be32(const unsigned char *p)
{ return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3]; }
static unsigned hash(const unsigned char *p,size_t n)
{ unsigned h=2166136261u; while(n--) h=(h^*p++)*16777619u; return h; }
static const char *sample(const char *name)
{
    if(!strcmp(name,"ascii")) return "AROS clipboard test 123";
    if(!strcmp(name,"latin1")) return "Grüße AROS";
    if(!strcmp(name,"polish")) return "Zażółć gęślą jaźń";
    if(!strcmp(name,"emoji")) return "中文 😀 👨‍👩‍👧‍👦 🇵🇱";
    return NULL;
}
static int file_save(const char *path,const unsigned char *data,size_t n)
{
    FILE *f=fopen(path,"rb"); int failed;
    if(f) { fclose(f); puts("Refusing to overwrite existing file"); return -1; }
    f=fopen(path,"wb"); if(!f) return -1;
    failed=fwrite(data,1,n,f)!=n; if(fclose(f)) failed=1;
    return failed?-1:0;
}
int main(int argc,char **argv)
{
    unsigned unit,format=0,lossy=0; size_t n=0,m=0; int result;
    const char *value; char *end; unsigned long parsed;
    if(argc<3) { puts("clip-probe inspect|backup|restore|write|verify|text unit [file/sample]"); return 20; }
    errno=0;
    parsed=strtoul(argv[2],&end,10);
    if(errno || end==argv[2] || argv[2][0]=='-' || *end || parsed>255) return 20;
    unit=(unsigned)parsed;
    if(!strcmp(argv[1],"restore")) {
        FILE *f; int ch;
        if(argc!=4 || !(f=fopen(argv[3],"rb"))) return 20;
        n=fread(raw,1,sizeof raw,f); ch=fgetc(f);
        result=ferror(f) || ch!=EOF; fclose(f);
        if(result || aros_clip_write(unit,raw,n)) return 20;
        puts("Raw clipboard restored"); return 0;
    }
    if(!strcmp(argv[1],"write")) {
        if(argc!=4 || !(value=sample(argv[3]))) return 20;
        result=clip_encode_utf8((const unsigned char *)value,strlen(value),raw,sizeof raw,&n,&lossy);
        if(result || aros_clip_write(unit,raw,n)) return 20;
        printf("Wrote %lu IFF bytes; legacy replacements=%u\n",(unsigned long)n,lossy); return 0;
    }
    if(aros_clip_read(unit,raw,sizeof raw,&n)) { puts("Clipboard read failed/too large"); return 20; }
    printf("unit=%u raw=%lu hash=%08x\n",unit,(unsigned long)n,hash(raw,n));
    if(!strcmp(argv[1],"backup")) {
        if(argc!=4 || file_save(argv[3],raw,n)) return 20;
        puts("Raw clipboard backed up without conversion"); return 0;
    }
    result=clip_decode_text(raw,n,text,sizeof text,&m,&format);
    printf("decode=%d format=%u utf8-bytes=%lu hash=%08x\n",result,format,(unsigned long)m,hash(text,m));
    if(!strcmp(argv[1],"inspect")) {
        size_t at=12;
        while(at+8<=n) {
            size_t size=be32(raw+at+4);
            printf("chunk %.4s bytes=%lu\n",raw+at,(unsigned long)size);
            if(size>n-at-8 || (size&1)>n-at-8-size) break;
            at+=8+size+(size&1);
        }
        return 0;
    }
    if(!strcmp(argv[1],"verify")) {
        if(argc!=4 || !(value=sample(argv[3]))) return 20;
        if(result || m!=strlen(value) || memcmp(text,value,m)) {
            puts("Text comparison FAILED"); return 20;
        }
        puts("Text comparison passed"); return 0;
    }
    if(!strcmp(argv[1],"text")) {
        if(argc!=4 || result || file_save(argv[3],text,m)) return 20;
        puts("Decoded text saved"); return 0;
    }
    return 20;
}
