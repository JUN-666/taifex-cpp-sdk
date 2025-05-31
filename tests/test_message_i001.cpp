#include "messages/message_i001.h"
#include <iostream>
#include <vector>
#include <cassert>

// Using SpecificMessageParsers namespace for convenience
using namespace SpecificMessageParsers;

void test_parse_i001_empty_body() {
    std::cout << "Running test_parse_i001_empty_body (length 0)..." << std::endl;
    MessageI001 msg_i001;
    const unsigned char* body_data = nullptr; // No data for length 0
    uint16_t body_length = 0;

    bool success = parse_i001_body(body_data, body_length, msg_i001);
    assert(success == true);
    std::cout << "test_parse_i001_empty_body (length 0) PASSED." << std::endl;
}

void test_parse_i001_body_with_checksum_terminator() {
    std::cout << "Running test_parse_i001_body_with_checksum_terminator (length 3)..." << std::endl;
    MessageI001 msg_i001;
    // Dummy data, parser doesn't inspect it, only length matters for this message type
    const unsigned char body_data[] = {0xDA, 0xDB, 0xDC};
    uint16_t body_length = 3;

    bool success = parse_i001_body(body_data, body_length, msg_i001);
    assert(success == true);
    std::cout << "test_parse_i001_body_with_checksum_terminator (length 3) PASSED." << std::endl;
}

void test_parse_i001_invalid_length_short() {
    std::cout << "Running test_parse_i001_invalid_length_short (length 1)..." << std::endl;
    MessageI001 msg_i001;
    const unsigned char body_data[] = {0xDA};
    uint16_t body_length = 1;

    bool success = parse_i001_body(body_data, body_length, msg_i001);
    assert(success == false);
    std::cout << "test_parse_i001_invalid_length_short (length 1) PASSED." << std::endl;
}

void test_parse_i001_invalid_length_long() {
    std::cout << "Running test_parse_i001_invalid_length_long (length 4)..." << std::endl;
    MessageI001 msg_i001;
    const unsigned char body_data[] = {0xDA, 0xDB, 0xDC, 0xDD};
    uint16_t body_length = 4;

    bool success = parse_i001_body(body_data, body_length, msg_i001);
    assert(success == false);
    std::cout << "test_parse_i001_invalid_length_long (length 4) PASSED." << std::endl;
}

int main() {
    test_parse_i001_empty_body();
    test_parse_i001_body_with_checksum_terminator();
    test_parse_i001_invalid_length_short();
    test_parse_i001_invalid_length_long();

    std::cout << "All I001 tests completed." << std::endl;
    return 0;
}
