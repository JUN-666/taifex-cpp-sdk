#ifndef MESSAGE_PARSER_UTILS_H
#define MESSAGE_PARSER_UTILS_H

#include <string>
#include <vector>
#include <cstddef> // For size_t and std::byte
#include <span>    // For std::span

// Forward declaration for CoreUtils::packBcdToAscii (if needed, but usually not for .h)

namespace SpecificMessageParsers {

/**
 * @brief Converts a segment of raw BCD byte data to an ASCII string representation.
 *
 * This function takes a span of BCD data bytes and the expected
 * number of digits in the resulting ASCII string. It uses CoreUtils::packBcdToAscii
 * for the underlying conversion.
 *
 * @param bcd_data_view A std::span representing the raw BCD data bytes.
 * @param num_digits The total number of digits expected in the output ASCII string.
 * @return std::string The ASCII representation of the BCD data. Returns an empty
 *                     string if the span is empty or if the
 *                     conversion via CoreUtils::packBcdToAscii fails.
 */
std::string bcdBytesToAsciiStringHelper(std::span<const std::byte> bcd_data_view, size_t num_digits);

} // namespace SpecificMessageParsers

#endif // MESSAGE_PARSER_UTILS_H
