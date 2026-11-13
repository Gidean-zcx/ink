#pragma once

#include "archive_files.h"

static const char* const known_ext[] = {
    [ArchiveFileTypeText] = ".txt|.md|.csv|.json|.html|.htm",
    [ArchiveFileTypeBook] = ".epub",
    [ArchiveFileTypeAudio] = ".mp3|.wav|.ogg|.opus|.flac|.m4a",
    [ArchiveFileTypeImage] = ".png|.jpg|.jpeg|.bmp|.gif|.webp",
    [ArchiveFileTypeZip] = "?",
    [ArchiveFileTypeFolder] = "?",
    [ArchiveFileTypeUnknown] = "*",
    [ArchiveFileTypeAppOrJs] = "?",
    [ArchiveFileTypeSetting] = "?",
};

static inline bool archive_is_known_app(ArchiveFileTypeEnum type) {
    return type != ArchiveFileTypeFolder && type != ArchiveFileTypeUnknown;
}
