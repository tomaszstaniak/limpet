#include "clipboard_text.h"
#include "../vendor/utf8proc/utf8proc.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Format references:
 * https://wiki.amigaos.net/wiki/UTF8_IFF_UTF-8_Unicode_Text
 * https://wiki.amigaos.net/wiki/FTXT_IFF_Formatted_Text
 * https://www.iana.org/assignments/character-sets/character-sets.xhtml
 * CSET 0 legacy and 106 UTF8 usage independently checked against NetSurf's
 * frontends/amiga/iff_cset.h and frontends/amiga/clipboard.c. */

static uint32_t read32(const unsigned char *p)
{
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}
static void write32(unsigned char *p, size_t n)
{
    p[0]=(unsigned char)(n>>24); p[1]=(unsigned char)(n>>16);
    p[2]=(unsigned char)(n>>8); p[3]=(unsigned char)n;
}
static int validate(const unsigned char *text,size_t length,size_t *scalars,unsigned *lossy)
{
    size_t at=0,count=0; unsigned loss=0;
    while(at<length) {
        utf8proc_int32_t scalar;
        utf8proc_ssize_t n=utf8proc_iterate(text+at,(utf8proc_ssize_t)(length-at),&scalar);
        if(n<0) return CLIP_ERR_UTF8;
        if(!scalar) return CLIP_ERR_NUL;
        ++count; if(scalar>255) ++loss;
        at+=(size_t)n;
    }
    if(scalars) *scalars=count;
    if(lossy) *lossy=loss;
    return CLIP_OK;
}
int clip_encode_utf8(const unsigned char *text, size_t length,
                     unsigned char *out, size_t capacity, size_t *written,
                     unsigned *lossy)
{
    size_t chars,total,at,pos; unsigned loss; int error;
    if(written) *written=0;
    if(lossy) *lossy=0;
    if(!written || !lossy || (!text && length) || (!out && capacity)) return CLIP_ERR_ARGUMENT;
    if(length>CLIP_TEXT_LIMIT) return CLIP_ERR_LIMIT;
    error=validate(text,length,&chars,&loss);
    if(error) return error;
    total=28+length+(length&1)+chars+(chars&1);
    if(total>CLIP_RAW_LIMIT) return CLIP_ERR_LIMIT;
    if(total>capacity) return CLIP_ERR_CAPACITY;
    memcpy(out,"FORM",4); write32(out+4,total-8); memcpy(out+8,"FTXTCHRS",8);
    write32(out+16,chars); at=0; pos=20;
    while(at<length) {
        utf8proc_int32_t scalar;
        utf8proc_ssize_t n=utf8proc_iterate(text+at,(utf8proc_ssize_t)(length-at),&scalar);
        out[pos++]=scalar>255 ? '?' : (unsigned char)scalar;
        at+=(size_t)n;
    }
    if(chars&1) out[pos++]=0;
    memcpy(out+pos,"UTF8",4); write32(out+pos+4,length); pos+=8;
    if(length) memcpy(out+pos,text,length);
    pos+=length;
    if(length&1) out[pos++]=0;
    *written=pos; *lossy=loss;
    return CLIP_OK;
}

/* Validate every chunk boundary before selecting either representation.
 * Padding byte values are ignored as required for unknown IFF chunks. */
static int structure(const unsigned char *raw,size_t length,int *unicode)
{
    size_t at=12;
    if(length<12 || memcmp(raw,"FORM",4) || memcmp(raw+8,"FTXT",4) ||
       read32(raw+4)!=length-8) return CLIP_ERR_FORMAT;
    while(at<length) {
        size_t n,padded;
        if(length-at<8) return CLIP_ERR_FORMAT;
        n=read32(raw+at+4);
        if(n>length-at-8) return CLIP_ERR_FORMAT;
        padded=n+(n&1);
        if(padded>length-at-8) return CLIP_ERR_FORMAT;
        if(!memcmp(raw+at,"UTF8",4)) *unicode=1;
        if(!memcmp(raw+at,"CSET",4) && n<4) return CLIP_ERR_FORMAT;
        at+=8+padded;
    }
    return CLIP_OK;
}
int clip_decode_text(const unsigned char *raw, size_t length,
                     unsigned char *out, size_t capacity, size_t *written,
                     unsigned *format)
{
    size_t at=12,used=0; uint32_t charset=4;
    unsigned modes=0; int unicode=0,error; unsigned char *text;
    if(written) *written=0;
    if(format) *format=0;
    if(!written || !format || (!raw && length) || (!out && capacity)) return CLIP_ERR_ARGUMENT;
    if(length>CLIP_RAW_LIMIT) return CLIP_ERR_LIMIT;
    error=structure(raw,length,&unicode);
    if(error) return error;
    text=malloc(CLIP_TEXT_LIMIT);
    if(!text) return CLIP_ERR_MEMORY;
    while(at<length) {
        const unsigned char *id=raw+at,*data=raw+at+8;
        size_t n=read32(raw+at+4),i;
        if(!unicode && !memcmp(id,"CSET",4)) charset=read32(data);
        if((unicode && !memcmp(id,"UTF8",4)) || (!unicode && !memcmp(id,"CHRS",4))) {
            if(!unicode && charset!=0 && charset!=4 && charset!=106) { error=CLIP_ERR_CHARSET; goto done; }
            if(unicode || charset==106) {
                modes|=2;
                if(n>CLIP_TEXT_LIMIT-used) { error=CLIP_ERR_LIMIT; goto done; }
                if(n) memcpy(text+used,data,n);
                used+=n;
            } else {
                modes|=1;
                for(i=0;i<n;++i) {
                    unsigned char c=data[i]; size_t width=c<128 ? 1 : 2;
                    if(width>CLIP_TEXT_LIMIT-used) { error=CLIP_ERR_LIMIT; goto done; }
                    if(c>=128) {
                        text[used++]=(unsigned char)(0xc0|(c>>6));
                        text[used++]=(unsigned char)(0x80|(c&63));
                    } else text[used++]=c;
                }
            }
        }
        at+=8+n+(n&1);
    }
    error=validate(text,used,NULL,NULL);
    if(error) goto done;
    if(used>capacity) { error=CLIP_ERR_CAPACITY; goto done; }
    if(used) memcpy(out,text,used);
    *written=used;
    *format=unicode ? CLIP_FORMAT_UTF8 : modes==1 ? CLIP_FORMAT_LATIN1 :
            modes==2 ? CLIP_FORMAT_CSET_UTF8 : modes==3 ? CLIP_FORMAT_MIXED : CLIP_FORMAT_EMPTY;
done:
    free(text);
    return error;
}
