// main_common_header_parser_test.cpp
#include "common_header.h"
#include "logger.h" // For logging test progress
#include "error_codes.h" // For CoreUtils::ParsingError
#include <vector>
#include <cassert>
#include <iomanip> // For std::hex printing

// Helper to create a BCD byte from two digits
unsigned char make_bcd_byte(int d1, int d2) {
    return static_cast<unsigned char>(((d1 & 0x0F) << 4) | (d2 & 0x0F));
}

int main() {
    CoreUtils::setLogLevel(CoreUtils::LogLevel::DEBUG);
    LOG_INFO << "Testing CommonHeader Parsing and BCD Decoders...";

    CoreUtils::CommonHeader header;

    // Example raw header data based on typical values
    // ESC(1), TC(1), MK(1), IT(6), CI(2), CS(5), VN(1), BL(2) = 18 bytes
    std::vector<unsigned char> raw_data = {
        0x1B, // ESC
        0x31, // TC '1' (ASCII for '1')
        0x41, // MK 'A' (ASCII for 'A')
        // Information Time (BCD): 09:30:55.123456 -> "093055123456"
        make_bcd_byte(0,9), make_bcd_byte(3,0), make_bcd_byte(5,5),
        make_bcd_byte(1,2), make_bcd_byte(3,4), make_bcd_byte(5,6),
        // Channel ID (BCD): 12 -> "0012" (packBcdToAscii with num_digits=4)
        make_bcd_byte(0,0), make_bcd_byte(1,2),
        // Channel Seq (BCD): 1234567890 -> "1234567890"
        make_bcd_byte(1,2), make_bcd_byte(3,4), make_bcd_byte(5,6),
        make_bcd_byte(7,8), make_bcd_byte(9,0),
        // Version No (BCD): 01 -> "01"
        make_bcd_byte(0,1),
        // Body Length (BCD): 256 -> "0256"
        make_bcd_byte(0,2), make_bcd_byte(5,6)
    };
    assert(raw_data.size() == CoreUtils::CommonHeader::HEADER_SIZE);

    bool parse_ok = CoreUtils::CommonHeader::parse(raw_data.data(), raw_data.size(), header);
    assert(parse_ok);
    LOG_DEBUG << "CommonHeader::parse successful.";

    // Test direct field values
    assert(header.esc_code == 0x1B);
    assert(header.transmission_code == 0x31); // ASCII '1'
    assert(header.message_kind == 0x41);     // ASCII 'A'
    LOG_DEBUG << "Direct fields (ESC, TC, MK) parsed correctly.";

    // Test BCD decoding helpers
    try {
        std::string time_str = header.getInformationTimeString();
        assert(time_str == "093055123456");
        LOG_DEBUG << "InformationTime: " << time_str << " - PASS";

        uint32_t channel_id = header.getChannelId();
        // packBcdToAscii({0x00, 0x12}, 4) -> "0012" -> 12
        assert(channel_id == 12);
        LOG_DEBUG << "ChannelId: " << channel_id << " - PASS";

        uint64_t channel_seq = header.getChannelSeq();
        assert(channel_seq == 1234567890ULL);
        LOG_DEBUG << "ChannelSeq: " << channel_seq << " - PASS";

        uint8_t version_no = header.getVersionNo();
        // packBcdToAscii({0x01}, 2) -> "01" -> 1
        assert(version_no == 1);
        LOG_DEBUG << "VersionNo: " << (int)version_no << " - PASS";

        uint16_t body_length = header.getBodyLength();
        // packBcdToAscii({0x02, 0x56}, 4) -> "0256" -> 256
        assert(body_length == 256);
        LOG_DEBUG << "BodyLength: " << body_length << " - PASS";

    } catch (const CoreUtils::ParsingError& e) {
        LOG_ERROR << "ParsingError during BCD decoding: " << e.what();
        assert(false); // Should not throw in this test case
    } catch (const std::exception& e) {
        LOG_ERROR << "Std exception during BCD decoding: " << e.what();
        assert(false);
    }

    // Test parse with insufficient buffer
    CoreUtils::CommonHeader temp_header; // Use a temporary for modification tests
    bool parse_fail_short = CoreUtils::CommonHeader::parse(raw_data.data(), 5, temp_header);
    assert(!parse_fail_short);
    LOG_DEBUG << "CommonHeader::parse with insufficient buffer returned false - PASS";

    // Test parse with null buffer
    bool parse_fail_null = CoreUtils::CommonHeader::parse(nullptr, raw_data.size(), temp_header);
    assert(!parse_fail_null);
    LOG_DEBUG << "CommonHeader::parse with null buffer returned false - PASS";

    // Test BCD decoding with invalid BCD data (manually corrupt a field)
    LOG_DEBUG << "Testing invalid BCD data handling...";
    CoreUtils::CommonHeader bad_header_data;
    CoreUtils::CommonHeader::parse(raw_data.data(), raw_data.size(), bad_header_data); // Start with valid data

    // Corrupt Channel ID with an invalid BCD nibble (A is > 9)
    bad_header_data.channel_id_bcd[0] = make_bcd_byte(0, 0x0A); // 0x0A
    bool caught_parsing_error = false;
    try {
        bad_header_data.getChannelId();
    } catch (const CoreUtils::ParsingError& e) {
        LOG_DEBUG << "Caught expected ParsingError for bad BCD in ChannelId: " << e.what() << " - PASS";
        caught_parsing_error = true;
    } catch (const std::exception& e) {
        LOG_ERROR << "Unexpected std::exception for bad BCD in ChannelId: " << e.what();
        assert(false); // Should be CoreUtils::ParsingError
    }
    assert(caught_parsing_error);

    // Test out of range for stoul (e.g. if packBcdToAscii produced too many digits for type)
    // This is harder to test directly without packBcdToAscii itself misbehaving,
    // as packBcdToAscii(..., num_digits) should produce exactly num_digits.
    // The internal checks like `val > UINT16_MAX` are safeguards.
    // Example: if version_no_bcd was 0x99 and somehow interpreted as "999"
    CoreUtils::CommonHeader range_header_data;
    CoreUtils::CommonHeader::parse(raw_data.data(), raw_data.size(), range_header_data);
    // Manually set version_no_bcd to something that would be valid BCD "99"
    // but if the string conversion inside getVersionNo was faulty and produced e.g. "256"
    // This test as is won't hit stoul out_of_range unless stoul itself is limited.
    // The current stoul converts to unsigned long, then we check against type max.
    // Let's test the internal limit check for version_no.
    // If packBcdToAscii({0x26}, 2) -> "26" -> stoul("26") is 26. OK.
    // If packBcdToAscii({make_bcd_byte(2,6)}, 2) was somehow {make_bcd_byte(9,9)} and num_digits=3 -> "999"
    // Forcing an error: Let's assume getVersionNo received a string "256" (which is > UINT8_MAX)
    // This requires deeper mock or friend class to test that specific internal path.
    // The current test for bad BCD (0x0A) is sufficient for now to show ParsingError is thrown.

    LOG_INFO << "CommonHeader Parsing and BCD Decoder tests finished.";
    return 0;
}
