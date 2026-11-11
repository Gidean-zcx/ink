#include "book_bookmarks.h"
#ifndef EBOOK_EMULATOR
#include "assets/lang_config.h"
#endif
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <unistd.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#endif

namespace reader {
namespace {
const char* BookmarkText(const char* zh, const char* en) {
    #ifndef EBOOK_EMULATOR
    return std::strcmp(Lang::Current(), "zh-CN") == 0 ? zh : en;
    #else
    (void)zh;
    return en;
    #endif
}

}
uint32_t ink_bookmark_next_id();
bool ink_load_bookmarks(const std::string&,std::vector<BookBookmark>&,std::string&);
bool ink_save_bookmarks(const std::string&,const std::vector<BookBookmark>&,const std::vector<BookBookmark>&,std::string&);
void BookBookmarkStore::Load(const std::string& book_path,uint64_t size,int64_t mtime,uint32_t format) {
    path_=book_path;file_size_=size;mtime_=mtime;format_=format;entries_.clear();error_.clear();
    try { ink_load_bookmarks(path_,entries_,error_); }
    catch(const std::bad_alloc&) { entries_.clear();error_=BookmarkText("内存不足，无法读取书签", "Not enough memory to read bookmarks"); }
}
bool BookBookmarkStore::Save(const std::vector<BookBookmark>& entries,std::string& error) {
    if(!error_.empty()) { error=error_;return false; }
    return ink_save_bookmarks(path_,entries,entries_,error);
}
bool BookBookmarkStore::Add(BookBookmark entry,std::string& error) {
    try {
        if(!SourceBefore(entry.start,entry.end) || entry.excerpt.size()>kMaxExcerptBytes) { error=BookmarkText("无有效正文位置", "No valid text position");return false; }
        if(entries_.size()>=kMaxBookmarks) { error=BookmarkText("书签已满（最多256条）", "Bookmark limit reached (256)");return false; }
        uint32_t last=0;for(const auto& other:entries_)last=std::max(last,other.id);
        if(last==std::numeric_limits<uint32_t>::max()) { error=BookmarkText("书签编号已满", "No more bookmark IDs available");return false; }
        entry.id=ink_bookmark_next_id();auto updated=entries_;updated.push_back(std::move(entry));
        std::sort(updated.begin(),updated.end(),[](const BookBookmark& a,const BookBookmark& b){return a.id>b.id;});
        if(!Save(updated,error))return false;
        entries_.swap(updated);return true;
    } catch(const std::bad_alloc&) { error=BookmarkText("内存不足，无法保存书签", "Not enough memory to save bookmarks");return false; }
}
bool BookBookmarkStore::Remove(const std::vector<uint32_t>& ids,std::string& error) {
    try {
        auto updated=entries_;
        updated.erase(std::remove_if(updated.begin(),updated.end(),[&](const BookBookmark& entry){
            return std::find(ids.begin(),ids.end(),entry.id)!=ids.end();}),updated.end());
        if(!Save(updated,error))return false;
        entries_.swap(updated);return true;
    } catch(const std::bad_alloc&) { error=BookmarkText("内存不足，无法移除书签", "Not enough memory to remove bookmarks");return false; }
}
} // namespace reader
