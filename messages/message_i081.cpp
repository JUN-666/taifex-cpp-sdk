#include "message_i081.h"
#include "common_header.h" // For CoreUtils::packBcdToAscii (potentially, if CommonHeader itself uses it)
// pack_bcd.h is now included via message_parser_utils.h or its .cpp
#include "message_parser_utils.h" // For bcdBytesToAsciiStringHelper

#include <vector>
#include <string>
#include <cstring>   // For memcpy
#include <stdexcept> // For std::stoll, std::stoul
#include <iostream>  // For temporary error logging

namespace SpecificMessageParsers {

// Local static bcdBytesToAsciiString removed. Using bcdBytesToAsciiStringHelper from message_parser_utils.h

bool parse_i081_body(const unsigned char* body_data,
                     uint16_t body_length,
                     MessageI081& out_msg) {
    if (!body_data) {
        // std::cerr << "Error: body_data is null for I081." << std::endl;
        return false;
    }

    size_t offset = 0;

    // Minimum length for fixed part: PROD-ID (20) + PROD-MSG-SEQ (5) + NO-MD-ENTRIES (1) = 26 bytes
    const size_t MIN_FIXED_PART_LEN = 26;
    if (body_length < MIN_FIXED_PART_LEN) {
        // std::cerr << "Error: body_length (" << body_length
        //           << ") is too short for I081 fixed part. Expected at least "
        //           << MIN_FIXED_PART_LEN << " bytes." << std::endl;
        return false;
    }

    // 1. PROD-ID: X(20), Length 20
    const size_t PROD_ID_LEN = 20;
    out_msg.prod_id.assign(reinterpret_cast<const char*>(body_data + offset), PROD_ID_LEN);
    offset += PROD_ID_LEN;

    // 2. PROD-MSG-SEQ: 9(10), Length 5
    const size_t PROD_MSG_SEQ_BCD_LEN = 5;
    const size_t PROD_MSG_SEQ_DIGITS = 10;
    std::span<const unsigned char> pms_span(body_data + offset, PROD_MSG_SEQ_BCD_LEN);
    std::string pms_str = bcdBytesToAsciiStringHelper(std::as_bytes(pms_span), PROD_MSG_SEQ_DIGITS);
    if (pms_str.empty() && PROD_MSG_SEQ_DIGITS > 0) { return false; }
    try {
        out_msg.prod_msg_seq = std::stoul(pms_str); // Max 4,294,967,295 fits in uint32_t
    } catch (const std::exception& e) { /* std::cerr << "Error converting PROD-MSG-SEQ: " << e.what() << std::endl; */ return false; }
    offset += PROD_MSG_SEQ_BCD_LEN;

    // 3. NO-MD-ENTRIES: 9(2), Length 1
    const size_t NO_MD_ENTRIES_BCD_LEN = 1;
    const size_t NO_MD_ENTRIES_DIGITS = 2;
    std::span<const unsigned char> nme_span(body_data + offset, NO_MD_ENTRIES_BCD_LEN);
    std::string nme_str = bcdBytesToAsciiStringHelper(std::as_bytes(nme_span), NO_MD_ENTRIES_DIGITS);
    if (nme_str.empty() && NO_MD_ENTRIES_DIGITS > 0) { return false; }
    try {
        out_msg.no_md_entries = static_cast<uint8_t>(std::stoul(nme_str));
    } catch (const std::exception& e) { /* std::cerr << "Error converting NO-MD-ENTRIES: " << e.what() << std::endl; */ return false; }
    offset += NO_MD_ENTRIES_BCD_LEN;

    // Check if remaining body_length is sufficient for all entries
    // Each entry: MD-UPDATE-ACTION (1) + MD-ENTRY-TYPE (1) + SIGN (1) + MD-ENTRY-PX (5) + MD-ENTRY-SIZE (4) + MD-PRICE-LEVEL (1) = 13 bytes
    const size_t MD_ENTRY_SIZE_BYTES = 13;
    if (body_length < offset + (static_cast<size_t>(out_msg.no_md_entries) * MD_ENTRY_SIZE_BYTES)) {
        // std::cerr << "Error: body_length (" << body_length
        //           << ") is too short for the declared NO-MD-ENTRIES (" << out_msg.no_md_entries
        //           << ")." << std::endl;
        return false;
    }

    out_msg.md_entries.clear();
    out_msg.md_entries.reserve(out_msg.no_md_entries);

    for (uint8_t i = 0; i < out_msg.no_md_entries; ++i) {
        MdEntryI081 entry;

        // MD-UPDATE-ACTION: X(1), Length 1
        entry.md_update_action = static_cast<char>(body_data[offset]);
        offset += 1;

        // MD-ENTRY-TYPE: X(1), Length 1
        entry.md_entry_type = static_cast<char>(body_data[offset]);
        offset += 1;

        // SIGN: X(1), Length 1
        entry.sign = static_cast<char>(body_data[offset]);
        offset += 1;

        // MD-ENTRY-PX: 9(9), Length 5
        const size_t MD_ENTRY_PX_BCD_LEN = 5;
        const size_t MD_ENTRY_PX_DIGITS = 9;
        std::span<const unsigned char> px_span(body_data + offset, MD_ENTRY_PX_BCD_LEN);
        std::string px_str = bcdBytesToAsciiStringHelper(std::as_bytes(px_span), MD_ENTRY_PX_DIGITS);
        if (px_str.empty() && MD_ENTRY_PX_DIGITS > 0) { return false; }
        try {
            entry.md_entry_px = std::stoll(px_str);
        } catch (const std::exception& e) { /* std::cerr << "Error converting MD-ENTRY-PX: " << e.what() << std::endl; */ return false; }
        offset += MD_ENTRY_PX_BCD_LEN;

        // MD-ENTRY-SIZE: 9(8), Length 4
        const size_t MD_ENTRY_SIZE_BCD_LEN = 4;
        const size_t MD_ENTRY_SIZE_DIGITS = 8;
        std::span<const unsigned char> size_span(body_data + offset, MD_ENTRY_SIZE_BCD_LEN);
        std::string size_str = bcdBytesToAsciiStringHelper(std::as_bytes(size_span), MD_ENTRY_SIZE_DIGITS);
        if (size_str.empty() && MD_ENTRY_SIZE_DIGITS > 0) { return false; }
        try {
            entry.md_entry_size = std::stoll(size_str);
        } catch (const std::exception& e) { /* std::cerr << "Error converting MD-ENTRY-SIZE: " << e.what() << std::endl; */ return false; }
        offset += MD_ENTRY_SIZE_BCD_LEN;

        // MD-PRICE-LEVEL: 9(2), Length 1
        const size_t MD_PRICE_LEVEL_BCD_LEN = 1;
        const size_t MD_PRICE_LEVEL_DIGITS = 2;
        std::span<const unsigned char> level_span(body_data + offset, MD_PRICE_LEVEL_BCD_LEN);
        std::string level_str = bcdBytesToAsciiStringHelper(std::as_bytes(level_span), MD_PRICE_LEVEL_DIGITS);
        if (level_str.empty() && MD_PRICE_LEVEL_DIGITS > 0) { return false; }
        try {
            entry.md_price_level = static_cast<uint8_t>(std::stoul(level_str));
        } catch (const std::exception& e) { /* std::cerr << "Error converting MD-PRICE-LEVEL: " << e.what() << std::endl; */ return false; }
        offset += MD_PRICE_LEVEL_BCD_LEN;

        out_msg.md_entries.push_back(entry);
    }
    return true;
}

} // namespace SpecificMessageParsers
