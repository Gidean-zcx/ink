/* Shim: no Flipper apps or U2F here ("/app:" entries never occur). */
#pragma once
#include "archive_files.h"
typedef enum { ArchiveAppTypeUnknown } ArchiveAppTypeEnum;
#define archive_get_app_type(path) ((void)(path), ArchiveAppTypeUnknown)
#define archive_get_app_filetype(type) ((void)(type), ArchiveFileTypeUnknown)
#define archive_app_is_available(context, path) ((void)(context), (void)(path), false)
#define archive_app_read_dir(context, path) ((void)(context), (void)(path), false)
