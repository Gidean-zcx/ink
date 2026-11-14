/*
 * MIT License
 *
 * Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd.
 * Copyright (c) 2025 Project Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/* Verbatim excerpts from AlayaElla/MetalioClaw4-AgentUI @ 172ec530ff65e11521585fe3b8611daa20a78aa4,
 * main/display/agent_ui/apps/camera/. Changed lines are listed in docs/tasks/gallery-3/SOURCE.md;
 * only system/apps/gallery/repository.cpp includes this file. */
#pragma once

namespace agent_ui::camera {
namespace {

bool IsJpg(const char* name) {
    if (name == nullptr) return false;
    const std::size_t length = std::strlen(name);
    if (length < 5) return false;
    const char* extension = name + length - 4;
    const bool jpg = extension[0] == '.' &&
                     (extension[1] == 'j' || extension[1] == 'J') &&
                     (extension[2] == 'p' || extension[2] == 'P') &&
                     (extension[3] == 'g' || extension[3] == 'G');
    if (jpg) return true;
    if (length < 6) return false;
    extension = name + length - 5;
    return extension[0] == '.' &&
           (extension[1] == 'j' || extension[1] == 'J') &&
           (extension[2] == 'p' || extension[2] == 'P') &&
           (extension[3] == 'e' || extension[3] == 'E') &&
           (extension[4] == 'g' || extension[4] == 'G');
}

std::string FormatDate(const std::tm& local, char separator) {
    const long long raw_year = static_cast<long long>(local.tm_year) + 1900;
    const unsigned year = static_cast<unsigned>(
        std::clamp(raw_year, 0LL, 9999LL));
    const unsigned month = static_cast<unsigned>(std::clamp(
        static_cast<long long>(local.tm_mon) + 1, 1LL, 12LL));
    const unsigned day = static_cast<unsigned>(std::clamp(
        static_cast<long long>(local.tm_mday), 1LL, 31LL));

    std::string result(10, '0');
    result[0] = static_cast<char>('0' + year / 1000);
    result[1] = static_cast<char>('0' + (year / 100) % 10);
    result[2] = static_cast<char>('0' + (year / 10) % 10);
    result[3] = static_cast<char>('0' + year % 10);
    result[4] = separator;
    result[5] = static_cast<char>('0' + month / 10);
    result[6] = static_cast<char>('0' + month % 10);
    result[7] = separator;
    result[8] = static_cast<char>('0' + day / 10);
    result[9] = static_cast<char>('0' + day % 10);
    return result;
}

std::string FormatDayLabel(const std::tm& local) {
    char value[32];
    std::snprintf(value, sizeof(value), TR("%d月%d日"), local.tm_mon + 1,
                  local.tm_mday);
    return value;
}

std::string FormatTimeLabel(const std::tm& local) {
    char value[8];
    std::snprintf(value, sizeof(value), "%02d:%02d", local.tm_hour,
                  local.tm_min);
    return value;
}

std::string FormatSizeLabel(std::size_t bytes) {
    if (bytes == 0) return TR("大小未知");
    char value[24];
    if (bytes < 1024 * 1024) {
        std::snprintf(value, sizeof(value), "%u KB",
                      static_cast<unsigned>((bytes + 1023) / 1024));
    } else {
        std::snprintf(value, sizeof(value), "%.1f MB",
                      static_cast<double>(bytes) / (1024.0 * 1024.0));
    }
    return value;
}

template <typename Photos>
void SortNewestFirst(Photos& result) {
    std::sort(result.begin(), result.end(), [](const auto& left,
                                               const auto& right) {
        if (left.modified != right.modified) return left.modified > right.modified;
        return left.name > right.name;
    });
}

template <typename Gallery>
std::vector<std::pair<std::size_t, std::size_t>> GroupByDateKey(const Gallery& gallery) {
    std::vector<std::pair<std::size_t, std::size_t>> groups;
    std::size_t begin = 0;
    while (begin < gallery.size()) {
        const std::string date_key = gallery[begin].date_key;
        std::size_t end = begin + 1;
        while (end < gallery.size() &&
               gallery[end].date_key == date_key) {
            ++end;
        }
        groups.emplace_back(begin, end);
        begin = end;
    }
    return groups;
}

}  // namespace
}  // namespace agent_ui::camera
