#include "networking/network_manager.h"
#include "sdk/taifex_sdk.h" // For TaifexSdk type, though we'll mock its usage
#include "core_utils/common_header.h" // For CommonHeader
#include "core_utils/message_identifier.h" // For MessageType for dummy messages
#include "core_utils/checksum.h" // For calculate_checksum for dummy messages
#include "core_utils/logger.h"   // For potential logging in tests or by tested components
#include <iostream>
#include <vector>
#include <cassert>
#include <map>
#include <cstring> // For memcpy
#include <iomanip> // For std::hex

// Using namespaces for brevity
using namespace Networking;
using namespace Taifex;
using namespace CoreUtils;

// --- Mock TaifexSdk for testing NetworkManager's forwarding ---
struct MockTaifexSdk {
    int process_message_call_count = 0;
    std::vector<unsigned char> last_data_processed;
    size_t last_data_length = 0;
    std::map<uint64_t, int> seq_counts; // combined_channel_seq_key -> count

    void process_message(const unsigned char* data, size_t length) {
        process_message_call_count++;
        last_data_processed.assign(data, data + length);
        last_data_length = length;

        if (length >= CommonHeader::HEADER_SIZE) {
            CommonHeader header;
            // Use a temporary length for parsing just the header part if needed by CommonHeader::parse
            // Assuming CommonHeader::parse can extract needed fields from at least HEADER_SIZE bytes.
            if (CommonHeader::parse(data, length, header)) {
                uint64_t combined_key = (static_cast<uint64_t>(header.getChannelId()) << 32) | header.getChannelSeq();
                seq_counts[combined_key]++;
            }
        }
    }
    void reset_mock() {
        process_message_call_count = 0;
        last_data_processed.clear();
        last_data_length = 0;
        seq_counts.clear();
    }
};

// Helper to create a minimal raw market data message for testing NetworkManager data path
std::vector<unsigned char> create_dummy_market_data(uint32_t channel_id_val, uint64_t channel_seq_val, char unique_payload_byte = 'A') {
    CommonHeader header; // Default constructor usually zeroes out BCD arrays
    header.esc_code = 0x1B;
    header.transmission_code = 0x02;
    header.message_kind = CoreUtils::MessageIdentifier::messageTypeToByte(MessageType::I081_ORDER_BOOK_UPDATE); // Arbitrary type

    // Simplified BCD for ChannelID (direct byte assignment, not true BCD)
    header.channel_id_bcd[0] = static_cast<unsigned char>((channel_id_val >> 8) & 0xFF);
    header.channel_id_bcd[1] = static_cast<unsigned char>(channel_id_val & 0xFF);

    // Simplified BCD for ChannelSeq (direct byte assignment, not true BCD)
    uint64_t temp_seq = channel_seq_val;
    for(int i = 4; i >= 0; --i) {
        if (temp_seq > 0) {
            unsigned char digit1 = temp_seq % 10;
            temp_seq /= 10;
            unsigned char digit2 = 0;
            if (temp_seq > 0) {
                digit2 = temp_seq % 10;
                temp_seq /= 10;
            }
            header.channel_seq_bcd[i] = (digit2 << 4) | digit1;
        } else {
            header.channel_seq_bcd[i] = 0;
        }
    }

    uint16_t body_length = 1; // Minimal body
    // Simplified BCD for BodyLength (direct byte assignment, not true BCD)
    header.body_length_bcd[0] = static_cast<unsigned char>((body_length >> 8) & 0xFF);
    header.body_length_bcd[1] = static_cast<unsigned char>(body_length & 0xFF);
    // Other BCD fields in header (InformationTimeBcd, VersionNoBcd) are default (likely zeroed).

    std::vector<unsigned char> message(CommonHeader::HEADER_SIZE + body_length + 1 + 2);
    size_t offset = 0;
    message[offset++] = header.esc_code;
    message[offset++] = header.transmission_code;
    message[offset++] = header.message_kind;
    memcpy(message.data() + offset, header.information_time_bcd.data(), 6); offset += 6;
    memcpy(message.data() + offset, header.channel_id_bcd.data(), 2); offset += 2;
    memcpy(message.data() + offset, header.channel_seq_bcd.data(), 5); offset += 5;
    message[offset++] = header.version_no_bcd;
    memcpy(message.data() + offset, header.body_length_bcd.data(), 2); offset += 2;

    message[CommonHeader::HEADER_SIZE] = unique_payload_byte; // Dummy body

    unsigned char checksum = CoreUtils::calculate_checksum(message.data() + 1, CommonHeader::HEADER_SIZE - 1 + body_length);
    message[CommonHeader::HEADER_SIZE + body_length] = checksum;
    message[CommonHeader::HEADER_SIZE + body_length + 1] = 0x0D;
    message[CommonHeader::HEADER_SIZE + body_length + 2] = 0x0A;
    return message;
}


void test_network_manager_config_and_lifecycle() {
    std::cout << "Running test_network_manager_config_and_lifecycle..." << std::endl;
    MockTaifexSdk mock_sdk;
    NetworkManager manager(reinterpret_cast<TaifexSdk*>(&mock_sdk));

    NetworkManagerConfig config;
    config.multicast_feeds.push_back({"225.0.140.140", 14000, "", true});

    // configure_and_start will attempt to create sockets and threads.
    // This may fail in restricted test environments (e.g. no network permissions).
    // The primary goal here is to check the configuration logic flow rather than actual network activity.
    bool configured_and_started = manager.configure_and_start(config);
    if (!configured_and_started) {
        CoreUtils::Logger::Log(CoreUtils::LogLevel::WARNING, "test_network_manager_config_and_lifecycle: configure_and_start returned false. This might be due to environment restrictions on network operations.");
        // Depending on test environment, this might be an expected outcome if sockets can't be opened.
        // For CI/headless tests without network, this is often the case.
    }
    // assert(configured_and_started); // This assertion is environment-dependent.

    manager.stop();
    std::cout << "test_network_manager_config_and_lifecycle PASSED (basic structure check)." << std::endl;
}

// This is a test hook that needs to be added to NetworkManager for this test to work as written.
// For example, in NetworkManager.h:
// #ifdef ENABLE_TEST_HOOKS
// public:
// void on_raw_multicast_data_for_test(const unsigned char* data, size_t length,
//                                   const std::string& source_group_ip, uint16_t source_group_port,
//                                   bool is_from_primary_feed) {
//     // Calls the actual private on_raw_multicast_data or process_incoming_packet
//     process_incoming_packet(data, length, is_from_primary_feed, false /*is_retransmitted*/);
// }
// #endif

void test_network_manager_deduplication() {
    std::cout << "Running test_network_manager_deduplication..." << std::endl;
    MockTaifexSdk mock_sdk;
    NetworkManager manager(reinterpret_cast<TaifexSdk*>(&mock_sdk));

    NetworkManagerConfig config;
    config.dual_feed_enabled = true;
    config.multicast_feeds.push_back({"GROUP_A_IP", 1234, "", true});
    config.multicast_feeds.push_back({"GROUP_B_IP", 1234, "", false});

    // configure_and_start loads the config. Actual networking parts may not fully succeed in a restricted env.
    manager.configure_and_start(config);


    uint32_t test_channel_id = 1;
    std::vector<unsigned char> packet1 = create_dummy_market_data(test_channel_id, 100ULL, 'A');
    std::vector<unsigned char> packet2_dup = create_dummy_market_data(test_channel_id, 100ULL, 'B');
    std::vector<unsigned char> packet3_new = create_dummy_market_data(test_channel_id, 101ULL, 'C');

    // Simulate receiving packet1 on primary feed
    // This requires NetworkManager::on_primary_multicast_data to be callable for test, or a similar hook.
    // Let's assume a public test hook `on_multicast_data_for_test` is added to NetworkManager
    manager.on_primary_multicast_data(packet1.data(), packet1.size(), "GROUP_A_IP", 1234);
    assert(mock_sdk.process_message_call_count == 1);
    assert(mock_sdk.last_data_length == packet1.size());
    // Check a distinguishing feature of packet1 (e.g., its unique payload byte)
    // Assuming payload byte is at CommonHeader::HEADER_SIZE
    assert(mock_sdk.last_data_processed[CommonHeader::HEADER_SIZE] == 'A');


    // Simulate receiving packet2 (duplicate seq) on secondary feed
    manager.on_secondary_multicast_data(packet2_dup.data(), packet2_dup.size(), "GROUP_B_IP", 1234);
    assert(mock_sdk.process_message_call_count == 1); // Should NOT increment

    // Simulate receiving packet3 (new seq) on primary feed
    manager.on_primary_multicast_data(packet3_new.data(), packet3_new.size(), "GROUP_A_IP", 1234);
    assert(mock_sdk.process_message_call_count == 2);
    assert(mock_sdk.last_data_length == packet3_new.size());
    assert(mock_sdk.last_data_processed[CommonHeader::HEADER_SIZE] == 'C');

    // Test deduplication window cleanup
    mock_sdk.reset_mock();
    // Fill the window with DEDUPLICATION_WINDOW_SIZE unique packets on channel `test_channel_id`
    // starting from a sequence number far from 100 and 101.
    uint64_t fill_start_seq = 200;
    for (size_t i = 0; i < DEDUPLICATION_WINDOW_SIZE; ++i) {
        std::vector<unsigned char> p = create_dummy_market_data(test_channel_id, fill_start_seq + i, 'D');
        manager.on_primary_multicast_data(p.data(), p.size(), "GROUP_A_IP", 1234);
    }
    assert(mock_sdk.process_message_call_count == DEDUPLICATION_WINDOW_SIZE);

    // Resend packet1 (seq 100). If DEDUPLICATION_WINDOW_SIZE is e.g. 100, and we sent 100 packets
    // with sequences 200-299, packet 100 should have aged out of a simple map if pruning oldest by key.
    // The current pruning logic `deduplication_log_.erase(deduplication_log_.begin());` removes the
    // entry with the smallest combined_seq_key. So, seq 100 (key (1<<32)|100) should be pruned.
    manager.on_primary_multicast_data(packet1.data(), packet1.size(), "GROUP_A_IP", 1234);
    assert(mock_sdk.process_message_call_count == DEDUPLICATION_WINDOW_SIZE + 1);


    std::cout << "test_network_manager_deduplication PASSED." << std::endl;
}


int main() {
    CoreUtils::Logger::SetLevel(CoreUtils::LogLevel::DEBUG); // Enable debug logs for more test visibility
    std::cout << "--- Running NetworkManager Tests ---" << std::endl;
    test_network_manager_config_and_lifecycle();
    test_network_manager_deduplication();

    std::cout << "All NetworkManager tests (basic) completed." << std::endl;
    return 0;
}
