#include "archive_favorites.h"
#include "archive_files.h"
#include "archive_apps.h"
#include "archive_browser.h"

#define ARCHIVE_FAV_FILE_BUF_LEN 32

static bool archive_favorites_read_line(File* file, FuriString* str_result) {
    furi_string_reset(str_result);
    uint8_t buffer[ARCHIVE_FAV_FILE_BUF_LEN];
    bool result = false;

    do {
        size_t read_count = storage_file_read(file, buffer, ARCHIVE_FAV_FILE_BUF_LEN);
        if(storage_file_get_error(file) != FSE_OK) {
            return false;
        }

        for(size_t i = 0; i < read_count; i++) {
            if(buffer[i] == '\n') {
                uint32_t position = storage_file_tell(file);
                if(storage_file_get_error(file) != FSE_OK) {
                    return false;
                }

                position = position - read_count + i + 1;

                storage_file_seek(file, position, true);
                if(storage_file_get_error(file) != FSE_OK) {
                    return false;
                }

                result = true;
                break;
            } else {
                furi_string_push_back(str_result, buffer[i]);
            }
        }

        if(result || read_count == 0) {
            break;
        }
    } while(true);

    return result;
}

// swap the rebuilt list in only once it is on the card
static void archive_favorites_commit_temp(Storage* storage, bool list_is_empty) {
    if(storage_common_exists(storage, ARCHIVE_FAV_TEMP_PATH)) {
        storage_common_remove(storage, ARCHIVE_FAV_PATH);
        storage_common_rename(storage, ARCHIVE_FAV_TEMP_PATH, ARCHIVE_FAV_PATH);
        storage_common_remove(storage, ARCHIVE_FAV_TEMP_PATH);
    } else if(list_is_empty) {
        storage_common_remove(storage, ARCHIVE_FAV_PATH);
    }
}

uint16_t archive_favorites_count(void* context) {
    furi_assert(context);

    Storage* fs_api = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(fs_api);

    FuriString* buffer;
    buffer = furi_string_alloc();

    bool result = storage_file_open(file, ARCHIVE_FAV_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    uint16_t lines = 0;

    if(result) {
        while(1) {
            if(!archive_favorites_read_line(file, buffer)) {
                break;
            }
            if(!furi_string_size(buffer)) {
                continue; // Skip empty lines
            }
            ++lines;
        }
    }

    storage_file_close(file);

    furi_string_free(buffer);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return lines;
}

static bool archive_favourites_rescan(void) {
    FuriString* buffer;
    buffer = furi_string_alloc();
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    storage_common_remove(storage, ARCHIVE_FAV_TEMP_PATH);
    bool result = storage_file_open(file, ARCHIVE_FAV_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    uint16_t kept = 0;
    if(result) {
        while(1) {
            if(!archive_favorites_read_line(file, buffer)) {
                break;
            }
            if(!furi_string_size(buffer)) {
                continue;
            }

            if(furi_string_search(buffer, "/app:") == 0) {
                if(archive_app_is_available(NULL, furi_string_get_cstr(buffer))) {
                    archive_file_append(
                        ARCHIVE_FAV_TEMP_PATH, "%s\n", furi_string_get_cstr(buffer));
                    kept++;
                }
            } else {
                if(storage_file_exists(storage, furi_string_get_cstr(buffer))) {
                    archive_file_append(
                        ARCHIVE_FAV_TEMP_PATH, "%s\n", furi_string_get_cstr(buffer));
                    kept++;
                }
            }
        }
    }

    furi_string_free(buffer);

    storage_file_close(file);
    archive_favorites_commit_temp(storage, kept == 0);

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return result;
}

bool archive_favorites_delete(const char* format, ...) {
    FuriString* buffer;
    FuriString* filename;
    va_list args;
    va_start(args, format);
    filename = furi_string_alloc_vprintf(format, args);
    va_end(args);

    buffer = furi_string_alloc();
    Storage* fs_api = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(fs_api);

    storage_common_remove(fs_api, ARCHIVE_FAV_TEMP_PATH);
    bool result = storage_file_open(file, ARCHIVE_FAV_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    uint16_t kept = 0;

    if(result) {
        while(1) {
            if(!archive_favorites_read_line(file, buffer)) {
                break;
            }
            if(!furi_string_size(buffer)) {
                continue;
            }

            if(furi_string_search(buffer, filename)) {
                archive_file_append(ARCHIVE_FAV_TEMP_PATH, "%s\n", furi_string_get_cstr(buffer));
                kept++;
            }
        }
    }

    furi_string_free(buffer);
    furi_string_free(filename);

    storage_file_close(file);
    archive_favorites_commit_temp(fs_api, kept == 0);

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return result;
}

bool archive_is_favorite(const char* format, ...) {
    FuriString* buffer;
    FuriString* filename;
    va_list args;
    va_start(args, format);
    filename = furi_string_alloc_vprintf(format, args);
    va_end(args);

    buffer = furi_string_alloc();
    Storage* fs_api = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(fs_api);

    bool found = false;
    bool result = storage_file_open(file, ARCHIVE_FAV_PATH, FSAM_READ, FSOM_OPEN_EXISTING);

    if(result) {
        while(1) {
            if(!archive_favorites_read_line(file, buffer)) {
                break;
            }
            if(!furi_string_size(buffer)) {
                continue;
            }
            if(!furi_string_search(buffer, filename)) {
                found = true;
                break;
            }
        }
    }

    storage_file_close(file);
    furi_string_free(buffer);
    furi_string_free(filename);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return found;
}

bool archive_favorites_rename(const char* src, const char* dst) {
    furi_assert(src);
    furi_assert(dst);

    Storage* fs_api = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(fs_api);

    FuriString* path;
    FuriString* buffer;

    buffer = furi_string_alloc();
    path = furi_string_alloc();

    furi_string_printf(path, "%s", src);
    storage_common_remove(fs_api, ARCHIVE_FAV_TEMP_PATH);
    bool result = storage_file_open(file, ARCHIVE_FAV_PATH, FSAM_READ, FSOM_OPEN_EXISTING);

    uint16_t written = 0;
    if(result) {
        while(1) {
            if(!archive_favorites_read_line(file, buffer)) {
                break;
            }
            if(!furi_string_size(buffer)) {
                continue;
            }

            archive_file_append(
                ARCHIVE_FAV_TEMP_PATH,
                "%s\n",
                furi_string_search(buffer, path) ? furi_string_get_cstr(buffer) : dst);
            written++;
        }
    }

    furi_string_free(buffer);
    furi_string_free(path);

    storage_file_close(file);
    archive_favorites_commit_temp(fs_api, written == 0);

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return result;
}

void archive_add_to_favorites(const char* file_path) {
    furi_assert(file_path);

    archive_file_append(ARCHIVE_FAV_PATH, "%s\n", file_path);
}
