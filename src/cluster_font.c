#include "cluster_font.h"
#include "aros_font.h"
#ifdef __AROS__
#include <proto/freetype2.h>
#else
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#endif
#include <hb.h>
#include <hb-ot.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef struct Entry {
    uint32_t scalars[XCLUSTER_MAX];
    unsigned count, span, generation, valid;
    XClusterRaster raster;
} Entry;
static Entry entries[128];

static hb_blob_t *font_table(hb_face_t *face,hb_tag_t tag,void *data)
{
    FT_ULong size=0; unsigned char *bytes; (void)face;
    if(FT_Load_Sfnt_Table((FT_Face)data,tag,0,NULL,&size) || !size || size>32*1024*1024)
        return hb_blob_get_empty();
    bytes=malloc(size);
    if(!bytes) return hb_blob_get_empty();
    if(FT_Load_Sfnt_Table((FT_Face)data,tag,0,bytes,&size)) { free(bytes); return hb_blob_get_empty(); }
    return hb_blob_create((const char *)bytes,(unsigned)size,HB_MEMORY_MODE_READONLY,bytes,free);
}

static hb_buffer_t *shape(FT_Face face,const uint32_t *s,unsigned n)
{
    hb_face_t *hf=hb_face_create_for_tables(font_table,face,NULL);
    hb_font_t *font=hb_font_create(hf);
    hb_buffer_t *buffer=hb_buffer_create();
    hb_ot_font_set_funcs(font);
    hb_font_set_scale(font,face->size->metrics.x_ppem*64,face->size->metrics.y_ppem*64);
    hb_buffer_add_utf32(buffer,s,n,0,n);
    hb_buffer_set_flags(buffer,HB_BUFFER_FLAG_REMOVE_DEFAULT_IGNORABLES);
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(font,buffer,NULL,0);
    hb_font_destroy(font); hb_face_destroy(hf);
    return buffer;
}

static uint32_t bitmap_pixel(const FT_Bitmap *b,unsigned x,unsigned y)
{
    const unsigned char *p;
    /* FreeType's buffer points at the allocation start. For an up-flow
     * bitmap its logical top row is the last physical row. */
    if(b->pitch<0)
        p=b->buffer+(size_t)(b->rows-y-1)*(size_t)(-(long)b->pitch);
    else p=b->buffer+(size_t)y*(size_t)b->pitch;
    unsigned a;
    if(b->pixel_mode==FT_PIXEL_MODE_BGRA) {
        p+=4*x;
        return ((uint32_t)p[3]<<24)|((uint32_t)p[2]<<16)|((uint32_t)p[1]<<8)|p[0];
    }
    if(b->pixel_mode==FT_PIXEL_MODE_MONO) a=(p[x/8]&(0x80>>(x%8)))?255:0;
    else a=b->num_grays>1 ? p[x]*255u/(b->num_grays-1) : 0;
    return a*0x01010101u;
}

static uint32_t over(uint32_t src,uint32_t dst)
{
    unsigned shift,inv=255-(src>>24); uint32_t result=0;
    for(shift=0;shift<32;shift+=8) {
        unsigned value=((src>>shift)&255)+(((dst>>shift)&255)*inv+127)/255;
        result|=(uint32_t)(value>255?255:value)<<shift;
    }
    return result;
}

static int render(XClusterRaster *r,FT_Face face,hb_buffer_t *buffer)
{
    unsigned n,i; int pass;
    int minx=INT_MAX,miny=INT_MAX,maxx=INT_MIN,maxy=INT_MIN;
    double scale=1.0,ox=0,oy=0;
    hb_glyph_info_t *info=hb_buffer_get_glyph_infos(buffer,&n);
    hb_glyph_position_t *pos=hb_buffer_get_glyph_positions(buffer,NULL);
    if(!n || n>64) return XFONT_SIZE_ERROR;
    r->glyphs=n;
    /* Measure the complete cluster, then sample bounded destination pixels.
     * FreeType owns its strike bitmap; no full-size application copy. */
    for(pass=0;pass<2;++pass) {
        int px=0,py=0;
        for(i=0;i<n;++i) {
            FT_Bitmap *b; int left,top; unsigned dx,dy;
            if(FT_Load_Glyph(face,info[i].codepoint,FT_LOAD_RENDER|FT_LOAD_COLOR|FT_LOAD_NO_HINTING))
                return XFONT_LOAD_ERROR;
            b=&face->glyph->bitmap;
            if(b->width && b->rows && b->pixel_mode!=FT_PIXEL_MODE_BGRA &&
               b->pixel_mode!=FT_PIXEL_MODE_GRAY && b->pixel_mode!=FT_PIXEL_MODE_MONO)
                return XFONT_FORMAT_ERROR;
            if(b->width>4096 || b->rows>4096) return XFONT_SIZE_ERROR;
            left=(px+pos[i].x_offset)/64+face->glyph->bitmap_left;
            top=(int)xfont_face_baseline(r->source)-(py+pos[i].y_offset)/64-face->glyph->bitmap_top;
            px+=pos[i].x_advance; py+=pos[i].y_advance;
            if(!b->width || !b->rows) continue;
            if(!pass) {
                if(left<minx) minx=left;
                if(top<miny) miny=top;
                if(left+(int)b->width>maxx) maxx=left+b->width;
                if(top+(int)b->rows>maxy) maxy=top+b->rows;
                if(b->pixel_mode==FT_PIXEL_MODE_BGRA) r->colour=1;
                continue;
            }
            for(dy=0;dy<r->height;++dy) for(dx=0;dx<r->width;++dx) {
                unsigned sx,sy,c,sum[4]={0,0,0,0};
                for(sy=0;sy<2;++sy) for(sx=0;sx<2;++sx) {
                    double fx=((double)dx+(sx+.5)/2-ox)/scale-left;
                    double fy=((double)dy+(sy+.5)/2-oy)/scale-top;
                    if(fx>=0 && fy>=0 && fx<b->width && fy<b->rows) {
                        uint32_t p=bitmap_pixel(b,(unsigned)fx,(unsigned)fy);
                        for(c=0;c<4;++c) sum[c]+=(p>>(8*c))&255;
                    }
                }
                { uint32_t p=0; unsigned at=dy*r->width+dx;
                  for(c=0;c<4;++c) p|=((sum[c]+2)/4)<<(8*c);
                  r->pixels[at]=over(p,r->pixels[at]); }
            }
        }
        if(!pass && minx!=INT_MAX) {
            double sx=(double)r->width/(maxx-minx),sy=(double)r->height/(maxy-miny);
            if(sx<scale) scale=sx;
            if(sy<scale) scale=sy;
            if(r->colour || scale<1) {
                ox=(r->width-(maxx-minx)*scale)/2-minx*scale;
                oy=(r->height-(maxy-miny)*scale)/2-miny*scale;
            } else {
                if(minx<0) ox=-minx;
                else if(maxx>(int)r->width) ox=(int)r->width-maxx;
                if(miny<0) oy=-miny;
                else if(maxy>(int)r->height) oy=(int)r->height-maxy;
            }
        }
    }
    return XFONT_OK;
}

const XClusterRaster *xfont_cluster(const uint32_t *s,unsigned count,unsigned span)
{
    unsigned i,j,hash=2166136261u,order[5]={0,1,2,3,4};
    static const XClusterRaster invalid={.status=XFONT_SIZE_ERROR};
    Entry *e; XClusterRaster *r;
    if(!s || !count || count>XCLUSTER_MAX || span<1 || span>2) return &invalid;
    for(i=0;i<count;++i) hash=(hash^s[i])*16777619u;
    e=&entries[(hash^span)%128]; r=&e->raster;
    if(e->valid && e->generation==xfont_generation() && e->count==count && e->span==span &&
       !memcmp(e->scalars,s,count*sizeof *s)) return r;
    memset(e,0,sizeof *e); memcpy(e->scalars,s,count*sizeof *s);
    e->valid=1; e->count=count; e->span=span; e->generation=xfont_generation();
    r->width=xfont_width()*span; r->height=xfont_height();
    if(!xfont_face(0)) { r->status=XFONT_CLOSED; return r; }
    if(r->width>XRASTER_MAX || r->height>XRASTER_MAX) { r->status=XFONT_SIZE_ERROR; return r; }
    if(span==2) { order[0]=4; order[1]=2; order[2]=0; order[3]=1; order[4]=3; }
    for(i=0;i<count;++i) if(s[i]==0xfe0e) { order[0]=0;order[1]=1;order[2]=2;order[3]=3;order[4]=4; }
    r->status=XFONT_MISSING;
    for(i=0;i<5;++i) {
        FT_Face face=(FT_Face)xfont_face(order[i]); hb_buffer_t *buffer;
        hb_glyph_info_t *info; unsigned n; int missing=0;
        if(!face) continue;
        buffer=shape(face,s,count); info=hb_buffer_get_glyph_infos(buffer,&n);
        if(!hb_buffer_allocation_successful(buffer)) missing=1;
        else if(!n) {
            /* REMOVE_DEFAULT_IGNORABLES can legitimately remove the whole
             * cluster (e.g. a standalone ZWJ or variation selector). */
            memset(r->pixels,0,sizeof r->pixels);
            r->glyphs=0; r->colour=0; r->source=order[i]; r->status=XFONT_OK;
            hb_buffer_destroy(buffer);
            return r;
        }
        for(j=0;j<n;++j) if(!info[j].codepoint) missing=1;
        if(!missing) {
            memset(r->pixels,0,sizeof r->pixels); r->colour=0; r->source=order[i];
            r->status=render(r,face,buffer);
            hb_buffer_destroy(buffer);
            if(r->status==XFONT_OK) return r;
        } else hb_buffer_destroy(buffer);
    }
    /* Stable visible fallback, not an apparently successful blank cluster. */
    { const XFontGlyph *g=xfont_glyph(0xfffd);
      if(g->status==XFONT_OK || g->status==XFONT_MISSING)
        for(i=0;i<g->height;++i) for(j=0;j<g->width;++j) {
            int x=g->left+j,y=g->top+i;
            if(x>=0 && y>=0 && x<(int)r->width && y<(int)r->height && (g->pixels[i]&(1u<<j)))
                r->pixels[y*r->width+x]=0xffffffff;
        }
    }
    return r;
}

void xfont_cluster_rgb(const XClusterRaster *r,uint32_t fg,uint32_t bg,unsigned char *out)
{
    unsigned i,c;
    if(!r || !out || r->width>XRASTER_MAX || r->height>XRASTER_MAX) return;
    for(i=0;i<r->width*r->height;++i) {
        unsigned a=r->pixels[i]>>24;
        for(c=0;c<3;++c) {
            unsigned shift=16-8*c;
            unsigned v=r->colour ? (r->pixels[i]>>shift)&255 : (((fg>>shift)&255)*a+127)/255;
            v+=(((bg>>shift)&255)*(255-a)+127)/255;
            out[i*3+c]=(unsigned char)(v>255?255:v);
        }
    }
}
