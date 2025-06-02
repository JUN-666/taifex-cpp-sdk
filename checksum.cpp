// checksum.cpp
#include "checksum.h"
#include <span>      // For std::span
#include <cstddef>   // For std::byte
#include <vector>    // For std::vector overload

namespace CoreUtils {

unsigned char calculateXorChecksum(std::span<const std::byte> data_segment) {
    if (data_segment.empty()) {
        return 0;
    }
    unsigned char checksum = 0;
    for (std::byte b : data_segment) {
        checksum ^= static_cast<unsigned char>(b); // Cast std::byte to unsigned char for XOR
    }
    return checksum;
}

unsigned char calculateXorChecksum(const std::vector<unsigned char>& data) {
    if (data.empty()) {
        return 0;
    }
    // Convert vector<unsigned char> to span<const std::byte>
    return calculateXorChecksum(std::as_bytes(std::span<const unsigned char>(data)));
}

bool verifyXorChecksum(std::span<const std::byte> data_segment, unsigned char expected_checksum) {
    return calculateXorChecksum(data_segment) == expected_checksum;
}

bool verifyXorChecksum(const std::vector<unsigned char>& data, unsigned char expected_checksum) {
    // Convert vector<unsigned char> to span<const std::byte>
    return verifyXorChecksum(std::as_bytes(std::span<const unsigned char>(data)), expected_checksum);
}

} // namespace CoreUtils
