#pragma once
#include <atomic>
#include <reader_types.h>
namespace reader {
inline bool DecodeZipEntryImageToL8(ZipReader&, const char *, int, int, RasterImage&, const std::atomic<bool> *) { return false; }
}
