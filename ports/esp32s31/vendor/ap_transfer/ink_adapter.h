/* HAL adaptation of CloudZao/Metalio-E-INK4-Plus@874538e (MIT). */
#pragma once
#include "ink/hal_transfer.h"
#include <esp_http_server.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdlib>
extern "C" void ink_transfer_wifi_pause(void);
extern "C" void ink_transfer_wifi_restore(bool);
static const hal_transfer_policy_t *ink_policy;
#define SD_FILE_NAME_CAPACITY 64
#define SD_MOUNT_POINT ""
typedef uint64_t FSIZE_t;
static int64_t InkIo(hal_transfer_io_t &io) {
    int64_t r = ink_policy->file(&io);
    if (r < 0) errno = r == -3 ? ENOENT : r == -4 ? EEXIST : r == -7 ? ENOSPC : r == -2 ? EACCES : EIO;
    else errno = 0;
    return r;
}
static void *InkOpen(const char *path, const char *mode) {
    hal_transfer_io_t io{}; io.op = HAL_TRANSFER_OPEN; io.path = path; io.value = *mode == 'w';
    return InkIo(io) ? nullptr : io.handle;
}
static size_t InkRead(void *data, size_t unit, size_t n, void *file) {
    hal_transfer_io_t io{}; io.op = HAL_TRANSFER_READ; io.handle = file; io.data = data; io.len = unit * n;
    int64_t r = InkIo(io); return r < 0 ? 0 : static_cast<size_t>(r) / unit;
}
static size_t InkWrite(const void *data, size_t unit, size_t n, void *file) {
    hal_transfer_io_t io{}; io.op = HAL_TRANSFER_WRITE; io.handle = file; io.data = const_cast<void *>(data); io.len = unit * n;
    int64_t r = InkIo(io); return r < 0 ? 0 : static_cast<size_t>(r) / unit;
}
static int InkFile(void *file, int op) { hal_transfer_io_t io{}; io.op = op; io.handle = file; return InkIo(io); }
static int InkStat(const char *path, struct stat *st) {
    hal_transfer_io_t io{}; io.op = HAL_TRANSFER_STAT; io.path = path;
    int r = InkIo(io); if (!r) { std::memset(st, 0, sizeof *st); st->st_size = io.value; st->st_mode = io.directory ? S_IFDIR : S_IFREG; } return r;
}
static int InkPath(const char *path, int op, const char *to = nullptr) {
    hal_transfer_io_t io{}; io.op = op; io.path = path; io.to = to; return InkIo(io);
}
struct InkDir { void *handle; struct dirent entry; };
static InkDir *InkOpenDir(const char *path) {
    hal_transfer_io_t io{}; io.op = HAL_TRANSFER_DIR_OPEN; io.path = path;
    if (InkIo(io)) return nullptr;
    auto *dir = static_cast<InkDir *>(heap_caps_malloc(sizeof(InkDir), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!dir) { InkFile(io.handle, HAL_TRANSFER_DIR_CLOSE); errno = ENOMEM; return nullptr; }
    dir->handle = io.handle; return dir;
}
static struct dirent *InkReadDir(InkDir *dir) {
    hal_transfer_io_t io{}; io.op = HAL_TRANSFER_DIR_NEXT; io.handle = dir->handle;
    if (InkIo(io) != 1) return nullptr;
    std::snprintf(dir->entry.d_name, sizeof dir->entry.d_name, "%s", io.name); return &dir->entry;
}
static int InkCloseDir(InkDir *dir) { int r = InkFile(dir->handle, HAL_TRANSFER_DIR_CLOSE); heap_caps_free(dir); return r; }
static esp_err_t InkSpace(const char *, uint64_t *total, uint64_t *free) {
    hal_transfer_io_t io{}; io.op = HAL_TRANSFER_SPACE; int r = InkIo(io); *total = io.total; *free = io.free_bytes; return r ? ESP_FAIL : ESP_OK;
}
static bool SdIsValidFileName(const char *name) {
    return name && *name && *name != '.' && std::strlen(name) < SD_FILE_NAME_CAPACITY - 9 && !std::strpbrk(name, "/\\:*?\"<>|");
}
struct SdCardManager { static SdCardManager &GetInstance() { static SdCardManager s; return s; }
    bool IsMounted() { uint64_t total, free; return InkSpace("", &total, &free) == ESP_OK; } };
namespace reader { inline void InvalidateBookLibraryCache() {} }
struct WifiStation { static WifiStation &GetInstance() { static WifiStation s; return s; }
    void PauseForLp() { ink_transfer_wifi_pause(); } bool IsLpPaused() { return true; } };
struct PowerPolicy { static PowerPolicy &GetInstance() { static PowerPolicy s; return s; }
    void Acquire(int) {} void Release(int) {} };
namespace PowerNeed { constexpr int UiKeepNet = 0; }
constexpr int8_t kMetalioWifiMaxTxPower = 48;
static bool InkAuthorize(httpd_req_t *req, char token[33]) {
    char code[8] = "", cookie[96] = "", issued[33] = "";
    httpd_req_get_hdr_value_str(req, "X-Ink-Code", code, sizeof code);
    httpd_req_get_hdr_value_str(req, "Cookie", cookie, sizeof cookie);
    const char *value = std::strstr(cookie, "ink_transfer=");
    if (value && (value == cookie || value[-1] == ' ' || value[-1] == ';') && std::strlen(value + 13) >= 32) {
        std::memcpy(token, value + 13, 32); token[32] = 0;
    }
    if (!ink_policy->authorize(code, token, issued)) return false;
    if (*issued) {
        static char header[96]; std::snprintf(header, sizeof header, "ink_transfer=%s; HttpOnly; SameSite=Strict; Path=/", issued);
        httpd_resp_set_hdr(req, "Set-Cookie", header); std::memcpy(token, issued, 33);
    }
    return true;
}
static const char InkLogin[] = "<!doctype html><meta charset=utf-8><title>无线传文件</title><form><label>授权码<input id=c inputmode=numeric pattern='[0-9]{6}' required></label><button>打开文件</button></form><p id=r></p><script>async function login(){let x=await fetch('/',{headers:{'X-Ink-Code':c.value}});if(x.ok){location.replace('/?path='+encodeURIComponent('/收件箱'))}else r.textContent='授权码不正确或已失效'}document.querySelector('form').onsubmit=e=>{e.preventDefault();login()};c.value=new URLSearchParams(location.hash.slice(1)).get('code')||'';history.replaceState(null,'',location.pathname);if(c.value)login();</script>";
