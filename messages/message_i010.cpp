#include "message_i010.h"
#include "common_header.h" // For CoreUtils::packBcdToAscii and potentially CommonHeader struct (though not directly used in this func)
// pack_bcd.h is now included via message_parser_utils.h or its .cpp
#include "message_parser_utils.h" // For bcdBytesToAsciiStringHelper

#include <vector>
#include <string>
#include <cstring> // For memcpy
#include <stdexcept> // For std::stoll, std::stoul
#include <iostream> // For temporary error logging, replace with proper logger

namespace SpecificMessageParsers {

// Local static bcdBytesToAsciiString removed. Using bcdBytesToAsciiStringHelper from message_parser_utils.h


bool parse_i010_body(const unsigned char* body_data,
                     uint16_t body_length,
                     // const CoreUtils::CommonHeader& header, // Not directly used for body parsing here
                     MessageI010& out_msg) {
    if (!body_data) {
        // std::cerr << "Error: body_data is null for I010." << std::endl;
        return false;
    }

    size_t offset = 0;

    // Expected total length of the fixed part of I010 body, excluding checksum & terminal code
    // PROD-ID-S (10) + REFERENCE-PRICE (5) + PROD-KIND (1) + DECIMAL-LOCATOR (1) +
    // STRIKE-PRICE-DECIMAL-LOCATOR (1) + BEGIN-DATE (4) + END-DATE (4) +
    // FLOW-GROUP (1) + DELIVERY-DATE (4) + DYNAMIC-BANDING (1) = 32 bytes
    const size_t EXPECTED_MIN_BODY_LENGTH = 32;

    if (body_length < EXPECTED_MIN_BODY_LENGTH) {
        // std::cerr << "Error: body_length (" << body_length
        //           << ") is too short for I010. Expected at least " << EXPECTED_MIN_BODY_LENGTH << " bytes." << std::endl;
        return false;
    }

    // 1. PROD-ID-S: X(10), Length 10
    const size_t PROD_ID_S_LEN = 10;
    out_msg.prod_id_s.assign(reinterpret_cast<const char*>(body_data + offset), PROD_ID_S_LEN);
    offset += PROD_ID_S_LEN;

    // 2. REFERENCE-PRICE: 9(9), Length 5
    const size_t REF_PRICE_BCD_LEN = 5;
    const size_t REF_PRICE_DIGITS = 9;
    std::string ref_price_str = bcdBytesToAsciiStringHelper(body_data + offset, REF_PRICE_BCD_LEN, REF_PRICE_DIGITS);
    if (ref_price_str.empty() && REF_PRICE_DIGITS > 0) { /* Handle error if string is empty and expected digits */ return false; }
    try {
        out_msg.reference_price = std::stoll(ref_price_str);
    } catch (const std::exception& e) { /* std::cerr << "Error converting REFERENCE-PRICE: " << e.what() << std::endl; */ return false; }
    offset += REF_PRICE_BCD_LEN;

    // 3. PROD-KIND: X(1), Length 1
    const size_t PROD_KIND_LEN = 1;
    out_msg.prod_kind = static_cast<char>(body_data[offset]);
    offset += PROD_KIND_LEN;

    // 4. DECIMAL-LOCATOR: 9(1), Length 1
    const size_t DEC_LOC_BCD_LEN = 1;
    const size_t DEC_LOC_DIGITS = 1;
    std::string dec_loc_str = bcdBytesToAsciiStringHelper(body_data + offset, DEC_LOC_BCD_LEN, DEC_LOC_DIGITS);
    if (dec_loc_str.empty() && DEC_LOC_DIGITS > 0) { return false; }
    try {
        out_msg.decimal_locator = static_cast<uint8_t>(std::stoul(dec_loc_str));
    } catch (const std::exception& e) { /* std::cerr << "Error converting DECIMAL-LOCATOR: " << e.what() << std::endl; */ return false; }
    offset += DEC_LOC_BCD_LEN;

    // 5. STRIKE-PRICE-DECIMAL-LOCATOR: 9(1), Length 1
    const size_t STRIKE_DEC_LOC_BCD_LEN = 1;
    const size_t STRIKE_DEC_LOC_DIGITS = 1;
    std::string strike_dec_loc_str = bcdBytesToAsciiStringHelper(body_data + offset, STRIKE_DEC_LOC_BCD_LEN, STRIKE_DEC_LOC_DIGITS);
    if (strike_dec_loc_str.empty() && STRIKE_DEC_LOC_DIGITS > 0) { return false; }
    try {
        out_msg.strike_price_decimal_locator = static_cast<uint8_t>(std::stoul(strike_dec_loc_str));
    } catch (const std::exception& e) { /* std::cerr << "Error converting STRIKE-PRICE-DECIMAL-LOCATOR: " << e.what() << std::endl; */ return false; }
    offset += STRIKE_DEC_LOC_BCD_LEN;

    // 6. BEGIN-DATE: 9(8), Length 4
    const size_t BEGIN_DATE_BCD_LEN = 4;
    const size_t BEGIN_DATE_DIGITS = 8;
    out_msg.begin_date = bcdBytesToAsciiStringHelper(body_data + offset, BEGIN_DATE_BCD_LEN, BEGIN_DATE_DIGITS);
    if (out_msg.begin_date.empty() && BEGIN_DATE_DIGITS > 0) { return false; }
    offset += BEGIN_DATE_BCD_LEN;

    // 7. END-DATE: 9(8), Length 4
    const size_t END_DATE_BCD_LEN = 4;
    const size_t END_DATE_DIGITS = 8;
    out_msg.end_date = bcdBytesToAsciiStringHelper(body_data + offset, END_DATE_BCD_LEN, END_DATE_DIGITS);
    if (out_msg.end_date.empty() && END_DATE_DIGITS > 0) { return false; }
    offset += END_DATE_BCD_LEN;

    // 8. FLOW-GROUP: 9(2), Length 1
    const size_t FLOW_GROUP_BCD_LEN = 1;
    const size_t FLOW_GROUP_DIGITS = 2;
    std::string flow_group_str = bcdBytesToAsciiStringHelper(body_data + offset, FLOW_GROUP_BCD_LEN, FLOW_GROUP_DIGITS);
    if (flow_group_str.empty() && FLOW_GROUP_DIGITS > 0) { return false; }
    try {
        out_msg.flow_group = static_cast<uint8_t>(std::stoul(flow_group_str));
    } catch (const std::exception& e) { /* std::cerr << "Error converting FLOW-GROUP: " << e.what() << std::endl; */ return false; }
    offset += FLOW_GROUP_BCD_LEN;

    // 9. DELIVERY-DATE: 9(8), Length 4
    const size_t DELIVERY_DATE_BCD_LEN = 4;
    const size_t DELIVERY_DATE_DIGITS = 8;
    out_msg.delivery_date = bcdBytesToAsciiStringHelper(body_data + offset, DELIVERY_DATE_BCD_LEN, DELIVERY_DATE_DIGITS);
    if (out_msg.delivery_date.empty() && DELIVERY_DATE_DIGITS > 0) { return false; }
    offset += DELIVERY_DATE_BCD_LEN;

    // 10. DYNAMIC-BANDING: X(1), Length 1
    const size_t DYNAMIC_BANDING_LEN = 1;
    out_msg.dynamic_banding = static_cast<char>(body_data[offset]);
    offset += DYNAMIC_BANDING_LEN;

    // At this point, offset should be EXPECTED_MIN_BODY_LENGTH (32)
    // If body_length is greater, the remaining bytes are not parsed by this function
    // (could be CHECK-SUM and TERMINAL-CODE if they are included in body_length).

    return true;
}

} // namespace SpecificMessageParsers
