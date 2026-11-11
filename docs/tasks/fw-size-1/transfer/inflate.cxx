#include <zlib.h>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static std::vector<unsigned char> read_file(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    assert(file.is_open());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

static int decode(const std::vector<unsigned char> &input, size_t chunk,
                  std::vector<unsigned char> &plain) {
    z_stream stream = {};
    stream.zalloc = [](void *, auto n, auto size) -> void * { return calloc(n, size); };
    stream.zfree = [](void *, void *p) { free(p); };
    assert(inflateInit(&stream) == Z_OK);
    std::vector<unsigned char> output(16384);
    size_t offset = 0;
    int result = Z_OK;
    for (;;) {
        if (!stream.avail_in && offset < input.size()) {
            size_t n = input.size() - offset;
            if (n > chunk) n = chunk;
            stream.next_in = const_cast<unsigned char *>(input.data() + offset);
            stream.avail_in = static_cast<uInt>(n);
            offset += n;
        }
        const auto before = stream.avail_in;
        stream.next_out = output.data();
        stream.avail_out = static_cast<uInt>(output.size());
        result = inflate(&stream, Z_NO_FLUSH);
        const size_t produced = output.size() - stream.avail_out;
        plain.insert(plain.end(), output.begin(), output.begin() + produced);
        if (result != Z_OK || (before == stream.avail_in && !produced)) break;
    }
    assert(inflateEnd(&stream) == Z_OK);
    return result;
}

int main(int argc, char **argv) {
    assert(argc == 2);
    for (int i = 0; i < 4; ++i) {
        const std::string base = std::string(argv[1]) + '/' + std::to_string(i);
        const auto expected = read_file(base + ".raw");
        auto input = read_file(base + ".z");
        for (size_t chunk : {size_t(1), size_t(997), size_t(16384)}) {
            std::vector<unsigned char> actual;
            assert(decode(input, chunk, actual) == Z_STREAM_END);
            assert(actual == expected);
        }
        input.back() ^= 1;
        std::vector<unsigned char> actual;
        assert(decode(input, 997, actual) == Z_DATA_ERROR);
        input.pop_back();
        actual.clear();
        assert(decode(input, 997, actual) != Z_STREAM_END);
    }
}
