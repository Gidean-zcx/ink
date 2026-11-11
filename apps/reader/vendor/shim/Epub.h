#pragma once
#include <string>
/* OPF discovery runs with cache=nullptr; disk-cache branches are unreachable. */
struct HalFile { explicit operator bool() const { return false; }
    void close() {} size_t position() const { return 0; } void seek(size_t) {} int available() const { return 0; }
};
struct InkUnusedStorage {
    bool exists(const char *) { return false; } void remove(const char *) {}
    bool openFileForWrite(const char *, const std::string&, HalFile&) { return false; }
    bool openFileForRead(const char *, const std::string&, HalFile&) { return false; }
};
inline InkUnusedStorage Storage;
