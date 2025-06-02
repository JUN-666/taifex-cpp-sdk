#include "utils/log_file_packet_simulator.h"
#include "core_utils/logger.h" // For logs during test
#include "core_utils/checksum.h" // For calculateXorChecksum
#include "core_utils/message_identifier.h" // For MessageType
#include "core_utils/common_header.h" // For CommonHeader constants
#include <iostream>
#include <fstream> // For writing test files
#include <vector>
#include <cassert>
#include <cstdio> // For std::remove with temporary files
#include <cstring> // For memcpy
#include <iomanip> // For std::hex
#include <span>      // For std::span
#include <cstddef>   // For std::byte

// Using namespaces for brevity
using namespace Utils;
// Added CoreUtils for the create_dummy_market_data helper, if it's not already there.
using namespace CoreUtils;

// Helper to print byte vector for debugging
void print_bytes_log_sim(const std::vector<unsigned char>& vec) { // Renamed to avoid clash if linked with other tests
    std::cout << std::hex << std::setfill('0');
    for (unsigned char c : vec) {
        std::cout << "0x" << std::setw(2) << static_cast<int>(c) << " ";
    }
    std::cout << std::dec << std::endl;
}

// Helper to create a PCAP-like file content in a vector
// file_header_size: e.g., 24
// packet_records: vector of pairs, where each pair is {pcap_packet_header_bytes (16), captured_data_bytes}
std::vector<unsigned char> create_test_log_content(
    size_t file_header_size,
    const std::vector<std::pair<std::vector<unsigned char>, std::vector<unsigned char>>>& packet_records) {

    std::vector<unsigned char> content(file_header_size); // Initialize with global header dummy data (all zeros)

    for (const auto& record : packet_records) {
        // Assuming LogFilePacketSimulator::DEFAULT_PACKET_HEADER_SIZE is accessible or known (it's 16)
        // If it were private, we'd hardcode 16 here or pass it.
        // For this test, we assume the helper create_pcap_packet_header uses the same constant.
        assert(record.first.size() == 16); // Check header part size
        content.insert(content.end(), record.first.begin(), record.first.end());
        content.insert(content.end(), record.second.begin(), record.second.end());
    }
    return content;
}

// Helper to create a 16-byte PCAP packet header
// For simplicity, assumes native endian for length.
std::vector<unsigned char> create_pcap_packet_header(uint32_t captured_length, uint32_t original_length_ignored = 0) {
    // Using LogFilePacketSimulator::DEFAULT_PACKET_HEADER_SIZE would be ideal if public,
    // but it's private. So, we use a known constant 16.
    const size_t pcap_header_sz = 16;
    std::vector<unsigned char> header(pcap_header_sz, 0);
    // ts_sec (offset 0, 4 bytes) - dummy
    // ts_usec (offset 4, 4 bytes) - dummy
    // incl_len (offset 8, 4 bytes) - this is captured_length
    // orig_len (offset 12, 4 bytes) - dummy (can be same as incl_len if not truncated)
    uint32_t len_to_write = captured_length;
    memcpy(header.data() + 8, &len_to_write, sizeof(uint32_t));
    if (original_length_ignored == 0) original_length_ignored = captured_length;
    memcpy(header.data() + 12, &original_length_ignored, sizeof(uint32_t));
    return header;
}

// Helper to write data to a temporary file and return its name
std::string write_temp_log_file(const std::string& base_name, const std::vector<unsigned char>& data) {
    std::string filename = base_name;
    std::ofstream outfile(filename, std::ios::binary | std::ios::trunc);
    if (!outfile) {
        CoreUtils::Logger::Log(CoreUtils::LogLevel::FATAL,"Failed to create temp file: " + filename);
        return "";
    }
    outfile.write(reinterpret_cast<const char*>(data.data()), data.size());
    outfile.close();
    return filename;
}

// This helper function is used by tests/test_network_manager.cpp as well.
// It needs to be present here if that test file is compiled separately and linked,
// or this helper needs to be moved to a common test utility.
// For now, assuming it's okay to have it here if test_log_file_packet_simulator is standalone.
// If it was meant for test_network_manager.cpp, it should be there.
// The prompt context implies this file is for LogFilePacketSimulator tests.
// The previous step added a create_dummy_market_data to test_network_manager.cpp.
// This one is specific to test_log_file_packet_simulator if it were to use it.
// For now, no test here uses create_dummy_market_data, so it can be removed if not needed *in this file*.
// However, the subtask prompt for step 12 *did* say:
// "Update call sites in tests/test_log_file_packet_simulator.cpp (in raw message creation helpers like create_dummy_market_data)"
// This implies such a helper *should* be in this file and *should* be updated.
// Let's assume it was intended to be here and update it.

std::vector<unsigned char> create_dummy_market_data_for_log_sim_tests(uint32_t channel_id_val, uint64_t channel_seq_val, char unique_payload_byte = 'A') {
    CommonHeader header;
    header.esc_code = 0x1B;
    header.transmission_code = 0x02;
    header.message_kind = CoreUtils::MessageIdentifier::messageTypeToByte(MessageType::I081_ORDER_BOOK_UPDATE);

    header.channel_id_bcd[0] = static_cast<unsigned char>((channel_id_val >> 8) & 0xFF);
    header.channel_id_bcd[1] = static_cast<unsigned char>(channel_id_val & 0xFF);

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

    uint16_t body_length = 1;
    header.body_length_bcd[0] = static_cast<unsigned char>((body_length >> 8) & 0xFF);
    header.body_length_bcd[1] = static_cast<unsigned char>(body_length & 0xFF);

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

    message[CommonHeader::HEADER_SIZE] = unique_payload_byte;

    size_t checksum_data_length = (CommonHeader::HEADER_SIZE - 1) + body_length;
    std::span<const unsigned char> checksum_data_uchars(message.data() + 1, checksum_data_length);
    unsigned char checksum = CoreUtils::calculateXorChecksum(std::as_bytes(checksum_data_uchars));
    message[CommonHeader::HEADER_SIZE + body_length] = checksum;
    message[CommonHeader::HEADER_SIZE + body_length + 1] = 0x0D;
    message[CommonHeader::HEADER_SIZE + body_length + 2] = 0x0A;
    return message;
}


void test_read_single_valid_packet() {
    std::cout << "Running test_read_single_valid_packet..." << std::endl;

    std::vector<unsigned char> taifex_msg1 = {0x1B, 0x02, 0xA1, 0x00, 0x05, 0x0D, 0x0A};
    std::vector<unsigned char> pcap_pkt_hdr1 = create_pcap_packet_header(static_cast<uint32_t>(taifex_msg1.size()));

    std::vector<std::pair<std::vector<unsigned char>, std::vector<unsigned char>>> records;
    records.push_back({pcap_pkt_hdr1, taifex_msg1});

    std::vector<unsigned char> file_content = create_test_log_content(24, records);
    std::string tmp_file = write_temp_log_file("single_valid.pcaplike", file_content);
    assert(!tmp_file.empty());

    LogFilePacketSimulator simulator(tmp_file);
    assert(simulator.open());
    assert(simulator.is_open());
    assert(simulator.has_next_packet());

    std::vector<unsigned char> packet = simulator.get_next_taifex_packet();
    assert(!packet.empty());
    assert(packet == taifex_msg1);

    assert(!simulator.has_next_packet());
    simulator.close();
    std::remove(tmp_file.c_str());
    std::cout << "test_read_single_valid_packet PASSED." << std::endl;
}

void test_read_multiple_packets() {
    std::cout << "Running test_read_multiple_packets..." << std::endl;
    std::vector<unsigned char> taifex_msg1 = {0x1B, 'A', 'B', 'C'};
    std::vector<unsigned char> taifex_msg2 = {0x1B, 'X', 'Y', 'Z', '0'};
    std::vector<unsigned char> pcap_pkt_hdr1 = create_pcap_packet_header(static_cast<uint32_t>(taifex_msg1.size()));
    std::vector<unsigned char> pcap_pkt_hdr2 = create_pcap_packet_header(static_cast<uint32_t>(taifex_msg2.size()));

    std::vector<std::pair<std::vector<unsigned char>, std::vector<unsigned char>>> records;
    records.push_back({pcap_pkt_hdr1, taifex_msg1});
    records.push_back({pcap_pkt_hdr2, taifex_msg2});

    std::vector<unsigned char> file_content = create_test_log_content(24, records);
    std::string tmp_file = write_temp_log_file("multi_valid.pcaplike", file_content);
    assert(!tmp_file.empty());

    LogFilePacketSimulator simulator(tmp_file);
    assert(simulator.open());

    assert(simulator.has_next_packet());
    std::vector<unsigned char> p1 = simulator.get_next_taifex_packet();
    assert(p1 == taifex_msg1);

    assert(simulator.has_next_packet());
    std::vector<unsigned char> p2 = simulator.get_next_taifex_packet();
    assert(p2 == taifex_msg2);

    assert(!simulator.has_next_packet());
    simulator.close();
    std::remove(tmp_file.c_str());
    std::cout << "test_read_multiple_packets PASSED." << std::endl;
}

void test_packet_without_esc_code() {
    std::cout << "Running test_packet_without_esc_code..." << std::endl;
    std::vector<unsigned char> non_taifex_data = {'N', 'O', 'T', 'A', 'P', 'K', 'T'};
    std::vector<unsigned char> pcap_pkt_hdr = create_pcap_packet_header(static_cast<uint32_t>(non_taifex_data.size()));

    std::vector<std::pair<std::vector<unsigned char>, std::vector<unsigned char>>> records;
    records.push_back({pcap_pkt_hdr, non_taifex_data});

    std::vector<unsigned char> file_content = create_test_log_content(24, records);
    std::string tmp_file = write_temp_log_file("no_esc.pcaplike", file_content);
    assert(!tmp_file.empty());

    LogFilePacketSimulator simulator(tmp_file);
    assert(simulator.open());
    assert(simulator.has_next_packet());
    std::vector<unsigned char> packet = simulator.get_next_taifex_packet();
    assert(packet.empty());

    assert(!simulator.has_next_packet());
    simulator.close();
    std::remove(tmp_file.c_str());
    std::cout << "test_packet_without_esc_code PASSED." << std::endl;
}

void test_eof_in_pcap_header() {
    std::cout << "Running test_eof_in_pcap_header..." << std::endl;
    std::vector<unsigned char> partial_pcap_hdr = {0,0,0,0, 0,0,0,0}; // Only 8 bytes

    std::vector<unsigned char> file_content(24); // Global header
    file_content.insert(file_content.end(), partial_pcap_hdr.begin(), partial_pcap_hdr.end());

    std::string tmp_file = write_temp_log_file("eof_header.pcaplike", file_content);
    assert(!tmp_file.empty());

    LogFilePacketSimulator simulator(tmp_file);
    assert(simulator.open());
    std::vector<unsigned char> packet = simulator.get_next_taifex_packet();
    assert(packet.empty());

    simulator.close();
    std::remove(tmp_file.c_str());
    std::cout << "test_eof_in_pcap_header PASSED." << std::endl;
}

void test_eof_in_pcap_data() {
    std::cout << "Running test_eof_in_pcap_data..." << std::endl;
    uint32_t declared_len = 100;
    std::vector<unsigned char> pcap_pkt_hdr = create_pcap_packet_header(declared_len);
    std::vector<unsigned char> partial_data = {0x1B, 'A', 'B'}; // Only 3 bytes

    std::vector<unsigned char> file_content(24);
    file_content.insert(file_content.end(), pcap_pkt_hdr.begin(), pcap_pkt_hdr.end());
    file_content.insert(file_content.end(), partial_data.begin(), partial_data.end());

    std::string tmp_file = write_temp_log_file("eof_data.pcaplike", file_content);
    assert(!tmp_file.empty());

    LogFilePacketSimulator simulator(tmp_file);
    assert(simulator.open());
    std::vector<unsigned char> packet = simulator.get_next_taifex_packet();
    assert(packet.empty());
    simulator.close();
    std::remove(tmp_file.c_str());
    std::cout << "test_eof_in_pcap_data PASSED." << std::endl;
}

void test_zero_length_capture() {
    std::cout << "Running test_zero_length_capture..." << std::endl;
    std::vector<unsigned char> pcap_pkt_hdr = create_pcap_packet_header(0);

    std::vector<std::pair<std::vector<unsigned char>, std::vector<unsigned char>>> records;
    records.push_back({pcap_pkt_hdr, {}});

    std::vector<unsigned char> file_content = create_test_log_content(24, records);
    std::string tmp_file = write_temp_log_file("zero_len.pcaplike", file_content);
    assert(!tmp_file.empty());

    LogFilePacketSimulator simulator(tmp_file);
    assert(simulator.open());
    assert(simulator.has_next_packet());
    std::vector<unsigned char> packet = simulator.get_next_taifex_packet();
    assert(packet.empty());

    assert(!simulator.has_next_packet());
    simulator.close();
    std::remove(tmp_file.c_str());
    std::cout << "test_zero_length_capture PASSED." << std::endl;
}


int main() {
    CoreUtils::Logger::SetLevel(CoreUtils::LogLevel::DEBUG);

    test_read_single_valid_packet();
    test_read_multiple_packets();
    test_packet_without_esc_code();
    test_eof_in_pcap_header();
    test_eof_in_pcap_data();
    test_zero_length_capture();

    std::cout << "All LogFilePacketSimulator tests completed." << std::endl;
    return 0;
}
