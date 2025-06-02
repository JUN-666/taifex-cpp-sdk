// checksum.h
#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <vector>
#include <numeric> // For std::accumulate if used, though direct XOR loop is fine
#include <cstddef> // For size_t and std::byte
#include <span>    // For std::span

namespace CoreUtils {

/**
 * @brief Calculates the XOR checksum for a given segment of data.
 *
 * As per the specification: "將第二個byte 起至 check-sum 欄位前一個byte,
 * 每個byte 中各bit 之 XOR 值記錄至check-sum 欄位."
 * This function assumes 'data_segment' IS the segment from the
 * "second byte up to check-sum field - 1 byte".
 *
 * For example, if the raw message part for checksum is [D2, D3, ..., Dn-1],
 * this function calculates D2 ^ D3 ^ ... ^ Dn-1.
 *
 * @param data_segment A std::span representing the data segment.
 * @return The calculated XOR checksum byte. Returns 0 if the span is empty.
 */
unsigned char calculateXorChecksum(std::span<const std::byte> data_segment);

/**
 * @brief Calculates the XOR checksum for a given vector of bytes.
 * This is an overload for convenience.
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
 * Calculates the checksum on 'data_segment' and compares it
 * with 'expected_checksum'.
 *
 * @param data_segment A std::span representing the data segment.
 * @param expected_checksum The checksum byte to verify against.
 * @return True if the calculated checksum matches expected_checksum, false otherwise.
 */
bool verifyXorChecksum(std::span<const std::byte> data_segment, unsigned char expected_checksum);

/**
 * @brief Verifies if the provided checksum is correct for the given data segment (vector version).
 * This is an overload for convenience.
 *
 * @param data The vector of bytes (data segment for checksum).
 * @param expected_checksum The checksum byte to verify against.
 * @return True if the calculated checksum matches expected_checksum, false otherwise.
 */
bool verifyXorChecksum(const std::vector<unsigned char>& data, unsigned char expected_checksum);

} // namespace CoreUtils

#endif // CHECKSUM_H
