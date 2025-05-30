// checksum.cpp
#include "checksum.h"

namespace CoreUtils {

unsigned char calculateXorChecksum(const unsigned char* data_segment_for_checksum, size_t length) {
    if (data_segment_for_checksum == nullptr || length == 0) {
        return 0; // Or throw an error, but 0 for empty segment is a possible checksum.
                  // The spec implies non-empty segments.
    }
    unsigned char checksum = 0;
    // The spec says "每個byte 中各bit 之 XOR 值". This is just XORing the bytes.
    for (size_t i = 0; i < length; ++i) {
        checksum ^= data_segment_for_checksum[i];
    }
    return checksum;
}

unsigned char calculateXorChecksum(const std::vector<unsigned char>& data) {
    if (data.empty()) {
        return 0;
    }
    return calculateXorChecksum(data.data(), data.size());
}

bool verifyXorChecksum(const unsigned char* data_segment_for_checksum, size_t length, unsigned char expected_checksum) {
    return calculateXorChecksum(data_segment_for_checksum, length) == expected_checksum;
}

bool verifyXorChecksum(const std::vector<unsigned char>& data, unsigned char expected_checksum) {
    return calculateXorChecksum(data.data(), data.size()) == expected_checksum;
}

} // namespace CoreUtils
