#ifndef MESSAGE_PARSER_UTILS_H
#define MESSAGE_PARSER_UTILS_H

#include <string>
#include <vector>
#include <cstddef> // For size_t

// Forward declaration for CoreUtils::packBcdToAscii
// This avoids including pack_bcd.h in this header if it's not strictly necessary
// for the declaration alone. However, the .cpp will need it.
// For simplicity here, we might just include pack_bcd.h in the .cpp file only.

namespace SpecificMessageParsers {

/**
 * @brief Converts a segment of raw BCD byte data to an ASCII string representation.
 *
 * This function takes a pointer to BCD data, its byte length, and the expected
 * number of digits in the resulting ASCII string. It uses CoreUtils::packBcdToAscii
 * for the underlying conversion.
 *
 * @param bcd_data Pointer to the raw BCD data bytes.
 * @param bcd_len The number of bytes in the BCD data.
 * @param num_digits The total number of digits expected in the output ASCII string.
 * @return std::string The ASCII representation of the BCD data. Returns an empty
 *                     string if bcd_data is null, bcd_len is 0, or if the
 *                     conversion via CoreUtils::packBcdToAscii fails.
 */
std::string bcdBytesToAsciiStringHelper(const unsigned char* bcd_data, size_t bcd_len, size_t num_digits);

} // namespace SpecificMessageParsers

#endif // MESSAGE_PARSER_UTILS_H
