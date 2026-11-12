#define INK_MOGO_PORT_IMPL
#include <runtime.h>
#include <settings.h>
#include <ui_kit.h>
static int ink_mogo_width, ink_mogo_height;
static bool ink_mogo_ui;
void ink_mogo_ui_coordinates(bool enabled) { ink_mogo_ui=enabled; }
void ink_mogo_canvas(int w, int h) { ink_mogo_width=w; ink_mogo_height=h; }
static EpdRect ink_mogo_rect(EpdRect r) { if (!ink_mogo_ui) return r; return (EpdRect){r.x*ink_mogo_width/684,r.y*ink_mogo_height/1216,r.width*ink_mogo_width/684,r.height*ink_mogo_height/1216}; }
void ui_clear_page(uint8_t *fb) { memset(fb,255,(size_t)ink_mogo_width*ink_mogo_height); }
void ui_fill_rect_fast(EpdRect r, uint8_t gray, uint8_t *fb) { epd_fill_rect(ink_mogo_rect(r),gray,fb); }
void ui_fill_round_rect(uint8_t *fb, EpdRect r, int radius, uint8_t gray) {
    r=ink_mogo_rect(r); radius=radius*ink_mogo_width/684;
    for (int y=0;y<r.height;++y) {
        int dy=y<radius?radius-y:y>=r.height-radius?y-(r.height-radius-1):0, inset=0;
        while (inset<radius && (radius-inset)*(radius-inset)+dy*dy>radius*radius) ++inset;
        epd_fill_rect((EpdRect){r.x+inset,r.y+y,r.width-2*inset,1},gray,fb);
    }
}
void ui_draw_round_rect(uint8_t *fb, EpdRect r, int radius, uint8_t gray) {
    r=ink_mogo_rect(r); radius=radius*ink_mogo_width/684;
    for (int y=0;y<r.height;++y) {
        int dy=y<radius?radius-y:y>=r.height-radius?y-(r.height-radius-1):0, inset=0;
        while (inset<radius && (radius-inset)*(radius-inset)+dy*dy>radius*radius) ++inset;
        if (y<2 || y>=r.height-2) epd_fill_rect((EpdRect){r.x+inset,r.y+y,r.width-2*inset,1},gray,fb);
        else {
            epd_fill_rect((EpdRect){r.x+inset,r.y+y,2,1},gray,fb);
            epd_fill_rect((EpdRect){r.x+r.width-inset-2,r.y+y,2,1},gray,fb);
        }
    }
}
int ui_text_effective_px(int px) { return ink_mogo_scaled((px*6+2)/5); }
int ui_text_title_fit(const char *s, int px, int width, const char *sample) {
    (void)sample;
    while (px>1 && ttf_text_width_px(px,s)>width) --px;
    return px;
}
void ui_text_box(uint8_t *fb, EpdRect r, int px, const char *s, enum EpdFontFlags align, bool inverted) {
    r=ink_mogo_rect(r); px=ui_text_effective_px(px);
    if (px>r.height-2) px=r.height-2;
    char text[320]; size_t n=strlen(s);
    if (n>=sizeof text) n=sizeof text-1;
    while (n && ((unsigned char)s[n]&0xc0)==0x80) --n;
    memcpy(text,s,n); text[n]=0;
    while (n && ttf_text_width_px(px,text)>r.width) { do { --n; } while (n && ((unsigned char)text[n]&0xc0)==0x80); text[n]=0; }
    int above=0,below=0; ttf_measure_line_px(px,text,&above,&below);
    int x=align==EPD_DRAW_ALIGN_LEFT?r.x:align==EPD_DRAW_ALIGN_RIGHT?r.x+r.width:r.x+r.width/2;
    ttf_draw_text_px(fb,x,r.y+r.height/2+(above-below)/2,px,text,align,inverted?15:0,inverted?0:15);
}
EpdRect ui_inset_rect(EpdRect r, int n) { return (EpdRect){r.x+n,r.y+n,r.width-2*n,r.height-2*n}; }
void ui_text_title_background_vc(uint8_t *fb, int x, int y, int px, const char *s, const char *sample, enum EpdFontFlags align, uint8_t bg) {
    (void)sample; (void)bg;
    ui_text_box(fb,(EpdRect){x,y-40,540,80},px*5/6,s,align,false);
}
void ui_draw_button(uint8_t *fb, EpdRect r, const char *text, bool selected) {
    ui_fill_round_rect(fb,r,16,selected?0:255); ui_draw_round_rect(fb,r,16,0); ui_text_box(fb,ui_inset_rect(r,8),28,text,EPD_DRAW_ALIGN_CENTER,selected);
}
void ui_nav_back(uint8_t *fb, int x, int y) { ui_text_box(fb,(EpdRect){x,y,70,70},40,"‹",EPD_DRAW_ALIGN_CENTER,false); }
void ui_text_fixed(uint8_t *fb,int x,int y,int px,const char *s,enum EpdFontFlags align,bool inv) { ui_text_box(fb,(EpdRect){x-100,y,200,80},px,s,align,inv); }
void epd_draw_line(int x0,int y0,int x1,int y1,uint8_t gray,uint8_t *fb) {
    int dx=abs(x1-x0),dy=abs(y1-y0),steps=dx>dy?dx:dy;
    for (int i=0;i<=steps;++i) ui_fill_rect_fast((EpdRect){x0+(steps?(x1-x0)*i/steps:0),y0+(steps?(y1-y0)*i/steps:0),2,2},gray,fb);
}
void epd_fill_circle(int x,int y,int radius,uint8_t gray,uint8_t *fb) { ui_fill_round_rect(fb,(EpdRect){x-radius,y-radius,2*radius+1,2*radius+1},radius,gray); }
void epd_draw_circle(int x,int y,int radius,uint8_t gray,uint8_t *fb) { ui_draw_round_rect(fb,(EpdRect){x-radius,y-radius,2*radius,2*radius},radius,gray); }
