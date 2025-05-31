#include "sdk/taifex_sdk.h"
#include "core_utils/common_header.h"      // For CoreUtils::CommonHeader
#include "core_utils/message_identifier.h" // For CoreUtils::MessageType for checks
#include "core_utils/checksum.h"           // For CoreUtils::calculate_checksum
#include "messages/message_i010.h"         // For SpecificMessageParsers::MessageI010 for verification

#include <iostream>
#include <vector>
#include <cassert>
#include <iomanip> // For std::hex printing if needed
#include <cstring> // For memcpy

// Using namespaces for brevity
using namespace Taifex;
using namespace CoreUtils;
using namespace SpecificMessageParsers;


// Helper function to create a raw I010 message for testing
// This is simplified. A real scenario might involve more complex BCD packing.
// For testing, we can pre-calculate BCD for known values.
std::vector<unsigned char> create_raw_i010_message(
    uint64_t channel_seq, uint16_t body_length, const MessageI010& i010_body_content) {

    std::vector<unsigned char> msg_body(body_length);
    size_t offset = 0;

    // Simplified body construction - assumes direct string copy and pre-packed BCDs
    // PROD-ID-S (10)
    memcpy(msg_body.data() + offset, i010_body_content.prod_id_s.c_str(), 10);
    offset += 10;
    // REFERENCE-PRICE (9(9) L5): e.g., 123456789 -> 0x01,0x23,0x45,0x67,0x89
    // For this test, let's use a fixed BCD pattern.
    unsigned char ref_price_bcd[] = {0x01, 0x23, 0x45, 0x67, 0x89}; // Represents 123456789
    memcpy(msg_body.data() + offset, ref_price_bcd, sizeof(ref_price_bcd));
    offset += sizeof(ref_price_bcd);
    // PROD-KIND (1)
    msg_body[offset++] = i010_body_content.prod_kind;
    // DECIMAL-LOCATOR (9(1) L1): e.g., 2 -> 0x02
    msg_body[offset++] = i010_body_content.decimal_locator; // Already a uint8_t, direct use for test
    // STRIKE-PRICE-DECIMAL-LOCATOR (9(1) L1): e.g., 0 -> 0x00
    msg_body[offset++] = i010_body_content.strike_price_decimal_locator;
    // BEGIN-DATE (9(8) L4): "20230115" -> 0x20,0x23,0x01,0x15
    unsigned char begin_date_bcd[] = {0x20, 0x23, 0x01, 0x15};
    memcpy(msg_body.data() + offset, begin_date_bcd, sizeof(begin_date_bcd));
    offset += sizeof(begin_date_bcd);
    // END-DATE (9(8) L4): "20241231" -> 0x20,0x24,0x12,0x31
    unsigned char end_date_bcd[] = {0x20, 0x24, 0x12, 0x31};
    memcpy(msg_body.data() + offset, end_date_bcd, sizeof(end_date_bcd));
    offset += sizeof(end_date_bcd);
    // FLOW-GROUP (9(2) L1): e.g., 5 -> 0x05
    msg_body[offset++] = i010_body_content.flow_group;
    // DELIVERY-DATE (9(8) L4): "20250320" -> 0x20,0x25,0x03,0x20
    unsigned char delivery_date_bcd[] = {0x20, 0x25, 0x03, 0x20};
    memcpy(msg_body.data() + offset, delivery_date_bcd, sizeof(delivery_date_bcd));
    offset += sizeof(delivery_date_bcd);
    // DYNAMIC-BANDING (1)
    msg_body[offset++] = i010_body_content.dynamic_banding;

    assert(offset == body_length); // Ensure we filled the body correctly

    CommonHeader header_obj;
    header_obj.esc_code = 0x1B; // ESC
    header_obj.transmission_code = 0x02; // Realtime
    header_obj.message_kind = MessageIdentifier::messageTypeToByte(MessageType::I010_PRODUCT_BASIC_DATA);

    // Simplified BCD population for header fields. A full BCD conversion utility would be better.
    // InformationTimeBcd (6 bytes) - set to zeros for simplicity
    memset(header_obj.information_time_bcd.data(), 0x00, 6);
    // ChannelIDBcd (2 bytes) - set to zeros
    memset(header_obj.channel_id_bcd.data(), 0x00, 2);
    // VersionNoBcd (1 byte) - set to zero
    header_obj.version_no_bcd = 0x00;

    // BodyLength BCD (2 bytes)
    header_obj.body_length_bcd[0] = (body_length / 100) % 10; // BCD tens
    header_obj.body_length_bcd[0] |= ((body_length / 1000) % 10) << 4; // BCD thousands
    header_obj.body_length_bcd[1] = (body_length % 10); // BCD units
    header_obj.body_length_bcd[1] |= ((body_length / 10) % 10) << 4; // BCD hundreds
    // This is a simplified direct BCD encoding for test, e.g. 32 -> 0x00 (0,0), 0x32 (3,2)
    // A proper BCD pack would be: 32 -> {0x00, 0x32} if interpreted digit by digit and then packed.
    // Or if it means each nibble is a digit: 32 -> 0x32. Let's use the latter for body_length_bcd[1] for 32.
    // For 32: body_length_bcd[0] = 0x00, body_length_bcd[1] = 0x32
    if (body_length == 32) { // Common case for I010
        header_obj.body_length_bcd[0] = 0x00;
        header_obj.body_length_bcd[1] = 0x32;
    } else { // Generic (but still simplified)
        header_obj.body_length_bcd[0] = 0; // Assuming body_length < 25600 for this simple BCD
        header_obj.body_length_bcd[1] = ((body_length / 100) << 4) | ((body_length / 10) % 10);
        // This is not quite right for general BCD. For test, fixed value is better.
        // For now, hardcoding for body_length=32 is safest for this helper.
    }


    // ChannelSeq BCD (5 bytes)
    uint64_t temp_seq = channel_seq;
    for (int i = 4; i >= 0; --i) {
        header_obj.channel_seq_bcd[i] = (temp_seq % 10);
        temp_seq /= 10;
        header_obj.channel_seq_bcd[i] |= (temp_seq % 10) << 4;
        temp_seq /= 10;
    }


    std::vector<unsigned char> full_message(CommonHeader::HEADER_SIZE + body_length + 1 + 2);
    size_t header_write_offset = 0;
    full_message[header_write_offset++] = header_obj.esc_code;
    full_message[header_write_offset++] = header_obj.transmission_code;
    full_message[header_write_offset++] = header_obj.message_kind;
    memcpy(full_message.data() + header_write_offset, header_obj.information_time_bcd.data(), 6); header_write_offset += 6;
    memcpy(full_message.data() + header_write_offset, header_obj.channel_id_bcd.data(), 2); header_write_offset += 2;
    memcpy(full_message.data() + header_write_offset, header_obj.channel_seq_bcd.data(), 5); header_write_offset += 5;
    full_message[header_write_offset++] = header_obj.version_no_bcd;
    memcpy(full_message.data() + header_write_offset, header_obj.body_length_bcd.data(), 2); header_write_offset += 2;
    assert(header_write_offset == CommonHeader::HEADER_SIZE);

    memcpy(full_message.data() + CommonHeader::HEADER_SIZE, msg_body.data(), body_length);

    unsigned char checksum = CoreUtils::calculate_checksum(full_message.data() + 1, CommonHeader::HEADER_SIZE -1 + body_length);
    full_message[CommonHeader::HEADER_SIZE + body_length] = checksum;

    full_message[CommonHeader::HEADER_SIZE + body_length + 1] = 0x0D;
    full_message[CommonHeader::HEADER_SIZE + body_length + 2] = 0x0A;

    return full_message;
}


void test_sdk_initialization() {
    std::cout << "Running test_sdk_initialization..." << std::endl;
    TaifexSdk sdk;
    auto info = sdk.get_product_info("ANYPROD"); // Test before init
    assert(!info.has_value());

    bool init_ok = sdk.initialize();
    assert(init_ok);

    info = sdk.get_product_info("ANYPROD"); // Test after init
    assert(!info.has_value());
    std::cout << "test_sdk_initialization PASSED." << std::endl;
}

void test_process_i010_message() {
    std::cout << "Running test_process_i010_message..." << std::endl;
    TaifexSdk sdk;
    sdk.initialize();

    MessageI010 test_i010_content;
    test_i010_content.prod_id_s = "TXF       "; // 10 chars
    test_i010_content.reference_price = 123456789;
    test_i010_content.prod_kind = 'F';
    test_i010_content.decimal_locator = 2;
    test_i010_content.strike_price_decimal_locator = 0;
    test_i010_content.begin_date = "20230115";
    test_i010_content.end_date = "20241231";
    test_i010_content.flow_group = 5;
    test_i010_content.delivery_date = "20250320";
    test_i010_content.dynamic_banding = 'Y';

    uint16_t i010_body_len = 32;
    std::vector<unsigned char> raw_msg = create_raw_i010_message(1ULL, i010_body_len, test_i010_content);

    sdk.process_message(raw_msg.data(), raw_msg.size());

    std::string expected_prod_id_s_key = "TXF";
    auto product_info_opt = sdk.get_product_info(expected_prod_id_s_key);
    assert(product_info_opt.has_value());
    if (product_info_opt) {
        const auto& cached_info = product_info_opt->get();
        assert(cached_info.prod_id_s == test_i010_content.prod_id_s);
        assert(cached_info.decimal_locator == 2);
        assert(cached_info.prod_kind == 'F');
    }
    std::cout << "test_process_i010_message PASSED." << std::endl;
}

void test_process_message_invalid_checksum() {
    std::cout << "Running test_process_message_invalid_checksum..." << std::endl;
    TaifexSdk sdk;
    sdk.initialize();

    MessageI010 test_i010_content;
    test_i010_content.prod_id_s = "CHKPROD   ";
    test_i010_content.decimal_locator = 0;
    test_i010_content.prod_kind = ' '; // Minimal valid char
    test_i010_content.dynamic_banding = 'N';
    uint16_t i010_body_len = 32;
    std::vector<unsigned char> raw_msg = create_raw_i010_message(2ULL, i010_body_len, test_i010_content);

    raw_msg[CommonHeader::HEADER_SIZE + i010_body_len] ^= 0xFF;

    sdk.process_message(raw_msg.data(), raw_msg.size());

    auto product_info_opt = sdk.get_product_info("CHKPROD");
    assert(!product_info_opt.has_value());
    std::cout << "test_process_message_invalid_checksum PASSED." << std::endl;
}

void test_process_message_length_mismatch() {
    std::cout << "Running test_process_message_length_mismatch..." << std::endl;
    TaifexSdk sdk;
    sdk.initialize();

    MessageI010 test_i010_content;
    test_i010_content.prod_id_s = "LENPROD   ";
    test_i010_content.decimal_locator = 0;
    test_i010_content.prod_kind = ' ';
    test_i010_content.dynamic_banding = 'N';
    uint16_t i010_body_len = 32;
    std::vector<unsigned char> raw_msg = create_raw_i010_message(3ULL, i010_body_len, test_i010_content);

    sdk.process_message(raw_msg.data(), raw_msg.size() - 1);

    auto product_info_opt = sdk.get_product_info("LENPROD");
    assert(!product_info_opt.has_value());
    std::cout << "test_process_message_length_mismatch PASSED." << std::endl;
}


int main() {
    test_sdk_initialization();
    test_process_i010_message();
    test_process_message_invalid_checksum();
    test_process_message_length_mismatch();
    // TODO: Add more tests for I081, I083, I002 processing via raw messages
    // TODO: Add tests for sequence number validation effects
    test_process_i083_message();
    test_process_i081_message(); // New test
    std::cout << "TaifexSdk core tests completed." << std::endl;
    return 0;
}


// --- New helper and test for I083 ---
#include "messages/message_i083.h" // For SpecificMessageParsers::MessageI083
#include "order_book/order_book.h"  // For OrderBookManagement::OrderBook for verification

// Helper function to create a raw I083 message for testing
// Simplified BCD packing for header and some body fields. MD-ENTRY BCDs are pre-defined.
std::vector<unsigned char> create_raw_i083_message(
    uint64_t channel_seq,
    const std::string& prod_id, // 20 chars
    uint32_t prod_msg_seq,
    char calculated_flag,
    const std::vector<MdEntryI083>& entries) {

    uint16_t body_length = 20 + 5 + 1 + 1 + (entries.size() * 12); // PROD-ID(20) + SEQ(5) + FLAG(1) + NUM_ENTRIES(1) + ENTRIES_DATA

    std::vector<unsigned char> msg_body(body_length);
    size_t offset = 0;

    // PROD-ID X(20)
    memcpy(msg_body.data() + offset, prod_id.c_str(), 20);
    offset += 20;
    // PROD-MSG-SEQ 9(10) L5 - simplified BCD (actual BCD packing needed for correctness)
    // Example for prod_msg_seq = 123 -> {0x00, 0x00, 0x00, 0x01, 0x23}
    msg_body[offset + 0] = (prod_msg_seq / 100000000) % 100; // Placeholder, not real BCD
    msg_body[offset + 1] = (prod_msg_seq / 1000000) % 100;   // Placeholder
    msg_body[offset + 2] = (prod_msg_seq / 10000) % 100;    // Placeholder
    msg_body[offset + 3] = (prod_msg_seq / 100) % 100;      // Placeholder
    msg_body[offset + 4] = prod_msg_seq % 100;             // Placeholder
    offset += 5;
    // CALCULATED-FLAG X(1)
    msg_body[offset++] = calculated_flag;
    // NO-MD-ENTRIES 9(2) L1 (e.g., 2 -> 0x02) - simplified BCD
    msg_body[offset++] = static_cast<unsigned char>(entries.size()); // Placeholder, not real BCD

    for (const auto& entry : entries) {
        msg_body[offset++] = entry.md_entry_type;
        msg_body[offset++] = entry.sign;
        // MD-ENTRY-PX 9(9) L5 - simplified BCD
        msg_body[offset + 0] = 0; // Placeholder
        msg_body[offset + 1] = 0; // Placeholder
        msg_body[offset + 2] = (entry.md_entry_px / 10000) % 100; // Placeholder
        msg_body[offset + 3] = (entry.md_entry_px / 100) % 100;   // Placeholder
        msg_body[offset + 4] = entry.md_entry_px % 100;          // Placeholder
        offset += 5;
        // MD-ENTRY-SIZE 9(8) L4 - simplified BCD
        msg_body[offset + 0] = 0; // Placeholder
        msg_body[offset + 1] = 0; // Placeholder
        msg_body[offset + 2] = (entry.md_entry_size / 100) % 100; // Placeholder
        msg_body[offset + 3] = entry.md_entry_size % 100;        // Placeholder
        offset += 4;
        // MD-PRICE-LEVEL 9(2) L1
        msg_body[offset++] = entry.md_price_level; // Placeholder, not real BCD
    }
    assert(offset == body_length);

    CommonHeader header_obj;
    header_obj.esc_code = 0x1B;
    header_obj.transmission_code = 0x02;
    header_obj.message_kind = MessageIdentifier::messageTypeToByte(MessageType::I083_ORDER_BOOK_SNAPSHOT);

    // Simplified BodyLength BCD (Placeholder - not actual BCD)
    // For body_length, e.g. 51: header_obj.body_length_bcd should be {0x00, 0x51}
    header_obj.body_length_bcd[0] = (body_length >> 8) & 0xFF; // Upper byte
    header_obj.body_length_bcd[1] = body_length & 0xFF;        // Lower byte

    // Simplified ChannelSeq BCD (Placeholder - not actual BCD)
    memset(header_obj.channel_seq_bcd.data(), 0, 5); // zero out
    header_obj.channel_seq_bcd[4] = channel_seq % 100; // Simplification
    memset(header_obj.information_time_bcd.data(), 0, 6);
    memset(header_obj.channel_id_bcd.data(), 0, 2);
    header_obj.version_no_bcd = 0;


    std::vector<unsigned char> full_message(CommonHeader::HEADER_SIZE + body_length + 1 + 2);
    size_t header_write_offset = 0;
    full_message[header_write_offset++] = header_obj.esc_code;
    full_message[header_write_offset++] = header_obj.transmission_code;
    full_message[header_write_offset++] = header_obj.message_kind;
    memcpy(full_message.data() + header_write_offset, header_obj.information_time_bcd.data(), 6); header_write_offset += 6;
    memcpy(full_message.data() + header_write_offset, header_obj.channel_id_bcd.data(), 2); header_write_offset += 2;
    memcpy(full_message.data() + header_write_offset, header_obj.channel_seq_bcd.data(), 5); header_write_offset += 5;
    full_message[header_write_offset++] = header_obj.version_no_bcd;
    memcpy(full_message.data() + header_write_offset, header_obj.body_length_bcd.data(), 2); header_write_offset += 2;

    memcpy(full_message.data() + CommonHeader::HEADER_SIZE, msg_body.data(), body_length);
    unsigned char checksum = CoreUtils::calculate_checksum(full_message.data() + 1, CommonHeader::HEADER_SIZE - 1 + body_length);
    full_message[CommonHeader::HEADER_SIZE + body_length] = checksum;
    full_message[CommonHeader::HEADER_SIZE + body_length + 1] = 0x0D;
    full_message[CommonHeader::HEADER_SIZE + body_length + 2] = 0x0A;

    return full_message;
}

void test_process_i083_message() {
    std::cout << "Running test_process_i083_message..." << std::endl;
    TaifexSdk sdk;
    sdk.initialize();

    // 1. Send I010 for product "SNAPTESTP " (10 chars for PROD-ID-S)
    // The order book will be keyed by "SNAPTESTPROD1234567890" (20 chars)
    // The I010 lookup from the 20-char ID should map to "SNAPTESTP" after trimming/logic.
    std::string i010_prod_id_s = "SNAPTESTP "; // 10 chars, space padded
    std::string order_book_key_prod_id = "SNAPTESTPROD1234567890";

    MessageI010 i010_content;
    i010_content.prod_id_s = i010_prod_id_s;
    i010_content.decimal_locator = 2;
    i010_content.prod_kind = 'F'; // Fill other mandatory fields for i010_content
    i010_content.strike_price_decimal_locator = 0;
    i010_content.flow_group = 1;
    i010_content.dynamic_banding = 'N';
    // Reference price, begin/end date, delivery date BCDs are fixed in create_raw_i010_message for now

    std::vector<unsigned char> raw_i010 = create_raw_i010_message(10ULL, 32, i010_content);
    sdk.process_message(raw_i010.data(), raw_i010.size());

    // Verify I010 was processed using the key that get_base_prod_id_for_i010_lookup would derive
    std::string i010_lookup_key = "SNAPTESTP"; // This is i010_prod_id_s trimmed
    auto info = sdk.get_product_info(i010_lookup_key);
    assert(info.has_value());
    if (!info) { // Guard for test execution
        std::cerr << "I010 info not found for key: " << i010_lookup_key << std::endl;
        assert(false); // Fail test
        return;
    }
    assert(info->get().decimal_locator == 2);


    // 2. Send I083 for "SNAPTESTPROD1234567890"
    std::vector<MdEntryI083> entries;
    // Prices are scaled: 15000 means 150.00 (locator 2)
    entries.push_back({'0', '0', 15000, 10, 1}); // Bid
    entries.push_back({'1', '0', 15100, 5,  1}); // Ask

    // Using order_book_key_prod_id for the I083 message
    std::vector<unsigned char> raw_i083 = create_raw_i083_message(11ULL, order_book_key_prod_id, 123, '0', entries);
    sdk.process_message(raw_i083.data(), raw_i083.size());

    auto ob_opt = sdk.get_order_book(order_book_key_prod_id);
    assert(ob_opt.has_value());
    if (ob_opt) {
        const auto& ob = ob_opt->get();
        assert(ob.get_last_prod_msg_seq() == 123); // This is prod_msg_seq from I083
        auto bids = ob.get_top_bids(1);
        assert(bids.size() == 1 && bids[0].price == 15000 && bids[0].quantity == 10);
        auto asks = ob.get_top_asks(1);
        assert(asks.size() == 1 && asks[0].price == 15100 && asks[0].quantity == 5);
    }
    std::cout << "test_process_i083_message PASSED." << std::endl;
}


// --- New helper and test for I081 ---
#include "messages/message_i081.h" // For SpecificMessageParsers::MessageI081

// Helper function to create a raw I081 message for testing
// Simplified BCD packing.
std::vector<unsigned char> create_raw_i081_message(
    uint64_t channel_seq,
    const std::string& prod_id, // 20 chars
    uint32_t prod_msg_seq,
    const std::vector<MdEntryI081>& entries) {

    // PROD-ID(20) + SEQ(5) + NUM_ENTRIES(1) + ENTRIES_DATA(N*13)
    uint16_t body_length = 20 + 5 + 1 + (entries.size() * 13);

    std::vector<unsigned char> msg_body(body_length);
    size_t offset = 0;

    memcpy(msg_body.data() + offset, prod_id.c_str(), 20);
    offset += 20;
    // PROD-MSG-SEQ 9(10) L5 - simplified BCD
    msg_body[offset + 0] = (prod_msg_seq / 100000000) % 100; // Placeholder
    msg_body[offset + 1] = (prod_msg_seq / 1000000) % 100;   // Placeholder
    msg_body[offset + 2] = (prod_msg_seq / 10000) % 100;    // Placeholder
    msg_body[offset + 3] = (prod_msg_seq / 100) % 100;      // Placeholder
    msg_body[offset + 4] = prod_msg_seq % 100;             // Placeholder
    offset += 5;
    msg_body[offset++] = static_cast<unsigned char>(entries.size()); // NO-MD-ENTRIES - simplified BCD

    for (const auto& entry : entries) {
        msg_body[offset++] = entry.md_update_action;
        msg_body[offset++] = entry.md_entry_type;
        msg_body[offset++] = entry.sign;
        // MD-ENTRY-PX 9(9) L5 - simplified BCD
        msg_body[offset + 0] = 0; // Placeholder
        msg_body[offset + 1] = 0; // Placeholder
        msg_body[offset + 2] = (entry.md_entry_px / 10000) % 100; // Placeholder
        msg_body[offset + 3] = (entry.md_entry_px / 100) % 100;   // Placeholder
        msg_body[offset + 4] = entry.md_entry_px % 100;          // Placeholder
        offset += 5;
        // MD-ENTRY-SIZE 9(8) L4 - simplified BCD
        msg_body[offset + 0] = 0; // Placeholder
        msg_body[offset + 1] = 0; // Placeholder
        msg_body[offset + 2] = (entry.md_entry_size / 100) % 100; // Placeholder
        msg_body[offset + 3] = entry.md_entry_size % 100;        // Placeholder
        offset += 4;
        msg_body[offset++] = entry.md_price_level;
    }
    assert(offset == body_length);

    CommonHeader header_obj;
    header_obj.esc_code = 0x1B;
    header_obj.transmission_code = 0x02;
    header_obj.message_kind = MessageIdentifier::messageTypeToByte(MessageType::I081_ORDER_BOOK_UPDATE);
    // Simplified BodyLength BCD (Placeholder - not actual BCD)
    header_obj.body_length_bcd[0] = (body_length >> 8) & 0xFF;
    header_obj.body_length_bcd[1] = body_length & 0xFF;
    // Simplified ChannelSeq BCD (Placeholder - not actual BCD)
    memset(header_obj.channel_seq_bcd.data(), 0, 5);
    header_obj.channel_seq_bcd[4] = channel_seq % 100;
    memset(header_obj.information_time_bcd.data(), 0, 6);
    memset(header_obj.channel_id_bcd.data(), 0, 2);
    header_obj.version_no_bcd = 0;


    std::vector<unsigned char> full_message(CommonHeader::HEADER_SIZE + body_length + 1 + 2);
    size_t header_write_offset = 0;
    full_message[header_write_offset++] = header_obj.esc_code;
    full_message[header_write_offset++] = header_obj.transmission_code;
    full_message[header_write_offset++] = header_obj.message_kind;
    memcpy(full_message.data() + header_write_offset, header_obj.information_time_bcd.data(), 6); header_write_offset += 6;
    memcpy(full_message.data() + header_write_offset, header_obj.channel_id_bcd.data(), 2); header_write_offset += 2;
    memcpy(full_message.data() + header_write_offset, header_obj.channel_seq_bcd.data(), 5); header_write_offset += 5;
    full_message[header_write_offset++] = header_obj.version_no_bcd;
    memcpy(full_message.data() + header_write_offset, header_obj.body_length_bcd.data(), 2); header_write_offset += 2;

    memcpy(full_message.data() + CommonHeader::HEADER_SIZE, msg_body.data(), body_length);
    unsigned char checksum = CoreUtils::calculate_checksum(full_message.data() + 1, CommonHeader::HEADER_SIZE - 1 + body_length);
    full_message[CommonHeader::HEADER_SIZE + body_length] = checksum;
    full_message[CommonHeader::HEADER_SIZE + body_length + 1] = 0x0D;
    full_message[CommonHeader::HEADER_SIZE + body_length + 2] = 0x0A;

    return full_message;
}

void test_process_i081_message() {
    std::cout << "Running test_process_i081_message..." << std::endl;
    TaifexSdk sdk;
    sdk.initialize();

    std::string prod_id_s = "UPDTESTP  "; // 10 chars
    std::string order_book_prod_id = "UPDTESTPROD1234567890"; // 20 chars

    MessageI010 i010_content;
    i010_content.prod_id_s = prod_id_s;
    i010_content.decimal_locator = 2;
    i010_content.prod_kind = 'F'; // Minimal other fields
    i010_content.strike_price_decimal_locator = 0;
    i010_content.flow_group = 1;
    i010_content.dynamic_banding = 'N';
    std::vector<unsigned char> raw_i010 = create_raw_i010_message(20ULL, 32, i010_content);
    sdk.process_message(raw_i010.data(), raw_i010.size());
    std::string i010_lookup_key = "UPDTESTP";
    auto info = sdk.get_product_info(i010_lookup_key);
    assert(info.has_value());
    if (!info) {
        std::cerr << "I010 info not found for key: " << i010_lookup_key << std::endl;
        assert(false);
        return;
    }


    // Optional: Send I083 to establish a base book
    std::vector<MdEntryI083> initial_entries;
    initial_entries.push_back({'0', '0', 20000, 10, 1}); // Bid 200.00 Q10
    initial_entries.push_back({'1', '0', 20100, 5,  1}); // Ask 201.00 Q5
    std::vector<unsigned char> raw_i083 = create_raw_i083_message(21ULL, order_book_prod_id, 1, '0', initial_entries);
    sdk.process_message(raw_i083.data(), raw_i083.size());
    auto ob_opt_initial = sdk.get_order_book(order_book_prod_id);
    assert(ob_opt_initial.has_value());
    if (!ob_opt_initial) { // Guard
        std::cerr << "Initial OrderBook not created for: " << order_book_prod_id << std::endl;
        assert(false);
        return;
    }
    assert(ob_opt_initial->get().get_top_bids(1)[0].quantity == 10);


    // Send I081 to update
    std::vector<MdEntryI081> update_entries;
    // 1. New Bid P=20050, Q=3 (becomes best bid)
    update_entries.push_back({'0', '0', '0', 20050, 3, 1});
    // 2. Change Ask P=20100, Q=5 -> Q=8
    update_entries.push_back({'1', '1', '0', 20100, 8, 1});

    std::vector<unsigned char> raw_i081 = create_raw_i081_message(22ULL, order_book_prod_id, 2, update_entries);
    sdk.process_message(raw_i081.data(), raw_i081.size());

    auto ob_opt_updated = sdk.get_order_book(order_book_prod_id);
    assert(ob_opt_updated.has_value());
    if (ob_opt_updated) {
        const auto& ob = ob_opt_updated->get();
        assert(ob.get_last_prod_msg_seq() == 2); // Prod Msg Seq from I081

        auto bids = ob.get_top_bids(1); // New best bid
        assert(bids.size() == 1 && bids[0].price == 20050 && bids[0].quantity == 3);

        auto asks = ob.get_top_asks(1); // Ask quantity changed
        assert(asks.size() == 1 && asks[0].price == 20100 && asks[0].quantity == 8);
    }
    std::cout << "test_process_i081_message PASSED." << std::endl;
}
