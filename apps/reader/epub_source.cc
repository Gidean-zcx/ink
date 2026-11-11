#include <reader_source.h>
#include <reader_runtime.h>
#include <ink/sys/app.h>
#include <epub_document.h>
#include <html_content.h>
#include <text_encoding.h>
#include <txt_chapter.h>
#include <ContainerParser.h>
#include <ContentOpfParser.h>
#include <TocNavParser.h>
#include <TocNcxParser.h>
#include <Epub/BookMetadataCache.h>
#include <algorithm>
#include <cstring>
#include <new>

struct ink_reader_source {
    reader::EpubDocument epub;
    std::string path;
    std::vector<ink_reader_entry_t> toc;
    ink_reader_info_t info{};
};
static void ink_copy(char *out, size_t cap, const std::string& text) {
    size_t n = std::min(cap - 1, text.size());
    while (n && n < text.size() && (static_cast<unsigned char>(text[n]) & 0xc0) == 0x80) --n;
    memcpy(out, text.data(), n); out[n] = 0;
}
static ink_reader_error_t ink_error(reader::BookReadError e) {
    if (e == reader::BookReadError::kFileIo) return INK_READER_IO;
    if (e == reader::BookReadError::kUnsupported) return INK_READER_ENCRYPTED;
    if (e == reader::BookReadError::kNoMemory) return INK_READER_MEMORY;
    return INK_READER_CORRUPT;
}
template<class Parser> static bool ink_parse(reader::ZipReader& zip, const std::string& path, Parser& p) {
    if (!p.setup() || !zip.OpenEntryStream(path.c_str())) return false;
    uint8_t buf[1024]; bool ok = true;
    while (!zip.EntryStreamEof() && ok) {
        size_t n = zip.EntryStreamRead(buf, sizeof buf);
        ok = n ? p.write(buf, n) == n : zip.EntryStreamEof();
    }
    ok = ok && !zip.EntryStreamError(); zip.CloseEntryStream(); return ok;
}
/* Expat binds an existing TOC fragment to the byte position; Metalio converts its prefix. */
static uint64_t ink_anchor(reader::ZipReader& zip, const std::string& href, const std::string& anchor) {
    if (anchor.empty()) return 0;
    struct Match { const std::string *id; XML_Parser p; int64_t byte = -1; } m{&anchor, XML_ParserCreate(nullptr)};
    if (!m.p || !zip.OpenEntryStream(href.c_str())) { XML_ParserFree(m.p); return 0; }
    XML_SetHashSalt(m.p, 1); XML_SetUserData(m.p, &m);
    XML_SetStartElementHandler(m.p, [](void *ctx, const char *, const char **attrs) {
        auto *a = static_cast<Match *>(ctx);
        for (int i = 0; attrs[i]; i += 2)
            if ((!strcmp(attrs[i], "id") || !strcmp(attrs[i], "xml:id")) && *a->id == attrs[i+1]) {
                a->byte = XML_GetCurrentByteIndex(a->p); XML_StopParser(a->p, XML_FALSE); break;
            }
    });
    uint8_t buf[1024];
    while (!zip.EntryStreamEof() && m.byte < 0) {
        size_t n = zip.EntryStreamRead(buf, sizeof buf);
        if (!n || XML_Parse(m.p, reinterpret_cast<const char *>(buf), static_cast<int>(n), false) == XML_STATUS_ERROR) break;
    }
    zip.CloseEntryStream(); XML_ParserFree(m.p);
    if (m.byte < 0 || !zip.OpenEntryStream(href.c_str())) return 0;
    uint64_t length = 0;
    reader::HtmlBlockParser prefix("", [&](reader::ContentBlock&& b) {
        length += b.kind == reader::ContentKind::kImage ? strlen("［图片］\n") : b.text.size() + !b.continues; return true;
    });
    while (m.byte > 0) {
        size_t n = zip.EntryStreamRead(buf, std::min(static_cast<size_t>(m.byte), sizeof buf));
        if (!n || !prefix.Feed(buf, n)) break;
        m.byte -= static_cast<int64_t>(n);
    }
    prefix.Finish(); zip.CloseEntryStream(); return length;
}
static bool ink_txt(ink_reader_source *s, const reader::ContentBlockSink& sink) {
    FILE *f = std::fopen(s->path.c_str(), "rb");
    if (!f) return false;
    uint8_t buf[4096 + 4]; size_t carry = 0; bool first = true, gb = false, ok = true;
    do {
        size_t n = std::fread(buf + carry, 1, 4096, f), total = n + carry, used = 0;
        if (first) { gb = !reader::LooksLikeUtf8(buf, total); first = false; }
        if (n) {
            while (used + 4 <= total) { uint32_t cp; used += gb ? reader::Gb18030Next(buf+used, total-used, &cp) : reader::Utf8Next(buf+used, total-used, &cp); }
        } else used = total;
        reader::ContentBlock b;
        reader::BytesToUtf8(buf, used, gb, b.text); b.continues = true;
        ok = sink(std::move(b)); carry = total - used; memmove(buf, buf + used, carry);
        if (!n) break;
    } while (ok);
    bool error = std::ferror(f); std::fclose(f); return ok && !error;
}
extern "C" ink_reader_error_t ink_reader_source_open(const char *path, ink_reader_source_t **out, ink_reader_info_t *info) {
    struct Args { const char *path; ink_reader_source_t **out; ink_reader_info_t *info; ink_reader_error_t error; } a{path,out,info,INK_READER_OK};
    ink_reader_host_call([](void *ctx) {
        auto& q = *static_cast<Args *>(ctx); *q.out = nullptr;
        auto *s = new(std::nothrow) ink_reader_source;
        if (!s) { q.error = INK_READER_MEMORY; return; }
        s->path = q.path; auto dot = s->path.find_last_of('.');
        std::string ext = dot == std::string::npos ? "" : s->path.substr(dot);
        for (auto& c : ext) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        s->info.epub = ext == ".epub"; s->info.chapters = 1;
        auto slash = s->path.find_last_of('/');
        ink_copy(s->info.title, sizeof s->info.title, s->path.substr(slash+1, dot-slash-1));
        if (ext == ".txt") {
            uint64_t offset = 0, start = 0; std::string line;
            bool ok = ink_txt(s, [&](reader::ContentBlock&& b) {
                for (char c : b.text) {
                    if (c == '\n') {
                        if (reader::IsTxtChapterTitleLine(line)) { ink_reader_entry_t e{}; ink_copy(e.title,sizeof e.title,line); e.offset = start; s->toc.push_back(e); }
                        line.clear(); start = offset + 1;
                    } else if (line.size() <= reader::kMaxTxtChapterTitleBytes) line += c;
                    ++offset;
                } return true;
            });
            if (reader::IsTxtChapterTitleLine(line)) { ink_reader_entry_t e{}; ink_copy(e.title,sizeof e.title,line); e.offset = start; s->toc.push_back(e); }
            if (!ok) q.error = INK_READER_IO;
        } else if (!s->info.epub) q.error = INK_READER_FORMAT;
        else {
            reader::ZipReader zip; std::vector<uint8_t> mime;
            if (!zip.Open(q.path)) q.error = ink_error(zip.Error());
            else if (!zip.OpenEntryStream("mimetype")) q.error = zip.Error() == reader::BookReadError::kUnsupported ? INK_READER_ENCRYPTED : INK_READER_FORMAT;
            else { zip.CloseEntryStream(); if (!zip.ReadEntry("mimetype",mime,128)) q.error = INK_READER_FORMAT; }
            if (q.error != INK_READER_OK) { delete s; return; }
            if (std::string(mime.begin(),mime.end()) != "application/epub+zip") q.error = INK_READER_FORMAT;
            else if (!s->epub.Open(q.path)) q.error = INK_READER_CORRUPT;
            else {
                ink_copy(s->info.title,sizeof s->info.title,s->epub.Title()); ink_copy(s->info.author,sizeof s->info.author,s->epub.Author());
                s->info.chapters = s->epub.SpineCount();
                size_t size = 0; zip.GetUncompressedSize("META-INF/container.xml", &size); ContainerParser container(size);
                if (ink_parse(zip,"META-INF/container.xml",container)) {
                    std::string base = container.fullPath.substr(0,container.fullPath.find_last_of('/')+1), cache;
                    zip.GetUncompressedSize(container.fullPath.c_str(),&size); ContentOpfParser opf(cache,base,size,nullptr);
                    if (ink_parse(zip,container.fullPath,opf)) {
                        BookMetadataCache toc;
                        auto parse_toc = [&](const std::string& path, bool nav) {
                            if (path.empty() || !zip.GetUncompressedSize(path.c_str(),&size)) return false;
                            std::string dir = path.substr(0,path.find_last_of('/')+1);
                            if (nav) { TocNavParser p(dir,size,&toc); return ink_parse(zip,path,p); }
                            TocNcxParser p(dir,size,&toc); return ink_parse(zip,path,p);
                        };
                        if (!parse_toc(opf.tocNavPath,true)) { toc.entries.clear(); parse_toc(opf.tocNcxPath,false); }
                        for (const auto& t : toc.entries) for (int c = 0; c < s->info.chapters; ++c)
                            if (t.href == s->epub.SpineItem(c)->href) {
                                ink_reader_entry_t e{}; ink_copy(e.title,sizeof e.title,t.title); e.chapter = c; e.offset = ink_anchor(zip,t.href,t.anchor); s->toc.push_back(e); break;
                            }
                    }
                }
            }
        }
        s->info.fallback = s->toc.empty();
        if (q.error == INK_READER_OK && s->info.fallback) for (int c = 0; c < s->info.chapters; ++c) { ink_reader_entry_t e{}; e.chapter=c; ink_copy(e.title,sizeof e.title,s->info.epub ? s->epub.SpineItem(c)->href : s->info.title); s->toc.push_back(e); }
        s->info.entries = static_cast<int>(s->toc.size());
        if (q.error != INK_READER_OK) { delete s; return; }
        *q.info = s->info; *q.out = s;
    }, &a); return a.error;
}
extern "C" int ink_reader_source_toc(ink_reader_source_t *s, int i, ink_reader_entry_t *e) {
    if (!s || i < 0 || static_cast<size_t>(i) >= s->toc.size()) return -1;
    *e = s->toc[static_cast<size_t>(i)]; return 0;
}
extern "C" ink_reader_error_t ink_reader_source_read(ink_reader_source_t *s, int chapter, uint64_t offset,
    char *out, size_t cap, size_t *length, bool *more, ink_reader_cancel_fn cancel, void *ctx) {
    if (!s || chapter < 0 || chapter >= s->info.chapters || cap < 8) return INK_READER_FORMAT;
    struct Args { ink_reader_source_t *s; int chapter; uint64_t offset; char *out; size_t cap, n; bool more; ink_reader_cancel_fn cancel; void *ctx; ink_reader_error_t error; } a{s,chapter,offset,out,cap,0,false,cancel,ctx,INK_READER_OK};
    ink_reader_host_call([](void *arg) {
        auto& q = *static_cast<Args *>(arg); uint64_t pos = 0; size_t paragraph = 0;
        bool stopped = false, cancelled = false;
        auto sink = [&](reader::ContentBlock&& b) {
            if (q.cancel && q.cancel(q.ctx)) { cancelled = true; return false; }
            std::string text = b.kind == reader::ContentKind::kImage ? "［图片］\n" : b.text + (b.continues ? "" : "\n");
            for (char c : text) {
                if (pos++ < q.offset) continue;
                if (q.n == q.cap - 1) {
                    if (paragraph) q.n = paragraph;
                    else { q.out[q.n] = c; while (q.n && (static_cast<unsigned char>(q.out[q.n]) & 0xc0) == 0x80) --q.n; }
                    stopped = true; return false;
                }
                q.out[q.n++] = c;
                if (c == '\n') paragraph = q.n;
            } return true;
        };
        bool ok = q.s->info.epub ? q.s->epub.StreamChapterBlocks(q.chapter,sink) : ink_txt(q.s,sink);
        q.out[q.n] = 0; q.more = stopped;
        if (cancelled) q.error = INK_READER_CANCELLED;
        else if (!ok && !stopped) q.error = q.s->info.epub ? ink_error(q.s->epub.Error()) : INK_READER_IO;
    }, &a); *length = a.n; *more = a.more; return a.error;
}
extern "C" void ink_reader_source_close(ink_reader_source_t *s) {
    ink_reader_host_call([](void *p) { delete static_cast<ink_reader_source *>(p); }, s);
}
