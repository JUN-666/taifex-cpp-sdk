// string_utils.h
#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <string>
#include <vector>
#include <cstddef> // For size_t

namespace CoreUtils {

/**
 * @brief Converts a byte array (which may contain null characters) to an std::string.
 *
 * The resulting string will have its size equal to 'len', including any null characters
 * from the input byte array.
 *
 * @param data Pointer to the byte array.
 * @param len Length of the byte array.
 * @return std::string containing the exact byte sequence. Returns empty string if data is null or len is 0.
 */
std::string bytesToString(const unsigned char* data, size_t len);

/**
 * @brief Converts a byte array to its hexadecimal string representation.
 *
 * Example: {0xDE, 0xAD, 0xBE, 0xEF} -> "DEADBEEF"
 *
 * @param data Pointer to the byte array.
 * @param len Length of the byte array.
 * @return Hexadecimal string representation (uppercase). Empty if data is null or len is 0.
 */
std::string bytesToHexString(const unsigned char* data, size_t len);

/**
 * @brief Converts a vector of bytes to its hexadecimal string representation.
 *
 * Example: {0xDE, 0xAD, 0xBE, 0xEF} -> "DEADBEEF"
 *
 * @param data Vector of bytes.
 * @return Hexadecimal string representation (uppercase). Empty if vector is empty.
 */
std::string bytesToHexString(const std::vector<unsigned char>& data);

} // namespace CoreUtils

#endif // STRING_UTILS_H
