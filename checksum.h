// checksum.h
#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <vector>
#include <numeric> // For std::accumulate if used, though direct XOR loop is fine
#include <cstddef> // For size_t

namespace CoreUtils {

/**
 * @brief Calculates the XOR checksum for a given range of bytes.
 *
 * As per the specification: "將第二個byte 起至 check-sum 欄位前一個byte,
 * 每個byte 中各bit 之 XOR 值記錄至check-sum 欄位."
 * This function assumes 'data_segment_for_checksum' IS the segment from the
 * "second byte up to check-sum field - 1 byte".
 *
 * For example, if the raw message part for checksum is [D2, D3, ..., Dn-1],
 * this function calculates D2 ^ D3 ^ ... ^ Dn-1.
 *
 * @param data_segment_for_checksum A pointer to the beginning of the data segment.
 * @param length The number of bytes in the data segment.
 * @return The calculated XOR checksum byte. Returns 0 if length is 0.
 */
unsigned char calculateXorChecksum(const unsigned char* data_segment_for_checksum, size_t length);

/**
 * @brief Calculates the XOR checksum for a given vector of bytes.
 *
 * Convenience wrapper for the pointer-based calculateXorChecksum.
 * The input vector should represent the data segment from the "second byte
 * up to check-sum field - 1 byte".
 *
 * @param data The vector of bytes for which to calculate the checksum.
 * @return The calculated XOR checksum byte. Returns 0 if the vector is empty.
 */
unsigned char calculateXorChecksum(const std::vector<unsigned char>& data);


/**
 * @brief Verifies if the provided checksum is correct for the given data segment.
 *
 * Calculates the checksum on 'data_segment_for_checksum' and compares it
 * with 'expected_checksum'.
 *
 * @param data_segment_for_checksum A pointer to the beginning of the data segment.
 * @param length The number of bytes in the data segment.
 * @param expected_checksum The checksum byte to verify against.
 * @return True if the calculated checksum matches expected_checksum, false otherwise.
 */
bool verifyXorChecksum(const unsigned char* data_segment_for_checksum, size_t length, unsigned char expected_checksum);

/**
 * @brief Verifies if the provided checksum is correct for the given data segment (vector version).
 *
 * @param data The vector of bytes (data segment for checksum).
 * @param expected_checksum The checksum byte to verify against.
 * @return True if the calculated checksum matches expected_checksum, false otherwise.
 */
bool verifyXorChecksum(const std::vector<unsigned char>& data, unsigned char expected_checksum);

} // namespace CoreUtils

#endif // CHECKSUM_H
