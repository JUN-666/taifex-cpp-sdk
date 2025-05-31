#include "messages/message_i083.h"
// #include "messages/message_parser_utils.h" // Not directly used by test logic, but parser uses it
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <iomanip> // For std::hex

// Using SpecificMessageParsers namespace for convenience if preferred, or qualify explicitly.
using namespace SpecificMessageParsers;

void test_parse_i083_valid_multiple_entries() {
    std::cout << "Running test_parse_i083_valid_multiple_entries..." << std::endl;
    MessageI083 msg_i083;

    // PROD-ID (20): "TESTPROD012345678901"
    // PROD-MSG-SEQ (9(10) L5): 12345 (BCD: 0x00 0x00 0x01 0x23 0x45)
    // CALCULATED-FLAG (1): '0'
    // NO-MD-ENTRIES (9(2) L1): 2 (BCD: 0x02)
    // Entry 1:
    //   MD-ENTRY-TYPE (1): '0' (Buy)
    //   SIGN (1): '0' (Positive)
    //   MD-ENTRY-PX (9(9) L5): 15000 (BCD: 0x00 0x00 0x01 0x50 0x00)
    //   MD-ENTRY-SIZE (9(8) L4): 100 (BCD: 0x00 0x00 0x01 0x00)
    //   MD-PRICE-LEVEL (9(2) L1): 1 (BCD: 0x01)
    // Entry 2:
    //   MD-ENTRY-TYPE (1): '1' (Sell)
    //   SIGN (1): '0' (Positive)
    //   MD-ENTRY-PX (9(9) L5): 15200 (BCD: 0x00 0x00 0x01 0x52 0x00)
    //   MD-ENTRY-SIZE (9(8) L4): 50 (BCD: 0x00 0x00 0x00 0x50)
    //   MD-PRICE-LEVEL (9(2) L1): 1 (BCD: 0x01)
    // Fixed part length = 20 + 5 + 1 + 1 = 27
    // Entry length = 1 + 1 + 5 + 4 + 1 = 12
    // Total length = 27 + 2 * 12 = 27 + 24 = 51 bytes

    const unsigned char body_data[] = {
        // PROD-ID: "TESTPROD012345678901" (20 bytes)
        'T', 'E', 'S', 'T', 'P', 'R', 'O', 'D', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1',
        // PROD-MSG-SEQ: 12345 (BCD: 0x00 0x00 0x01 0x23 0x45) (5 bytes)
        0x00, 0x00, 0x01, 0x23, 0x45,
        // CALCULATED-FLAG: '0' (1 byte)
        '0',
        // NO-MD-ENTRIES: 2 (BCD: 0x02) (1 byte)
        0x02,
        // Entry 1 (12 bytes)
        /*TYPE*/ '0', /*SIGN*/ '0', /*PX*/ 0x00, 0x00, 0x01, 0x50, 0x00, /*SIZE*/ 0x00, 0x00, 0x01, 0x00, /*LEVEL*/ 0x01,
        // Entry 2 (12 bytes)
        /*TYPE*/ '1', /*SIGN*/ '0', /*PX*/ 0x00, 0x00, 0x01, 0x52, 0x00, /*SIZE*/ 0x00, 0x00, 0x00, 0x50, /*LEVEL*/ 0x01
    };
    uint16_t body_length = sizeof(body_data);

    bool success = parse_i083_body(body_data, body_length, msg_i083);
    assert(success == true);

    assert(msg_i083.prod_id == "TESTPROD012345678901");
    assert(msg_i083.prod_msg_seq == 12345);
    assert(msg_i083.calculated_flag == '0');
    assert(msg_i083.no_md_entries == 2);
    assert(msg_i083.md_entries.size() == 2);

    // Check Entry 1
    assert(msg_i083.md_entries[0].md_entry_type == '0');
    assert(msg_i083.md_entries[0].sign == '0');
    assert(msg_i083.md_entries[0].md_entry_px == 15000LL);
    assert(msg_i083.md_entries[0].md_entry_size == 100LL);
    assert(msg_i083.md_entries[0].md_price_level == 1);

    // Check Entry 2
    assert(msg_i083.md_entries[1].md_entry_type == '1');
    assert(msg_i083.md_entries[1].sign == '0');
    assert(msg_i083.md_entries[1].md_entry_px == 15200LL);
    assert(msg_i083.md_entries[1].md_entry_size == 50LL);
    assert(msg_i083.md_entries[1].md_price_level == 1);

    std::cout << "test_parse_i083_valid_multiple_entries PASSED." << std::endl;
}

void test_parse_i083_valid_no_entries() {
    std::cout << "Running test_parse_i083_valid_no_entries..." << std::endl;
    MessageI083 msg_i083;

    // PROD-ID (20): "NOPROD00000000000000"
    // PROD-MSG-SEQ (9(10) L5): 1 (BCD: 0x00 0x00 0x00 0x00 0x01)
    // CALCULATED-FLAG (1): '1'
    // NO-MD-ENTRIES (9(2) L1): 0 (BCD: 0x00)
    // Total length = 20 + 5 + 1 + 1 = 27 bytes
    const unsigned char body_data[] = {
        'N', 'O', 'P', 'R', 'O', 'D', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0',
        0x00, 0x00, 0x00, 0x00, 0x01,
        '1',
        0x00
    };
    uint16_t body_length = sizeof(body_data);

    bool success = parse_i083_body(body_data, body_length, msg_i083);
    assert(success == true);
    assert(msg_i083.prod_id == "NOPROD00000000000000");
    assert(msg_i083.prod_msg_seq == 1);
    assert(msg_i083.calculated_flag == '1');
    assert(msg_i083.no_md_entries == 0);
    assert(msg_i083.md_entries.empty() == true);

    std::cout << "test_parse_i083_valid_no_entries PASSED." << std::endl;
}

void test_parse_i083_invalid_length_for_entries() {
    std::cout << "Running test_parse_i083_invalid_length_for_entries..." << std::endl;
    MessageI083 msg_i083;
    // Fixed part is okay, NO-MD-ENTRIES is 2, but data only for 1 entry
    // Total length = 27 (fixed) + 12 (1 entry) = 39. Expected 27 + 24 = 51
    const unsigned char body_data[] = {
        // PROD-ID (20 bytes)
        'S', 'H', 'O', 'R', 'T', 'P', 'R', 'O', 'D', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
        // PROD-MSG-SEQ (5 bytes)
        0x00, 0x00, 0x00, 0x00, 0x02,
        // CALCULATED-FLAG (1 byte)
        '0',
        // NO-MD-ENTRIES (1 byte) : 2 (BCD: 0x02)
        0x02,
        // Entry 1 (12 bytes) - Complete
        /*TYPE*/ '0', /*SIGN*/ '0', /*PX*/ 0x00, 0x00, 0x01, 0x50, 0x00, /*SIZE*/ 0x00, 0x00, 0x01, 0x00, /*LEVEL*/ 0x01
        // Missing data for Entry 2
    };
    uint16_t body_length = sizeof(body_data); // Should be 39

    bool success = parse_i083_body(body_data, body_length, msg_i083);
    assert(success == false); // Should fail because body_length is too short for 2 entries

    std::cout << "test_parse_i083_invalid_length_for_entries PASSED." << std::endl;
}

void test_parse_i083_invalid_fixed_length() {
    std::cout << "Running test_parse_i083_invalid_fixed_length..." << std::endl;
    MessageI083 msg_i083;
    const unsigned char body_data[] = { 'S', 'H', 'O', 'R', 'T' }; // Way too short (5 bytes)
    uint16_t body_length = sizeof(body_data);

    bool success = parse_i083_body(body_data, body_length, msg_i083);
    assert(success == false); // Should fail, minimum fixed part is 27 bytes
    std::cout << "test_parse_i083_invalid_fixed_length PASSED." << std::endl;
}


int main() {
    test_parse_i083_valid_multiple_entries();
    test_parse_i083_valid_no_entries();
    test_parse_i083_invalid_length_for_entries();
    test_parse_i083_invalid_fixed_length();

    std::cout << "All I083 tests completed." << std::endl;
    return 0;
}
