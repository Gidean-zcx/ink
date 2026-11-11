#include "bookmarks.h"
#include "vendor/metalio_bookmarks/book_bookmarks.h"
#include <reader_runtime.h>
#include <ink/sys/app.h>
#include <ink/sys/data.h>
#include <ink/sys/json.h>
#include <algorithm>
#include <cstring>

static const ink_db_def_t ink_bookmark_def={"reader.bookmarks","阅读书签",INK_DB_MEMBER,1,true,true,false,0,0,false};
static ink_db_t *ink_bookmark_db;
static reader::BookBookmarkStore ink_bookmark_store;
enum { INK_BOOKMARK_FIELDS = 9 };
static struct { char text[6144], book[160], excerpt[INK_BOOKMARK_EXCERPT+1]; ink_json_t json; } ink_scratch INK_PSRAM_BSS;
static const char *const ink_fields[]={"id","chapter","block","offset","end_chapter","end_block","end_offset","page","added_at"};
static_assert(sizeof ink_fields/sizeof *ink_fields==INK_BOOKMARK_FIELDS,"bookmark fields fit");
namespace reader {
uint32_t ink_bookmark_next_id() { return ink_db_new_id(ink_bookmark_db); }
bool ink_load_bookmarks(const std::string& path,std::vector<BookBookmark>& entries,std::string& error) {
    ink_db_cursor_t cursor={}; ink_rec_t record;
    while (ink_db_next(ink_bookmark_db,&cursor,&record,ink_scratch.text,sizeof ink_scratch.text-1)==1) {
        if (record.len>=sizeof ink_scratch.text || ink_json_parse(&ink_scratch.json,ink_scratch.text,record.len)) { error="Invalid bookmark record"; return false; }
        auto& j=ink_scratch.json;
        if (ink_json_str(&j,ink_json_find(&j,0,"book"),ink_scratch.book,sizeof ink_scratch.book)<0 || path!=ink_scratch.book) continue;
        int64_t v[INK_BOOKMARK_FIELDS]={};
        for (size_t i=0;i<INK_BOOKMARK_FIELDS;i++) if (!ink_json_int(&j,ink_json_find(&j,0,ink_fields[i]),&v[i])) { error="Invalid bookmark position"; return false; }
        BookBookmark entry; entry.id=static_cast<uint32_t>(v[0]); entry.start={uint32_t(v[1]),uint32_t(v[2]),uint32_t(v[3])};
        entry.end={uint32_t(v[4]),uint32_t(v[5]),uint32_t(v[6])}; entry.page=uint32_t(v[7]); entry.added_at=v[8];
        if (!entry.id || !SourceBefore(entry.start,entry.end) || entries.size()>=BookBookmarkStore::kMaxBookmarks ||
            ink_json_str(&j,ink_json_find(&j,0,"excerpt"),ink_scratch.excerpt,sizeof ink_scratch.excerpt)<0) { error="Invalid bookmark"; return false; }
        entry.excerpt=ink_scratch.excerpt; entries.push_back(std::move(entry));
    }
    std::sort(entries.begin(),entries.end(),[](const BookBookmark& a,const BookBookmark& b){ return a.id>b.id; }); return true;
}
bool ink_save_bookmarks(const std::string& path,const std::vector<BookBookmark>& entries,const std::vector<BookBookmark>& previous,std::string& error) {
    for (const auto& old:previous) if (std::none_of(entries.begin(),entries.end(),[&](const BookBookmark& e){return e.id==old.id;})) {
        if (ink_db_del(ink_bookmark_db,old.id,0)) { error="Cannot remove bookmark"; return false; }
    }
    for (const auto& e:entries) if (std::none_of(previous.begin(),previous.end(),[&](const BookBookmark& old){return old.id==e.id;})) {
        ink_jw_t w; ink_jw_init(&w,ink_scratch.text,sizeof ink_scratch.text); ink_jw_raw(&w,"{\"book\":"); ink_jw_str(&w,path.c_str());
        const int64_t values[]={e.id,e.start.chapter,e.start.block,e.start.offset,e.end.chapter,e.end.block,e.end.offset,e.page,e.added_at};
        static_assert(sizeof values/sizeof *values==sizeof ink_fields/sizeof *ink_fields,"bookmark values fit");
        for (size_t i=0;i<INK_BOOKMARK_FIELDS;i++) { ink_jw_raw(&w,","); ink_jw_str(&w,ink_fields[i]); ink_jw_raw(&w,":"); ink_jw_int(&w,values[i]); }
        ink_jw_raw(&w,",\"excerpt\":"); ink_jw_str(&w,e.excerpt.c_str()); ink_jw_raw(&w,"}");
        ink_db_write_t write={}; write.id=e.id; write.sort=e.id; write.data=w.buf; write.len=w.len;
        if (w.overflow || ink_db_put(ink_bookmark_db,&write,nullptr)) { error="Cannot save bookmark"; return false; }
    }
    return true;
}
}
extern "C" bool ink_bookmarks_open(const char *member,const char *book) {
    if (!ink_db_find_def(ink_bookmark_def.name) && ink_db_register(&ink_bookmark_def)) return false;
    ink_bookmark_db=ink_db_open_member(&ink_bookmark_def,member);
    if (!ink_bookmark_db) return false;
    ink_reader_host_call([](void *p){ ink_bookmark_store.Load(static_cast<const char *>(p),0,0,0); },const_cast<char *>(book));
    return ink_bookmark_store.Error().empty();
}
extern "C" void ink_bookmarks_close(void) {
    ink_reader_host_call([](void *){ ink_bookmark_store=reader::BookBookmarkStore(); },nullptr);
    ink_db_close(ink_bookmark_db); ink_bookmark_db=nullptr;
}
extern "C" int ink_bookmarks_count(void) { return static_cast<int>(ink_bookmark_store.Entries().size()); }
extern "C" bool ink_bookmarks_get(int index,ink_bookmark_t *out) {
    const auto& entries=ink_bookmark_store.Entries(); if (index<0 || static_cast<size_t>(index)>=entries.size()) return false;
    const auto& e=entries[static_cast<size_t>(index)]; out->id=e.id; out->page=e.page; out->added_at=e.added_at;
    out->start={e.start.chapter,e.start.block,e.start.offset}; out->end={e.end.chapter,e.end.block,e.end.offset};
    memcpy(out->excerpt,e.excerpt.c_str(),e.excerpt.size()+1); return true;
}
extern "C" bool ink_bookmarks_add(const ink_bookmark_t *entry) {
    struct Args { const ink_bookmark_t *entry; bool ok; } a{entry,false};
    ink_reader_host_call([](void *p){ auto& q=*static_cast<Args *>(p); const auto& e=*q.entry;
        reader::BookBookmark b; b.start={e.start.chapter,e.start.block,e.start.offset}; b.end={e.end.chapter,e.end.block,e.end.offset};
        b.page=e.page; b.added_at=e.added_at; b.excerpt=e.excerpt; std::string error; q.ok=ink_bookmark_db && ink_bookmark_store.Add(std::move(b),error);
    },&a); return a.ok;
}
extern "C" bool ink_bookmarks_remove(uint32_t id) {
    struct Args { uint32_t id; bool ok; } a{id,false};
    ink_reader_host_call([](void *p){ auto& q=*static_cast<Args *>(p); std::string error;
        q.ok=ink_bookmark_db && ink_bookmark_store.Remove({q.id},error); },&a); return a.ok;
}
extern "C" uint32_t ink_bookmarks_in_page(ink_reader_position_t first,ink_reader_position_t end) {
    for (const auto& e:ink_bookmark_store.Entries())
        if (reader::SourceInPage(e.start,{first.chapter,first.block,first.offset},{end.chapter,end.block,end.offset})) return e.id;
    return 0;
}
