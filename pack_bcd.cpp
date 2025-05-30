// pack_bcd.cpp
#include "pack_bcd.h"
#include <algorithm> // For std::all_of
#include <cctype>    // For std::isdigit

namespace CoreUtils {

std::vector<unsigned char> asciiToPackBcd(const std::string& ascii_numeric_str) {
    if (!std::all_of(ascii_numeric_str.begin(), ascii_numeric_str.end(), ::isdigit)) {
        if (ascii_numeric_str.empty()) {
             return {};
        }
        throw std::runtime_error("Invalid input: asciiToPackBcd expects a string containing only digits.");
    }

    if (ascii_numeric_str.empty()) {
        return {};
    }

    std::string s = ascii_numeric_str;
    if (s.length() % 2 != 0) {
        s.insert(0, "0");
    }

    std::vector<unsigned char> bcd_result;
    bcd_result.reserve(s.length() / 2);

    for (size_t i = 0; i < s.length(); i += 2) {
        unsigned char d1 = s[i] - '0';
        unsigned char d2 = s[i+1] - '0';
        bcd_result.push_back((d1 << 4) | d2);
    }
    return bcd_result;
}

std::string packBcdToAscii(const std::vector<unsigned char>& bcd_data, size_t num_digits) {
    std::string full_decoded_str;
    full_decoded_str.reserve(bcd_data.size() * 2);

    for (unsigned char byte : bcd_data) {
        unsigned char nibble1 = (byte >> 4) & 0x0F;
        unsigned char nibble2 = byte & 0x0F;

        if (nibble1 > 9 || nibble2 > 9) {
            throw std::runtime_error("Invalid BCD data: nibble > 9 detected during packBcdToAscii.");
        }
        full_decoded_str.push_back(nibble1 + '0');
        full_decoded_str.push_back(nibble2 + '0');
    }

    if (num_digits == 0 && !bcd_data.empty()) {
        return full_decoded_str;
    }
    if (num_digits == 0 && bcd_data.empty()){
        return "";
    }

    if (full_decoded_str.length() < num_digits) {
        std::string padding(num_digits - full_decoded_str.length(), '0');
        return padding + full_decoded_str;
    } else if (full_decoded_str.length() > num_digits) {
        return full_decoded_str.substr(full_decoded_str.length() - num_digits);
    }

    return full_decoded_str;
}

std::string packBcdToAscii(const std::vector<unsigned char>& bcd_data) {
    if (bcd_data.empty()) {
        return "";
    }
    std::string ascii_str;
    ascii_str.reserve(bcd_data.size() * 2);
    for (unsigned char byte : bcd_data) {
        unsigned char nibble1 = (byte >> 4) & 0x0F;
        unsigned char nibble2 = byte & 0x0F;

        if (nibble1 > 9) throw std::runtime_error("Invalid BCD data: first nibble > 9.");
        if (nibble2 > 9) throw std::runtime_error("Invalid BCD data: second nibble > 9.");

        ascii_str.push_back(nibble1 + '0');
        ascii_str.push_back(nibble2 + '0');
    }
    return ascii_str;
}

} // namespace CoreUtils
