#include "messages/message_i081.h"
// #include "messages/message_parser_utils.h" // Not directly used by test logic, but parser uses it
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <iomanip> // For std::hex

// Using SpecificMessageParsers namespace for convenience
using namespace SpecificMessageParsers;

void test_parse_i081_valid_multiple_entries() {
    std::cout << "Running test_parse_i081_valid_multiple_entries..." << std::endl;
    MessageI081 msg_i081;

    // PROD-ID (20): "UPDATEPROD0123456789"
    // PROD-MSG-SEQ (9(10) L5): 54321 (BCD: 0x00 0x00 0x05 0x43 0x21)
    // NO-MD-ENTRIES (9(2) L1): 2 (BCD: 0x02)
    // Entry 1:
    //   MD-UPDATE-ACTION (1): '0' (New)
    //   MD-ENTRY-TYPE (1): '0' (Buy)
    //   SIGN (1): '0' (Positive)
    //   MD-ENTRY-PX (9(9) L5): 25000 (BCD: 0x00 0x00 0x02 0x50 0x00)
    //   MD-ENTRY-SIZE (9(8) L4): 75 (BCD: 0x00 0x00 0x00 0x75)
    //   MD-PRICE-LEVEL (9(2) L1): 1 (BCD: 0x01)
    // Entry 2:
    //   MD-UPDATE-ACTION (1): '1' (Change)
    //   MD-ENTRY-TYPE (1): '1' (Sell)
    //   SIGN (1): '0' (Positive)
    //   MD-ENTRY-PX (9(9) L5): 25200 (BCD: 0x00 0x00 0x02 0x52 0x00)
    //   MD-ENTRY-SIZE (9(8) L4): 25 (BCD: 0x00 0x00 0x00 0x25)
    //   MD-PRICE-LEVEL (9(2) L1): 1 (BCD: 0x01)
    // Fixed part length = 20 + 5 + 1 = 26
    // Entry length = 1 + 1 + 1 + 5 + 4 + 1 = 13
    // Total length = 26 + 2 * 13 = 26 + 26 = 52 bytes

    const unsigned char body_data[] = {
        // PROD-ID: "UPDATEPROD0123456789" (20 bytes)
        'U', 'P', 'D', 'A', 'T', 'E', 'P', 'R', 'O', 'D', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        // PROD-MSG-SEQ: 54321 (BCD: 0x00 0x00 0x05 0x43 0x21) (5 bytes)
        0x00, 0x00, 0x05, 0x43, 0x21,
        // NO-MD-ENTRIES: 2 (BCD: 0x02) (1 byte)
        0x02,
        // Entry 1 (13 bytes)
        /*ACTION*/'0', /*TYPE*/ '0', /*SIGN*/ '0', /*PX*/ 0x00, 0x00, 0x02, 0x50, 0x00, /*SIZE*/ 0x00, 0x00, 0x00, 0x75, /*LEVEL*/ 0x01,
        // Entry 2 (13 bytes)
        /*ACTION*/'1', /*TYPE*/ '1', /*SIGN*/ '0', /*PX*/ 0x00, 0x00, 0x02, 0x52, 0x00, /*SIZE*/ 0x00, 0x00, 0x00, 0x25, /*LEVEL*/ 0x01
    };
    uint16_t body_length = sizeof(body_data);

    bool success = parse_i081_body(body_data, body_length, msg_i081);
    assert(success == true);

    assert(msg_i081.prod_id == "UPDATEPROD0123456789");
    assert(msg_i081.prod_msg_seq == 54321);
    assert(msg_i081.no_md_entries == 2);
    assert(msg_i081.md_entries.size() == 2);

    // Check Entry 1
    assert(msg_i081.md_entries[0].md_update_action == '0');
    assert(msg_i081.md_entries[0].md_entry_type == '0');
    assert(msg_i081.md_entries[0].sign == '0');
    assert(msg_i081.md_entries[0].md_entry_px == 25000LL);
    assert(msg_i081.md_entries[0].md_entry_size == 75LL);
    assert(msg_i081.md_entries[0].md_price_level == 1);

    // Check Entry 2
    assert(msg_i081.md_entries[1].md_update_action == '1');
    assert(msg_i081.md_entries[1].md_entry_type == '1');
    assert(msg_i081.md_entries[1].sign == '0');
    assert(msg_i081.md_entries[1].md_entry_px == 25200LL);
    assert(msg_i081.md_entries[1].md_entry_size == 25LL);
    assert(msg_i081.md_entries[1].md_price_level == 1);

    std::cout << "test_parse_i081_valid_multiple_entries PASSED." << std::endl;
}

void test_parse_i081_valid_no_entries() {
    std::cout << "Running test_parse_i081_valid_no_entries..." << std::endl;
    MessageI081 msg_i081;

    // PROD-ID (20): "EMPTYPROD00000000000"
    // PROD-MSG-SEQ (9(10) L5): 10 (BCD: 0x00 0x00 0x00 0x00 0x10)
    // NO-MD-ENTRIES (9(2) L1): 0 (BCD: 0x00)
    // Total length = 20 + 5 + 1 = 26 bytes
    const unsigned char body_data[] = {
        'E', 'M', 'P', 'T', 'Y', 'P', 'R', 'O', 'D', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0',
        0x00, 0x00, 0x00, 0x00, 0x10,
        0x00
    };
    uint16_t body_length = sizeof(body_data);

    bool success = parse_i081_body(body_data, body_length, msg_i081);
    assert(success == true);
    assert(msg_i081.prod_id == "EMPTYPROD00000000000");
    assert(msg_i081.prod_msg_seq == 10);
    assert(msg_i081.no_md_entries == 0);
    assert(msg_i081.md_entries.empty() == true);

    std::cout << "test_parse_i081_valid_no_entries PASSED." << std::endl;
}

void test_parse_i081_invalid_length_for_entries() {
    std::cout << "Running test_parse_i081_invalid_length_for_entries..." << std::endl;
    MessageI081 msg_i081;
    // Fixed part is okay, NO-MD-ENTRIES is 2, but data only for 1 entry
    // Total length = 26 (fixed) + 13 (1 entry) = 39. Expected 26 + 2*13 = 52
    const unsigned char body_data[] = {
        // PROD-ID (20 bytes)
        'L', 'E', 'N', 'G', 'T', 'H', 'E', 'R', 'R', 'P', 'R', 'O', 'D', '0', '1', '2', '3', '4', '5', '6',
        // PROD-MSG-SEQ (5 bytes)
        0x00, 0x00, 0x00, 0x00, 0x03,
        // NO-MD-ENTRIES (1 byte) : 2 (BCD: 0x02)
        0x02,
        // Entry 1 (13 bytes) - Complete
        /*ACTION*/'0', /*TYPE*/ '0', /*SIGN*/ '0', /*PX*/ 0x00, 0x00, 0x01, 0x50, 0x00, /*SIZE*/ 0x00, 0x00, 0x01, 0x00, /*LEVEL*/ 0x01
        // Missing data for Entry 2
    };
    uint16_t body_length = sizeof(body_data); // Should be 39

    bool success = parse_i081_body(body_data, body_length, msg_i081);
    assert(success == false); // Should fail because body_length is too short for 2 entries

    std::cout << "test_parse_i081_invalid_length_for_entries PASSED." << std::endl;
}

void test_parse_i081_invalid_fixed_length() {
    std::cout << "Running test_parse_i081_invalid_fixed_length..." << std::endl;
    MessageI081 msg_i081;
    const unsigned char body_data[] = { 'T', 'O', 'O', 'S', 'H', 'O', 'R', 'T' }; // Way too short (8 bytes)
    uint16_t body_length = sizeof(body_data);

    bool success = parse_i081_body(body_data, body_length, msg_i081);
    assert(success == false); // Should fail, minimum fixed part is 26 bytes
    std::cout << "test_parse_i081_invalid_fixed_length PASSED." << std::endl;
}


int main() {
    test_parse_i081_valid_multiple_entries();
    test_parse_i081_valid_no_entries();
    test_parse_i081_invalid_length_for_entries();
    test_parse_i081_invalid_fixed_length();

    std::cout << "All I081 tests completed." << std::endl;
    return 0;
}
