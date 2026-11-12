static book_shelf_entry_t *ink_mogo_shelf;
static int ink_mogo_shelf_count, ink_mogo_shelf_page;
enum { INK_MOGO_SETTING_COUNT=8, INK_MOGO_SHELF_ROWS=4, INK_MOGO_TOOL_COUNT=6 };
enum { INK_MOGO_TOOLS=10, INK_MOGO_MARKS, INK_MOGO_SETTINGS };
static const char *const ink_mogo_setting_actions[]={"mogo.set_font_size","mogo.set_line_spacing","mogo.set_paragraph_spacing","mogo.set_margin","mogo.set_weight","mogo.set_indent","mogo.set_header","mogo.set_footer"};
static const char *const ink_mogo_setting_names[] = {TR_NOOP("字号"),TR_NOOP("行距"),TR_NOOP("段距"),TR_NOOP("边距"),TR_NOOP("字重"),TR_NOOP("缩进"),TR_NOOP("页眉"),TR_NOOP("页脚")};
_Static_assert(sizeof ink_mogo_setting_actions/sizeof *ink_mogo_setting_actions == INK_MOGO_SETTING_COUNT,"setting actions");
static int ink_mogo_setting_value(int index) {
    const ink_mogo_settings_t *p=&ink_mogo_settings;
    int values[]={p->px,p->line,p->paragraph,p->margin[0],p->weight,p->indent,p->header,p->footer};
    _Static_assert(sizeof values/sizeof *values == INK_MOGO_SETTING_COUNT && sizeof values/sizeof *values == sizeof ink_mogo_setting_names/sizeof *ink_mogo_setting_names,"setting rows");
    return values[index];
}
static bool ink_mogo_setup(const ink_member_identity_t *owner, int w, int h, int px) {
    ink_mogo_owner(owner); ink_mogo_dimensions(w,h,px); ink_mogo_settings_load();
    ink_mogo_page_width=w; ink_mogo_page_height=h; s_reader_session=(reader_session_t){0};
    for (int i=0;i<4;++i) s_margin[i]=ink_mogo_scaled(ink_mogo_settings.margin[i]);
    s_margin[2]+=ink_mogo_settings.header?ink_mogo_scaled(64):0; s_margin[3]+=ink_mogo_settings.footer?ink_mogo_scaled(64):0;
    if (ttf_font_open(app_settings_font_path()) != ESP_OK) return false;
    book_layout_set_spacing(ink_mogo_settings.line,ink_mogo_settings.paragraph);
    book_layout_set_first_line_indent(ink_mogo_settings.indent);
    ttf_set_stroke_adjust_fine(ink_mogo_settings.weight);
    return true;
}
bool ink_mogo_page_open(const char *path,const ink_member_identity_t *owner,int w,int h,int px) {
    return ink_mogo_setup(owner,w,h,px) && open_book_impl(&ink_mogo_context,path);
}
static bool ink_mogo_scan(void) {
    save_progress(); s_view=SHELF; s_reader_panel=READER_PANEL_NONE;
    free(ink_mogo_shelf); ink_mogo_shelf=NULL; ink_mogo_shelf_count=ink_mogo_shelf_page=0;
    uint32_t cursor=0; ink_fs_entry_t e;
    while (ink_fs_index("/me/books",INK_FS_ANY,&cursor,&e)==1) {
        if (!book_file_path_supported(e.path)) continue;
        book_shelf_entry_t *next=realloc(ink_mogo_shelf,(size_t)(ink_mogo_shelf_count+1)*sizeof *next);
        if (!next) return false;
        ink_mogo_shelf=next; book_shelf_entry_t *b=&next[ink_mogo_shelf_count++]; memset(b,0,sizeof *b);
        copy_text(b->path,sizeof b->path,e.path); const char *ink_mogo_leaf_name=strrchr(e.path,'/');
        copy_text(b->name,sizeof b->name,ink_mogo_leaf_name?ink_mogo_leaf_name+1:e.path); b->search_match=true; b->size=(uint32_t)e.size;
        book_progress_t p;
        if (book_progress_load(e.path,b->size,&p)) { b->has_progress=true; b->pct=p.pct; b->recent=p.last_open_s; }
    }
    book_shelf_query_t query={.root="/me/books",.recent_sort=true};
    ink_mogo_shelf_count=book_shelf_sort(ink_mogo_shelf,ink_mogo_shelf_count,&query);
    return true;
}
bool ink_mogo_page_shelf(const ink_member_identity_t *owner,int w,int h,int px) {
    return ink_mogo_setup(owner,w,h,px) && ink_mogo_scan();
}
void ink_mogo_page_turn(int direction) {
    ink_mogo_context.now_ms=ink_now_us()/1000;
    if (s_view==TOC) { int last=book_toc_pages(book_navigation_count())-1; int page=(int)ink_mogo_context.leaf+direction; ink_mogo_context.leaf=(size_t)(page<0?0:page>last?last:page); }
    else if (s_reader_panel==INK_MOGO_MARKS) { int last=bookmark_pages(bookmark_count())-1; int page=s_bookmark_page+direction; s_bookmark_page=page<0?0:page>last?last:page; }
    else if (s_view==SHELF) { int last=ink_mogo_shelf_count?(ink_mogo_shelf_count-1)/4:0; int page=ink_mogo_shelf_page+direction; ink_mogo_shelf_page=page<0?0:page>last?last:page; }
    else (void)turn_page(&ink_mogo_context,direction);
}
static bool ink_mogo_setting(int index,int value) {
    ink_mogo_settings_t old=ink_mogo_settings, *p=&ink_mogo_settings;
    size_t offset=book_layout_page_start_offset(s_reader_session.position.page);
    if (index==0 && value>=18 && value<=90) p->px=(uint8_t)value;
    else if (index==1 && value>=110 && value<=150) p->line=(uint8_t)value;
    else if (index==2 && value>=0 && value<=100) p->paragraph=(uint8_t)value;
    else if (index==3 && value>=0 && value<=96) memset(p->margin,value,sizeof p->margin);
    else if (index==4 && value>=-32 && value<=32) p->weight=(int8_t)value;
    else if (index==5 && value>=0 && value<=3) p->indent=(uint8_t)value;
    else if (index==6 && value>=0 && value<=1) p->header=(uint8_t)value;
    else if (index==7 && value>=0 && value<=1) p->footer=(uint8_t)value;
    else return false;
    bool ok=false;
    for (int attempt=0;attempt<2;++attempt) {
        s_px=app_settings_book_px();
        for (int i=0;i<4;++i) s_margin[i]=ink_mogo_scaled(p->margin[i]);
        s_margin[2]+=p->header?ink_mogo_scaled(64):0; s_margin[3]+=p->footer?ink_mogo_scaled(64):0;
        book_layout_set_spacing(p->line,p->paragraph); book_layout_set_first_line_indent(p->indent); ttf_set_stroke_adjust_fine(p->weight);
        if (load_chapter(&ink_mogo_context,s_reader_session.position.chapter,offset,false) && !attempt && ink_mogo_settings_save()) { ok=true; break; }
        *p=old;
    }
    if (ok) { s_reader_panel=READER_PANEL_NONE; s_view=READING; save_progress(); }
    return ok;
}
bool ink_mogo_page_command(const char *ink_mogo_operation,int value) {
    if (!strcmp(ink_mogo_operation,"mogo.shelf")) return ink_mogo_scan();
    if (!strcmp(ink_mogo_operation,"mogo.shelf_open")) {
        if (value<1 || value>ink_mogo_shelf_count) return false;
        char path[BOOK_STORE_PATH_MAX]; copy_text(path,sizeof path,ink_mogo_shelf[value-1].path);
        return open_book_impl(&ink_mogo_context,path);
    }
    if (!s_text) return false;
    if (!strcmp(ink_mogo_operation,"mogo.next") || !strcmp(ink_mogo_operation,"mogo.previous")) ink_mogo_page_turn(!strcmp(ink_mogo_operation,"mogo.next")?1:-1);
    else if (!strcmp(ink_mogo_operation,"mogo.goto_chapter")) {
        if (value<1 || (size_t)value>book_navigation_count()) return false;
        size_t target=(size_t)value-1, chapter=book_navigation_chapter(target);
        if (!load_chapter_at(&ink_mogo_context,chapter,0,false,book_navigation_anchor(target),book_navigation_source_offset(target))) return false;
        s_selected_toc=target; s_view=READING; s_reader_panel=READER_PANEL_NONE; save_progress();
    } else if (!strcmp(ink_mogo_operation,"mogo.add_bookmark")) { if (!bookmark_is_current() && !bookmark_toggle()) return false; }
    else if (!strcmp(ink_mogo_operation,"mogo.goto_bookmark")) {
        reader_bookmarks_t marks;
        if (!bookmark_load(&marks) || value<1 || value>marks.count) return false;
        reader_bookmark_entry_t *b=&marks.entries[marks.count-value];
        if (!load_chapter(&ink_mogo_context,b->chapter,b->byte_off,false)) return false;
        s_view=READING; s_reader_panel=READER_PANEL_NONE; save_progress();
    } else if (!strcmp(ink_mogo_operation,"mogo.toc")) { s_view=TOC; ink_mogo_context.leaf=current_toc_position()/BOOK_TOC_ROWS; s_reader_panel=READER_PANEL_NONE; }
    else if (!strcmp(ink_mogo_operation,"mogo.bookmarks")) { s_view=READING; s_reader_panel=INK_MOGO_MARKS; s_bookmark_page=0; }
    else if (!strcmp(ink_mogo_operation,"mogo.settings")) { s_view=READING; s_reader_panel=INK_MOGO_SETTINGS; }
    else if (!strcmp(ink_mogo_operation,"mogo.tools")) { s_view=READING; s_reader_panel=INK_MOGO_TOOLS; }
    else if (!strcmp(ink_mogo_operation,"mogo.reader")) { s_view=READING; s_reader_panel=READER_PANEL_NONE; }
    else if (!strcmp(ink_mogo_operation,"mogo.toc_percent")) { ink_mogo_context.leaf=(size_t)book_toc_page_from_percent(value,book_toc_pages(book_navigation_count())); }
    else {
        for (int i=0;i<INK_MOGO_SETTING_COUNT;++i) if (!strcmp(ink_mogo_operation,ink_mogo_setting_actions[i])) return ink_mogo_setting(i,value);
        return false;
    }
    save_progress();
    return !s_save_failed;
}
void ink_mogo_page_draw(uint8_t *pixels) {
    ink_mogo_ui_coordinates(false);
    if (s_text && s_view!=SHELF && s_view!=TOC) draw_reader(pixels,s_reader_session.position.page);
    else ui_clear_page(pixels);
    ink_mogo_ui_coordinates(true);
    if (s_view==READING && s_text) {
        if (ink_mogo_settings.header) ui_text_box(pixels,(EpdRect){36,8,612,64},24,s_reader_session.book.title,EPD_DRAW_ALIGN_CENTER,false);
        if (ink_mogo_settings.footer) { char text[80]; snprintf(text,sizeof text,"%zu / %zu · %u%%",s_reader_session.position.page+1,book_layout_page_count(),percent(s_reader_session.position.page)); ui_text_box(pixels,(EpdRect){36,1130,612,64},24,text,EPD_DRAW_ALIGN_CENTER,false); }
    }
    if (s_view==TOC) book_toc_render(pixels,s_reader_session.book.title,book_navigation_count(),current_toc_position(),(int)ink_mogo_context.leaf,s_message);
    else if (s_view==SHELF) {
        ui_text_box(pixels,(EpdRect){36,80,612,90},42,TR("书架"),EPD_DRAW_ALIGN_CENTER,false);
        for (int row=0;row<INK_MOGO_SHELF_ROWS && ink_mogo_shelf_page*4+row<ink_mogo_shelf_count;++row)
            ui_draw_button(pixels,(EpdRect){36,210+row*190,612,164},ink_mogo_shelf[ink_mogo_shelf_page*4+row].name,false);
        ui_text_box(pixels,(EpdRect){36,1000,612,80},28,TR("‹ 上一页                     下一页 ›"),EPD_DRAW_ALIGN_CENTER,false);
    } else if (s_reader_panel==INK_MOGO_MARKS) draw_reader_bookmarks(pixels);
    else if (s_reader_panel==INK_MOGO_SETTINGS) {
        draw_sheet(pixels,180,TR("阅读设置"));
        ui_draw_button(pixels,(EpdRect){36,280,612,72},TR("返回正文"),false);
        for (int i=0;i<INK_MOGO_SETTING_COUNT;++i) {
            char text[80]; snprintf(text,sizeof text,"-     %s  %d     +",TR(ink_mogo_setting_names[i]),ink_mogo_setting_value(i));
            ui_draw_button(pixels,(EpdRect){36,380+i*94,612,82},text,false);
        }
    } else if (s_reader_panel==INK_MOGO_TOOLS) {
        draw_sheet(pixels,640,TR("阅读工具"));
        const char *labels[]={TR_NOOP("目录"),TR_NOOP("加书签"),TR_NOOP("书签列表"),TR_NOOP("阅读设置"),TR_NOOP("书架"),TR_NOOP("返回正文")};
        for (int i=0;i<INK_MOGO_TOOL_COUNT;++i) ui_draw_button(pixels,(EpdRect){36+(i%2)*318,750+(i/2)*136,294,112},TR(labels[i]),false);
    }
    ink_mogo_ui_coordinates(false);
}
const char *ink_mogo_page_hit(int x,int y,int *value) {
    x=x*684/ink_mogo_page_width; y=y*1216/ink_mogo_page_height;
    *value=0;
    if (s_view==SHELF) {
        if (y>=210 && y<970) { *value=ink_mogo_shelf_page*4+(y-210)/190+1; return "mogo.shelf_open"; }
        if (y>=1000) return x<342?"mogo.previous":"mogo.next";
    } else if (s_view==TOC) {
        int hit=book_toc_hit((uint16_t)x,(uint16_t)y,(int)ink_mogo_context.leaf,book_navigation_count());
        if (hit>=0) { *value=hit+1; return "mogo.goto_chapter"; }
        if (hit==BOOK_TOC_BACK) return "mogo.reader";
        if (hit==BOOK_TOC_PREV || hit==BOOK_TOC_SKIP_PREV) return "mogo.previous";
        if (hit==BOOK_TOC_NEXT || hit==BOOK_TOC_SKIP_NEXT) return "mogo.next";
        if (hit==BOOK_TOC_JUMP) { *value=x*100/684; return "mogo.toc_percent"; }
    } else if (s_reader_panel==INK_MOGO_MARKS) {
        if (y<650) return "mogo.reader";
        if (y>=1120) return x<342?"mogo.previous":"mogo.next";
        for (int i=0;i<bookmark_rows();++i) if (ui_rect_hit(bookmark_row_rect(i),x,y)) { *value=s_bookmark_page*bookmark_rows()+i+1; return "mogo.goto_bookmark"; }
    } else if (s_reader_panel==INK_MOGO_SETTINGS) {
        if (y>=280 && y<352) return "mogo.reader";
        if (y>=380 && y<1132) {
            int i=(y-380)/94, delta=x<342?-1:1, v=ink_mogo_setting_value(i);
            if (i==1 || i==2) delta*=5;
            if (i==3) delta*=12;
            if (i>=6) v=!v; else v+=delta;
            *value=v; return ink_mogo_setting_actions[i];
        }
    } else if (s_reader_panel==INK_MOGO_TOOLS) {
        static const char *const actions[]={"mogo.toc","mogo.add_bookmark","mogo.bookmarks","mogo.settings","mogo.shelf","mogo.reader"};
        if (y>=750 && y<1158) return actions[((y-750)/136)*2+(x>=342)];
    } else return NULL;
    return "";
}
void ink_mogo_page_close(void) {
    save_progress(); free_book(); ink_mogo_font_shutdown(); memset(&s_reader_session,0,sizeof s_reader_session);
    free(ink_mogo_shelf); ink_mogo_shelf=NULL; ink_mogo_shelf_count=ink_mogo_shelf_page=0;
    memset(&ink_mogo_settings,0,sizeof ink_mogo_settings); s_reader_panel=READER_PANEL_NONE; s_reader_slider=-1;
    s_bookmark_page=0; s_bookmark_selected=0; s_bookmark_edit=s_bookmark_delete_confirm=s_bookmark_delete_error=false;
    s_save_failed=false; s_message[0]=s_reader_notice[0]=0;
}
int ink_mogo_page_snapshot(char *out,size_t cap) {
    size_t start=book_layout_page_start_offset(s_reader_session.position.page), end=book_layout_page_start_offset(s_reader_session.position.page+1);
    size_t n=s_text && end>=start?end-start:0;
    if (n>600) n=600;
    while (n && ((unsigned char)s_text[start+n]&0xc0)==0x80) --n;
    const char *ink_mogo_panel_name=s_view==SHELF?TR_NOOP("书架"):s_view==TOC?TR_NOOP("目录"):s_reader_panel==INK_MOGO_MARKS?TR_NOOP("书签列表"):s_reader_panel==INK_MOGO_SETTINGS?TR_NOOP("设置"):s_reader_panel==INK_MOGO_TOOLS?TR_NOOP("工具"):TR_NOOP("正文");
    int len=snprintf(out,cap,TR_NOOP("新阅读 %s 章节=%zu 页=%zu/%zu 位置=%zu 结束=%zu 字号=%d 面板=%s 书签=%zu 章名=%s\n%.*s"),
        s_reader_session.book.title,s_reader_session.position.chapter+1,s_reader_session.position.page+1,book_layout_page_count(),start,end,s_px,ink_mogo_panel_name,bookmark_count(),s_reader_session.book.chapter_title,(int)n,s_text?s_text+start:s_message);
    if (s_view==SHELF && len>=0 && (size_t)len<cap) for (int i=0;i<ink_mogo_shelf_count;++i) {
        int written=snprintf(out+len,cap-(size_t)len,"\n%d %s",i+1,ink_mogo_shelf[i].path);
        if (written<0 || (size_t)written>=cap-(size_t)len) break;
        len+=written;
    }
    return len;
}
