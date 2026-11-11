#pragma once
#include "ink/hal_storage.h"
class HalFile {
 public:
  hal_file_t *file;
  explicit HalFile(hal_file_t *f) : file(f) {}
  ssize_t read(uint8_t *p, size_t n) { return hal_fs_read(file, p, n); }
  ssize_t write(const uint8_t *p, size_t n) { return hal_fs_write(file, p, n); }
};
