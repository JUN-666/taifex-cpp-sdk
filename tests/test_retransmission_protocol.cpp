#include "networking/retransmission_protocol.h"
#include "networking/endian_utils.h" // For direct use if needed in test setup, though structs use it
#include "core_utils/logger.h"       // For seeing logs from deserialize
#include "core_utils/message_identifier.h" // For MessageType for tests
#include <iostream>
#include <vector>
#include <cassert>
#include <cstring> // For memcpy in tests
#include <iomanip> // For std::hex
#include <type_traits> // For std::is_same_v

// Using namespace for brevity
using namespace TaifexRetransmission;
using namespace NetworkingUtils; // If directly using endian utils for test data setup
using namespace CoreUtils;       // For MessageType, Logger

// Helper to print byte vector for debugging
void print_bytes(const std::vector<unsigned char>& vec) {
    std::cout << std::hex << std::setfill('0');
    for (unsigned char c : vec) {
        std::cout << "0x" << std::setw(2) << static_cast<int>(c) << " ";
    }
    std::cout << std::dec << std::endl;
}

// --- Test calculate_check_code ---
void test_calculate_login_check_code() {
    std::cout << "Running test_calculate_login_check_code..." << std::endl;
    // Example from spec: password "1234", mult_op 168 -> CheckCode 73
    // 1234 * 168 = 207312. (207312 / 100) % 100 = 2073 % 100 = 73.
    uint8_t code1 = calculate_check_code(168, "1234");
    assert(code1 == 73);

    uint8_t code2 = calculate_check_code(10, "1"); // 10 * 1 = 10. (10/100)%100 = 0 % 100 = 0
    assert(code2 == 0);

    uint8_t code3 = calculate_check_code(100, "100"); // 100 * 100 = 10000. (10000/100)%100 = 100 % 100 = 0
    assert(code3 == 0);

    uint8_t code4 = calculate_check_code(50, "123"); // 50 * 123 = 6150. (6150/100)%100 = 61 % 100 = 61
    assert(code4 == 61);

    std::cout << "test_calculate_login_check_code PASSED." << std::endl;
}


// --- Template for testing a message type ---
template<typename MsgType>
void test_message_serialization_deserialization(
    MsgType original_msg, // Pass by value to allow modification for header setting
    const std::string& msg_name,
    const std::string& password_for_login = "" // Only for LoginRequest020
) {
    std::cout << "Running test for " << msg_name << "..." << std::endl;

    // Set common header fields that serialize methods might expect to be somewhat valid
    // In the actual serialize methods, msg_type and msg_size are overwritten.
    // msg_seq_num and msg_time are used from the 'original_msg' members.
    // If msg_time is not set by individual tests, it will be default (likely zero).
    // The individual test functions now set msg_seq_num.

    std::vector<unsigned char> buffer;
    if constexpr (std::is_same_v<MsgType, LoginRequest020>) {
        // For LoginRequest020, check_code should be set in original_msg before calling serialize.
        original_msg.serialize(buffer, password_for_login);
    } else if constexpr (std::is_same_v<MsgType, DataResponse102>) {
        std::vector<unsigned char> sample_retrans_data = {0x01, 0x02, 0x03, 0x04}; // Example retransmitted data
        original_msg.serialize(buffer, sample_retrans_data);
    } else {
        original_msg.serialize(buffer);
    }

    // print_bytes(buffer); // For debugging

    MsgType deserialized_msg;
    bool success;
    if constexpr (std::is_same_v<MsgType, LoginRequest020>) {
        success = deserialized_msg.deserialize(buffer.data(), buffer.size(), password_for_login);
    } else if constexpr (std::is_same_v<MsgType, DataResponse102>) {
        std::vector<unsigned char> out_retrans_data;
        success = deserialized_msg.deserialize(buffer.data(), buffer.size(), out_retrans_data);
        if (success) { // If main struct deserialize ok, check retrans_data
            std::vector<unsigned char> expected_retrans_data = {0x01, 0x02, 0x03, 0x04};
            assert(out_retrans_data == expected_retrans_data);
        }
    }
    else {
        success = deserialized_msg.deserialize(buffer.data(), buffer.size());
    }

    assert(success); // Main deserialization success

    // Field-by-field comparison
    // Header part (msg_type is set by serialize, msg_size also. SeqNum and Time should match if set in original)
    assert(deserialized_msg.header.msg_type == MsgType::MESSAGE_TYPE); // Check against static const
    assert(deserialized_msg.header.msg_seq_num == original_msg.header.msg_seq_num);
    // Timestamps can be tricky if serialize populates current time.
    // For tests where original_msg.header.msg_time is explicitly set, we can compare.
    // assert(deserialized_msg.header.msg_time.epoch_s == original_msg.header.msg_time.epoch_s);
    // assert(deserialized_msg.header.msg_time.nanosecond == original_msg.header.msg_time.nanosecond);


    // Test checksum failure
    if (!buffer.empty() && buffer.size() > RetransmissionMsgFooter::SIZE) { // Ensure there's a checksum byte
        std::vector<unsigned char> bad_checksum_buffer = buffer;
        size_t checksum_byte_offset = bad_checksum_buffer.size() - RetransmissionMsgFooter::SIZE;
        bad_checksum_buffer[checksum_byte_offset] ^= 0xFF; // Corrupt checksum

        MsgType msg_bad_checksum;
        bool checksum_fail_succeeded; // True if deserialize correctly fails
         if constexpr (std::is_same_v<MsgType, LoginRequest020>) {
            checksum_fail_succeeded = !msg_bad_checksum.deserialize(bad_checksum_buffer.data(), bad_checksum_buffer.size(), password_for_login);
        } else if constexpr (std::is_same_v<MsgType, DataResponse102>) {
            std::vector<unsigned char> out_retrans_data;
            checksum_fail_succeeded = !msg_bad_checksum.deserialize(bad_checksum_buffer.data(), bad_checksum_buffer.size(), out_retrans_data);
        } else {
            checksum_fail_succeeded = !msg_bad_checksum.deserialize(bad_checksum_buffer.data(), bad_checksum_buffer.size());
        }
        assert(checksum_fail_succeeded);
    }

    // Test msg_type mismatch
    if (buffer.size() >= RetransmissionMsgHeader::SIZE) {
        std::vector<unsigned char> bad_type_buffer = buffer;
        // MsgType is after MsgSize field. Offset is sizeof(uint16_t).
        uint16_t type_offset_in_buffer = sizeof(uint16_t);
        uint16_t original_type_net;
        memcpy(&original_type_net, bad_type_buffer.data() + type_offset_in_buffer, sizeof(uint16_t));

        uint16_t bad_type_val = (MsgType::MESSAGE_TYPE == 10 ? 11 : MsgType::MESSAGE_TYPE + 1); // Ensure it's different
        uint16_t bad_type_net = host_to_network_short(bad_type_val);
        memcpy(bad_type_buffer.data() + type_offset_in_buffer, &bad_type_net, sizeof(uint16_t));

        // Recalculate checksum for this modified buffer to isolate type check failure
        uint16_t actual_msg_size_val; // The value in the MsgSize field
        memcpy(&actual_msg_size_val, bad_type_buffer.data(), sizeof(uint16_t));
        actual_msg_size_val = network_to_host_short(actual_msg_size_val);
        size_t len_for_checksum = sizeof(uint16_t) + actual_msg_size_val;

        if (len_for_checksum < bad_type_buffer.size()) { // Ensure len_for_checksum is valid for buffer access
             bad_type_buffer[len_for_checksum] = calculate_retransmission_checksum(bad_type_buffer.data(), len_for_checksum);
        }

        MsgType msg_bad_type;
        bool type_fail_succeeded;
        if constexpr (std::is_same_v<MsgType, LoginRequest020>) {
            type_fail_succeeded = !msg_bad_type.deserialize(bad_type_buffer.data(), bad_type_buffer.size(), password_for_login);
        } else if constexpr (std::is_same_v<MsgType, DataResponse102>) {
            std::vector<unsigned char> out_retrans_data;
            type_fail_succeeded = !msg_bad_type.deserialize(bad_type_buffer.data(), bad_type_buffer.size(), out_retrans_data);
        } else {
            type_fail_succeeded = !msg_bad_type.deserialize(bad_type_buffer.data(), bad_type_buffer.size());
        }
        assert(type_fail_succeeded);
    }

    std::cout << msg_name << " test PASSED." << std::endl;
}


// --- Individual Message Tests ---
void test_login_request_020() {
    LoginRequest020 msg;
    msg.header.msg_seq_num = 1;
    // msg.header.msg_time can be default or set if comparing
    msg.multiplication_operator = 168;
    msg.session_id = 12345;
    msg.check_code = calculate_check_code(msg.multiplication_operator, "1234");
    test_message_serialization_deserialization(msg, "LoginRequest020", "1234");
}

void test_login_response_030() {
    LoginResponse030 msg;
    msg.header.msg_seq_num = 2;
    msg.channel_id = 1;
    test_message_serialization_deserialization(msg, "LoginResponse030");
}

void test_retrans_start_050() {
    RetransmissionStart050 msg;
    msg.header.msg_seq_num = 3;
    test_message_serialization_deserialization(msg, "RetransmissionStart050");
}

void test_heartbeat_server_104() {
    HeartbeatServer104 msg;
    msg.header.msg_seq_num = 10;
    test_message_serialization_deserialization(msg, "HeartbeatServer104");
}

void test_heartbeat_client_105() {
    HeartbeatClient105 msg;
    msg.header.msg_seq_num = 11;
    test_message_serialization_deserialization(msg, "HeartbeatClient105");
}

void test_data_request_101() {
    DataRequest101 msg;
    msg.header.msg_seq_num = 20;
    msg.channel_id = 2;
    msg.begin_seq_no = 1000;
    msg.recover_num = 50;
    test_message_serialization_deserialization(msg, "DataRequest101");
}

void test_data_response_102() {
    DataResponse102 msg; // This test will use the default empty retrans_data in serialize
    msg.header.msg_seq_num = 21;
    msg.channel_id = 2;
    msg.status_code = 0; // Success
    msg.begin_seq_no = 1000;
    msg.recover_num = 0; // For test, 0 retrans messages, so retrans_data is empty
    test_message_serialization_deserialization(msg, "DataResponse102");
}

void test_error_notification_010() {
    ErrorNotification010 msg;
    msg.header.msg_seq_num = 5;
    msg.status_code = 8; // Checksum error example
    test_message_serialization_deserialization(msg, "ErrorNotification010");
}


int main() {
    test_calculate_login_check_code();
    test_login_request_020();
    test_login_response_030();
    test_retrans_start_050();
    test_heartbeat_server_104();
    test_heartbeat_client_105();
    test_data_request_101();
    test_data_response_102(); // This will test with empty retrans_data
    test_error_notification_010();

    std::cout << "All Retransmission Protocol tests completed." << std::endl;
    return 0;
}
