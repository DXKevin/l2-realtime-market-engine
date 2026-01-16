#pragma once
#include <string>
#include <vector>
#include <zlib/zlib.h>
#include "Logger.h"

inline std::vector<unsigned char> base64_decode(const std::string& input) {
    static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<int> reverse_table(256, -1);
    for (int i = 0; i < 64; ++i)
        reverse_table[(unsigned char)b64_chars[i]] = i;

    std::vector<unsigned char> out;
    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (reverse_table[c] == -1) continue; // 跳过非法字符（如换行、空格、=）
        val = (val << 6) + reverse_table[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<unsigned char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

inline std::vector<unsigned char> gzip_decompress(const std::vector<unsigned char>& compressed) {
    if (compressed.empty())
        return {};

    z_stream strm = {};
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    // 关键：16 + MAX_WBITS 表示输入是 GZIP 格式
    int ret = inflateInit2(&strm, 16 + MAX_WBITS);
    if (ret != Z_OK)
        LOG_ERROR("Base64Decoder", "inflateInit2 (gzip) failed");

    strm.next_in = const_cast<Bytef*>(compressed.data());
    strm.avail_in = static_cast<uInt>(compressed.size());

    std::vector<unsigned char> outbuffer(65536); // 64KB buffer
    std::vector<unsigned char> result;

    do {
        strm.next_out = outbuffer.data();
        strm.avail_out = static_cast<uInt>(outbuffer.size());
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&strm);
            LOG_ERROR("Base64Decoder", "gzip decompression failed with error code: {}", ret);
        }
        size_t have = outbuffer.size() - strm.avail_out;
        result.insert(result.end(), outbuffer.begin(), outbuffer.begin() + have);
    } while (strm.avail_out == 0);

    inflateEnd(&strm);
    return result;
}