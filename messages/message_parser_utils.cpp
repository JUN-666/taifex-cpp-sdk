#include "message_parser_utils.h"
#include "pack_bcd.h" // For CoreUtils::packBcdToAscii
#include "logger.h" // For logging (Removed core_utils/ prefix)
#include <iostream>   // For potential error logging (can be replaced by a formal logger)
#include <span>      // For std::span
#include <cstddef>   // For std::byte
#include <vector>    // For std::vector

namespace SpecificMessageParsers {

std::string bcdBytesToAsciiStringHelper(std::span<const std::byte> bcd_data_view, size_t num_digits) {
    if (bcd_data_view.empty()) {
        return "";
    }
    // CoreUtils::packBcdToAscii expects std::vector<unsigned char>
    // Create this vector from the std::span<const std::byte>
    std::vector<unsigned char> bcd_vector;
    bcd_vector.reserve(bcd_data_view.size());
    for(std::byte b : bcd_data_view) {
        bcd_vector.push_back(static_cast<unsigned char>(b));
    }
    // Alternatively, if CoreUtils::packBcdToAscii could take iterators or a const unsigned char*:
    // const unsigned char* char_ptr = reinterpret_cast<const unsigned char*>(bcd_data_view.data());
    // return CoreUtils::packBcdToAscii(char_ptr, bcd_data_view.size(), num_digits); // If such overload existed

    try {
        // Assuming num_digits is correctly specified by the caller based on BCD format 9(M)
        return CoreUtils::packBcdToAscii(bcd_vector, num_digits);
    } catch (const std::runtime_error& e) {
        // Log the error from CoreUtils::packBcdToAscii
        LOG_ERROR << "bcdBytesToAsciiStringHelper: BCD to ASCII conversion error: " << e.what();
        return ""; // Return empty string on error, caller should check.
    }
}

} // namespace SpecificMessageParsers
