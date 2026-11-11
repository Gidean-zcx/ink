#pragma once
#include <string>
#include <vector>
struct InkTocLink { std::string title, href, anchor; };
class BookMetadataCache { public:
    std::vector<InkTocLink> entries;
    void createTocEntry(const std::string& t, const std::string& h, const std::string& a, unsigned) { entries.push_back({t,h,a}); }
    void createSpineEntry(const std::string&) {}
};
