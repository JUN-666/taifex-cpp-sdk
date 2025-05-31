#include "messages/message_i010.h"
#include "messages/message_parser_utils.h" // For bcdBytesToAsciiStringHelper (though not directly used by test logic, parser uses it)
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <iomanip> // For std::hex

// It's assumed CoreUtils::asciiToPackBcd is not directly available here for test data setup.
// So, BCD data will be manually specified in byte arrays.

void test_parse_i010_valid_message() {
    std::cout << "Running test_parse_i010_valid_message..." << std::endl;

    SpecificMessageParsers::MessageI010 msg_i010;

    // Manually prepared I010 body
    // PROD-ID-S (10): "PROD123456"
    // REFERENCE-PRICE (9(9) L5): 123456789 (BCD: 0x01 0x23 0x45 0x67 0x89)
    // PROD-KIND (1): 'S' (Stock)
    // DECIMAL-LOCATOR (9(1) L1): 2 (BCD: 0x02)
    // STRIKE-PRICE-DECIMAL-LOCATOR (9(1) L1): 0 (BCD: 0x00)
    // BEGIN-DATE (9(8) L4): "20230115" (BCD: 0x20 0x23 0x01 0x15)
    // END-DATE (9(8) L4): "20241231" (BCD: 0x20 0x24 0x12 0x31)
    // FLOW-GROUP (9(2) L1): 5 (BCD: 0x05)
    // DELIVERY-DATE (9(8) L4): "20250320" (BCD: 0x20 0x25 0x03 0x20)
    // DYNAMIC-BANDING (1): 'Y'
    // Total length = 10 + 5 + 1 + 1 + 1 + 4 + 4 + 1 + 4 + 1 = 32 bytes

    const unsigned char body_data[] = {
        // PROD-ID-S: "PROD123456" (10 bytes)
        'P', 'R', 'O', 'D', '1', '2', '3', '4', '5', '6',
        // REFERENCE-PRICE: 123456789 (BCD: 0x01 0x23 0x45 0x67 0x89) (5 bytes)
        0x01, 0x23, 0x45, 0x67, 0x89,
        // PROD-KIND: 'S' (1 byte)
        'S',
        // DECIMAL-LOCATOR: 2 (BCD: 0x02) (1 byte)
        0x02,
        // STRIKE-PRICE-DECIMAL-LOCATOR: 0 (BCD: 0x00) (1 byte)
        0x00,
        // BEGIN-DATE: "20230115" (BCD: 0x20 0x23 0x01 0x15) (4 bytes)
        0x20, 0x23, 0x01, 0x15,
        // END-DATE: "20241231" (BCD: 0x20 0x24 0x12 0x31) (4 bytes)
        0x20, 0x24, 0x12, 0x31,
        // FLOW-GROUP: 5 (BCD: 0x05) (1 byte)
        0x05,
        // DELIVERY-DATE: "20250320" (BCD: 0x20 0x25 0x03 0x20) (4 bytes)
        0x20, 0x25, 0x03, 0x20,
        // DYNAMIC-BANDING: 'Y' (1 byte)
        'Y'
    };
    uint16_t body_length = sizeof(body_data);

    bool success = SpecificMessageParsers::parse_i010_body(body_data, body_length, msg_i010);
    assert(success == true);

    assert(msg_i010.prod_id_s == "PROD123456");
    assert(msg_i010.reference_price == 123456789LL);
    assert(msg_i010.prod_kind == 'S');
    assert(msg_i010.decimal_locator == 2);
    assert(msg_i010.strike_price_decimal_locator == 0);
    assert(msg_i010.begin_date == "20230115");
    assert(msg_i010.end_date == "20241231");
    assert(msg_i010.flow_group == 5);
    assert(msg_i010.delivery_date == "20250320");
    assert(msg_i010.dynamic_banding == 'Y');

    std::cout << "test_parse_i010_valid_message PASSED." << std::endl;
}

void test_parse_i010_invalid_length() {
    std::cout << "Running test_parse_i010_invalid_length..." << std::endl;
    SpecificMessageParsers::MessageI010 msg_i010;
    const unsigned char body_data[] = { 'P', 'R', 'O', 'D' }; // Too short
    uint16_t body_length = sizeof(body_data);

    bool success = SpecificMessageParsers::parse_i010_body(body_data, body_length, msg_i010);
    assert(success == false);
    std::cout << "test_parse_i010_invalid_length PASSED." << std::endl;
}


int main() {
    test_parse_i010_valid_message();
    test_parse_i010_invalid_length();
    // Add more test cases as needed
    std::cout << "All I010 tests completed." << std::endl;
    return 0;
}
