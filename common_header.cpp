// common_header.cpp
#include "common_header.h"
#include "pack_bcd.h"      // For CoreUtils::packBcdToAscii
#include "error_codes.h"   // For CoreUtils::ParsingError and CoreUtils::InvalidArgumentError
#include <cstring>         // For std::memcpy
#include <vector>          // For std::vector conversion
#include <string>          // For std::stoul, std::stoull
#include <stdexcept>       // For std::out_of_range, std::invalid_argument from stoul

namespace CoreUtils {

CommonHeader::CommonHeader()
    : esc_code(0), transmission_code(0), message_kind(0), version_no_bcd(0) {
    information_time_bcd.fill(0);
    channel_id_bcd.fill(0);
    channel_seq_bcd.fill(0);
    body_length_bcd.fill(0);
}

bool CommonHeader::parse(const unsigned char* buffer, size_t buffer_len, CommonHeader& out_header) {
    if (buffer == nullptr || buffer_len < HEADER_SIZE) {
        return false; // Buffer too small or null
    }

    size_t offset = 0;
    out_header.esc_code = buffer[offset++];
    out_header.transmission_code = buffer[offset++];
    out_header.message_kind = buffer[offset++];

    std::memcpy(out_header.information_time_bcd.data(), buffer + offset, out_header.information_time_bcd.size());
    offset += out_header.information_time_bcd.size();

    std::memcpy(out_header.channel_id_bcd.data(), buffer + offset, out_header.channel_id_bcd.size());
    offset += out_header.channel_id_bcd.size();

    std::memcpy(out_header.channel_seq_bcd.data(), buffer + offset, out_header.channel_seq_bcd.size());
    offset += out_header.channel_seq_bcd.size();

    out_header.version_no_bcd = buffer[offset++];

    std::memcpy(out_header.body_length_bcd.data(), buffer + offset, out_header.body_length_bcd.size());
    offset += out_header.body_length_bcd.size();

    // Ensure that exactly HEADER_SIZE bytes were consumed.
    // This check is somewhat redundant given the fixed size copies and increments,
    // but it's a good safeguard if logic were to change.
    return offset == HEADER_SIZE;
}

// Helper template for BCD array to string conversion
template<size_t N>
static std::string bcdArrayToNumericString(const std::array<unsigned char, N>& bcd_array, size_t num_digits, const char* field_name) {
    std::vector<unsigned char> bcd_vec(bcd_array.begin(), bcd_array.end());
    try {
        // Assuming packBcdToAscii is robust and throws on invalid BCD format for the given num_digits.
        return packBcdToAscii(bcd_vec, num_digits);
    } catch (const CoreUtils::ParsingError& e) { // If packBcdToAscii itself throws ParsingError
        throw CoreUtils::ParsingError(std::string("Failed to decode BCD for ") + field_name + ": " + e.what());
    } catch (const std::runtime_error& e) { // Catch other runtime errors from packBcdToAscii (like its own invalid_argument for non-digits)
        // pack_bcd.cpp uses std::runtime_error for "Invalid BCD data: nibble > 9"
        // and "Invalid input: asciiToPackBcd expects a string containing only digits." (not relevant here)
        throw CoreUtils::ParsingError(std::string("Runtime error decoding BCD for ") + field_name + ": " + e.what());
    }
}
// Special case for single byte BCD
static std::string bcdByteToNumericString(unsigned char bcd_byte, size_t num_digits, const char* field_name) {
    std::vector<unsigned char> bcd_vec = {bcd_byte};
     try {
        return packBcdToAscii(bcd_vec, num_digits);
    } catch (const CoreUtils::ParsingError& e) {
        throw CoreUtils::ParsingError(std::string("Failed to decode BCD for ") + field_name + ": " + e.what());
    } catch (const std::runtime_error& e) {
        throw CoreUtils::ParsingError(std::string("Runtime error decoding BCD for ") + field_name + ": " + e.what());
    }
}


std::string CommonHeader::getInformationTimeString() const {
    // $9(12)$ -> 12 digits
    return bcdArrayToNumericString(information_time_bcd, 12, "INFORMATION-TIME");
}

uint32_t CommonHeader::getChannelId() const {
    // $9(4)$ -> 4 digits
    std::string s = bcdArrayToNumericString(channel_id_bcd, 4, "CHANNEL-ID");
    try {
        // std::stoul returns unsigned long. Ensure it fits in uint32_t.
        unsigned long val = std::stoul(s);
        if (val > UINT32_MAX) { // UINT32_MAX is from <cstdint> or <climits> via other includes
             throw std::out_of_range("CHANNEL-ID value out of range for uint32_t: " + s);
        }
        return static_cast<uint32_t>(val);
    } catch (const std::out_of_range& oor) {
        throw CoreUtils::ParsingError("CHANNEL-ID value out of range: " + s + " (" + oor.what() + ")");
    } catch (const std::invalid_argument& ia) {
        throw CoreUtils::ParsingError("CHANNEL-ID value invalid for conversion: " + s + " (" + ia.what() + ")");
    }
}

uint64_t CommonHeader::getChannelSeq() const {
    // $9(10)$ -> 10 digits
    std::string s = bcdArrayToNumericString(channel_seq_bcd, 10, "CHANNEL-SEQ");
    try {
        return std::stoull(s); // std::stoull returns unsigned long long (uint64_t generally)
    } catch (const std::out_of_range& oor) {
        throw CoreUtils::ParsingError("CHANNEL-SEQ value out of range: " + s + " (" + oor.what() + ")");
    } catch (const std::invalid_argument& ia) {
        throw CoreUtils::ParsingError("CHANNEL-SEQ value invalid for conversion: " + s + " (" + ia.what() + ")");
    }
}

uint8_t CommonHeader::getVersionNo() const {
    // $9(2)$ -> 2 digits from a single byte
    std::string s = bcdByteToNumericString(version_no_bcd, 2, "VERSION-NO");
    try {
        unsigned long val = std::stoul(s); // stoul for safety before casting
        if (val > UINT8_MAX) { // UINT8_MAX is from <cstdint> or <climits>
            throw std::out_of_range("VERSION-NO value out of range for uint8_t: " + s);
        }
        return static_cast<uint8_t>(val);
    } catch (const std::out_of_range& oor) {
        throw CoreUtils::ParsingError("VERSION-NO value out of range: " + s + " (" + oor.what() + ")");
    } catch (const std::invalid_argument& ia) {
        throw CoreUtils::ParsingError("VERSION-NO value invalid for conversion: " + s + " (" + ia.what() + ")");
    }
}

uint16_t CommonHeader::getBodyLength() const {
    // $9(4)$ -> 4 digits
    std::string s = bcdArrayToNumericString(body_length_bcd, 4, "BODY-LENGTH");
    try {
        unsigned long val = std::stoul(s); // stoul for safety
        if (val > UINT16_MAX) { // UINT16_MAX is from <cstdint> or <climits>
             throw std::out_of_range("BODY-LENGTH value out of range for uint16_t: " + s);
        }
        return static_cast<uint16_t>(val);
    } catch (const std::out_of_range& oor) {
        throw CoreUtils::ParsingError("BODY-LENGTH value out of range: " + s + " (" + oor.what() + ")");
    } catch (const std::invalid_argument& ia) {
        throw CoreUtils::ParsingError("BODY-LENGTH value invalid for conversion: " + s + " (" + ia.what() + ")");
    }
}

} // namespace CoreUtils
