#include <vendor/metalio_bookmarks/book_bookmarks.h>
#include <cassert>
#include <cstdio>

namespace reader {
static uint32_t ink_id;
uint32_t ink_bookmark_next_id() { return ++ink_id; }
bool ink_load_bookmarks(const std::string&,std::vector<BookBookmark>&,std::string&) { return true; }
bool ink_save_bookmarks(const std::string&,const std::vector<BookBookmark>&,const std::vector<BookBookmark>&,std::string&) { return true; }
}
int main() {
    using namespace reader;
    BookBookmarkStore store; std::string error; store.Load("synthetic.txt",0,0,0);
    BookBookmark entry; entry.end.offset=1; entry.excerpt.assign(BookBookmarkStore::kMaxExcerptBytes,'x');
    for (size_t i=0;i<BookBookmarkStore::kMaxBookmarks;i++) assert(store.Add(entry,error));
    assert(store.Entries().size()==256 && !store.Add(entry,error));
    assert(store.Remove({store.Entries()[0].id},error));
    entry.excerpt+='x'; assert(!store.Add(entry,error));
    entry.excerpt="我们看着灯塔。"; assert(store.Add(entry,error));
    assert(store.Entries().front().excerpt==entry.excerpt);
    assert(SourceInPage({1,3,9},{1,3,9},{1,5,0}));
    assert(!SourceInPage({1,5,0},{1,3,9},{1,5,0}));
    assert(!SourceInPage({0,3,9},{1,3,9},{1,5,0}));
    std::puts("bookmark model: 256 entries, 768 bytes, removal and source boundaries pass");
}
