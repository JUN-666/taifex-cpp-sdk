// string_utils.cpp
#include "string_utils.h"
#include <iomanip> // For std::setw, std::setfill, std::hex
#include <sstream> // For std::ostringstream

namespace CoreUtils {

std::string bytesToString(const unsigned char* data, size_t len) {
    if (data == nullptr || len == 0) {
        return "";
    }
    // Construct string directly from char pointer and length to include nulls
    return std::string(reinterpret_cast<const char*>(data), len);
}

std::string bytesToHexString(const unsigned char* data, size_t len) {
    if (data == nullptr || len == 0) {
        return "";
    }
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::string bytesToHexString(const std::vector<unsigned char>& data) {
    if (data.empty()) {
        return "";
    }
    return bytesToHexString(data.data(), data.size());
}

} // namespace CoreUtils
