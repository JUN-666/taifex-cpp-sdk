#include "message_parser_utils.h"
#include "pack_bcd.h" // For CoreUtils::packBcdToAscii
#include <iostream>   // For potential error logging (can be replaced by a formal logger)

namespace SpecificMessageParsers {

std::string bcdBytesToAsciiStringHelper(const unsigned char* bcd_data, size_t bcd_len, size_t num_digits) {
    if (!bcd_data || bcd_len == 0) {
        // Depending on strictness, could log an error or warning.
        // For now, returning empty string is consistent with previous local helpers.
        return "";
    }
    std::vector<unsigned char> bcd_vector(bcd_data, bcd_data + bcd_len);
    try {
        // Assuming num_digits is correctly specified by the caller based on BCD format 9(M)
        return CoreUtils::packBcdToAscii(bcd_vector, num_digits);
    } catch (const std::runtime_error& e) {
        // Optional: Log the error from CoreUtils::packBcdToAscii
        // std::cerr << "Error during BCD to ASCII conversion in helper: " << e.what() << std::endl;
        return ""; // Return empty string on error, caller should check.
    }
}

} // namespace SpecificMessageParsers
