// common_header.h
#ifndef COMMON_HEADER_H
#define COMMON_HEADER_H

#include <array>
#include <string>
#include <vector>  // Needed for BCD conversion helpers
#include <cstdint>
#include <cstddef> // For size_t
// #include <stdexcept> // For std::stoul, etc. exceptions - Not directly used in header

// Forward declare to avoid circular dependency if pack_bcd.h includes error_codes.h which might include this.
// However, pack_bcd.h does not include error_codes.h directly.
// #include "pack_bcd.h" // For CoreUtils::packBcdToAscii
// #include "error_codes.h" // For CoreUtils::ParsingError

namespace CoreUtils {
    // Forward declaration from pack_bcd.h
    // Ensure this signature matches the actual one in pack_bcd.h
    std::string packBcdToAscii(const std::vector<unsigned char>& bcd_data, size_t num_digits);

    // Forward declaration from error_codes.h
    // We need to ensure this is just a declaration if the definition is complex
    // or if error_codes.h includes this header. For now, this is fine.
    class ParsingError; // This is fine as it's just used as an exception type to throw.


struct CommonHeader {
    // Constant for the total size of the common header in bytes.
    // ESC(1) + TC(1) + MK(1) + IT(6) + CI(2) + CS(5) + VN(1) + BL(2) = 19 bytes
    static constexpr size_t HEADER_SIZE = 19;

    unsigned char esc_code;
    unsigned char transmission_code;
    unsigned char message_kind;
    std::array<unsigned char, 6> information_time_bcd;
    std::array<unsigned char, 2> channel_id_bcd;
    std::array<unsigned char, 5> channel_seq_bcd;
    unsigned char version_no_bcd;
    std::array<unsigned char, 2> body_length_bcd;

    CommonHeader(); // Default constructor

    /**
     * @brief Parses a raw byte buffer to populate the CommonHeader structure.
     *
     * @param buffer Pointer to the raw data buffer.
     * @param buffer_len Length of the buffer.
     * @param out_header Reference to the CommonHeader struct to be populated.
     * @return True if parsing was successful (i.e., buffer was long enough), false otherwise.
     */
    static bool parse(const unsigned char* buffer, size_t buffer_len, CommonHeader& out_header);

    // --- BCD Decoding Helper Methods ---

    /** @return INFORMATION-TIME as a 12-digit string (HHMMSSmmmSSS). Throws CoreUtils::ParsingError on failure. */
    std::string getInformationTimeString() const;

    /** @return CHANNEL-ID as uint32_t. Throws CoreUtils::ParsingError on failure. $9(4) -> 0-9999 */
    uint32_t getChannelId() const;

    /** @return CHANNEL-SEQ as uint64_t. Throws CoreUtils::ParsingError on failure. $9(10) -> 0-9,999,999,999 */
    uint64_t getChannelSeq() const;

    /** @return VERSION-NO as uint8_t. Throws CoreUtils::ParsingError on failure. $9(2) -> 0-99 */
    uint8_t getVersionNo() const;

    /** @return BODY-LENGTH as uint16_t. Throws CoreUtils::ParsingError on failure. $9(4) -> 0-9999 */
    uint16_t getBodyLength() const;
};

} // namespace CoreUtils

#endif // COMMON_HEADER_H
