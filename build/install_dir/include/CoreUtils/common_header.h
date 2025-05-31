// common_header.h
#ifndef COMMON_HEADER_H
#define COMMON_HEADER_H

#include <array>   // For std::array
#include <cstdint> // For fixed-width integers if needed (though unsigned char is fine for bytes)
#include <cstddef> // For size_t

// Forward declaration for parsing function (will be implemented in common_header.cpp later)
// struct CommonHeader;
// bool parseCommonHeader(const unsigned char* buffer, size_t buffer_len, CommonHeader& out_header);

namespace CoreUtils { // Assuming this continues to be part of CoreUtils, or a new MessageUtils namespace

/**
 * @brief Represents the "行情訊息共用檔頭" (Common Message Header)
 * as defined in "逐筆行情資訊傳輸作業手冊(V1.9.0).pdf", 肆、四、(一), page 14.
 *
 * This struct stores the raw byte representation of the header fields.
 * Parsing these bytes into meaningful values (e.g., BCD to numbers)
 * will be handled by a dedicated parsing function.
 */
struct CommonHeader {
    // Constant for the total size of the common header in bytes.
    // 1 (ESC) + 1 (TC) + 1 (MK) + 6 (IT) + 2 (CI) + 5 (CS) + 1 (VN) + 2 (BL) = 18 bytes
    static constexpr size_t HEADER_SIZE = 18;

    unsigned char esc_code;          // $X(1)$, 1 byte, (ASCII 27)
    unsigned char transmission_code; // $X(1)$, 1 byte, TRANSMISSION-CODE
    unsigned char message_kind;      // $X(1)$, 1 byte, MESSAGE-KIND

    // $9(12)$, 6 bytes, 資料時間 PACK BCD (時分秒毫秒微秒)
    std::array<unsigned char, 6> information_time_bcd;

    // $9(4)$, 2 bytes, 「傳輸群組編號 PACK BCD
    std::array<unsigned char, 2> channel_id_bcd;

    // $9(10)$, 5 bytes, 「傳輸群組訊息流水序號 PACK BCD (max 4294967295)
    // The document says $9(10)$ which is 10 digits. 5 bytes of BCD can store 10 digits.
    std::array<unsigned char, 5> channel_seq_bcd;

    // $9(2)$, 1 byte, 電文格式版本 PACK BCD
    unsigned char version_no_bcd; // Note: Doc says $9(2)$ and LENG 1. This implies 1 byte BCD (00-99).
                                  // If it were 2 BCD digits in 1 byte, like 0x12 for version 12.

    // $9(4)$, 2 bytes, 電文長度 PACK BCD
    std::array<unsigned char, 2> body_length_bcd;

    // Default constructor (optional, but can be useful)
    CommonHeader()
        : esc_code(0), transmission_code(0), message_kind(0), version_no_bcd(0) {
        information_time_bcd.fill(0);
        channel_id_bcd.fill(0);
        channel_seq_bcd.fill(0);
        body_length_bcd.fill(0);
    }

    // Placeholder for a parsing function that would populate this struct from a buffer.
    // bool parseFromBuffer(const unsigned char* buffer, size_t buffer_len);
};

} // namespace CoreUtils

#endif // COMMON_HEADER_H
