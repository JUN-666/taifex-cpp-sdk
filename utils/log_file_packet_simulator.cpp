#include "utils/log_file_packet_simulator.h"
#include "core_utils/logger.h" // For logging

#include <vector>
#include <algorithm> // For std::find
#include <cstring>   // For memcpy

namespace Utils {

// Define static const members (values are in the header as defaults or const initializers)
// No need to redefine values here, just the declaration for linkage if they were not constexpr inline in header.
// However, since they are const size_t/char in header and likely initialized there (or constexpr),
// this definition block might not be strictly necessary for modern compilers if they are header-only consts.
// If they were non-const static or needed out-of-line definition, this would be the place.
// For this setup (const static members initialized in header), this is okay or can be omitted.
// const size_t LogFilePacketSimulator::DEFAULT_GLOBAL_HEADER_SIZE;
// const size_t LogFilePacketSimulator::DEFAULT_PACKET_HEADER_SIZE;
// const unsigned char LogFilePacketSimulator::TAIFEX_ESC_CODE;


LogFilePacketSimulator::LogFilePacketSimulator(const std::string& filepath,
                                               size_t global_header_size,
                                               size_t packet_header_size)
    : log_filepath_(filepath),
      is_file_open_(false),
      global_header_to_skip_(global_header_size),
      pcap_packet_header_size_(packet_header_size) {
    CoreUtils::Logger::Log(CoreUtils::LogLevel::DEBUG, "LogFilePacketSimulator created for file: " + filepath);
}

LogFilePacketSimulator::~LogFilePacketSimulator() {
    close();
    CoreUtils::Logger::Log(CoreUtils::LogLevel::DEBUG, "LogFilePacketSimulator for file: " + log_filepath_ + " destroyed.");
}

bool LogFilePacketSimulator::open() {
    if (is_file_open_) {
        CoreUtils::Logger::Log(CoreUtils::LogLevel::WARNING, "Log file " + log_filepath_ + " is already open.");
        return true;
    }

    file_stream_.open(log_filepath_, std::ios::binary);
    if (!file_stream_.is_open()) {
        CoreUtils::Logger::Log(CoreUtils::LogLevel::ERROR, "Failed to open log file: " + log_filepath_);
        is_file_open_ = false;
        return false;
    }

    if (global_header_to_skip_ > 0) {
        file_stream_.seekg(global_header_to_skip_, std::ios::beg);
        if (!file_stream_.good()) { // Check if seek failed (e.g. file smaller than header)
            CoreUtils::Logger::Log(CoreUtils::LogLevel::ERROR, "Failed to seek past global header (" + std::to_string(global_header_to_skip_) + " bytes) in: " + log_filepath_ + ". File might be too short.");
            file_stream_.close();
            is_file_open_ = false;
            return false;
        }
    }

    is_file_open_ = true;
    CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "Log file opened successfully: " + log_filepath_);
    return true;
}

void LogFilePacketSimulator::close() {
    if (is_file_open_) {
        file_stream_.close();
        is_file_open_ = false;
        CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "Log file closed: " + log_filepath_);
    }
}

bool LogFilePacketSimulator::is_open() const {
    return is_file_open_ && file_stream_.is_open();
}

bool LogFilePacketSimulator::has_next_packet() {
    if (!is_open() || !file_stream_.good()) {
        return false;
    }
    return file_stream_.peek() != EOF;
}

// Private helper implementation
bool LogFilePacketSimulator::read_pcap_packet_captured_length(std::vector<unsigned char>& header_buffer, uint32_t& out_length) {
    // Renamed parameter to header_buffer to avoid confusion with member pcap_packet_header_size_
    // This function now expects header_buffer to be pre-filled by the caller.
    // The original prompt had this function taking (uint32_t& out_length) and reading from file_stream_ itself.
    // Let's stick to the version that reads from file_stream_ as per the prompt's .cpp structure.

    // The version in the prompt's .cpp: bool LogFilePacketSimulator::read_pcap_packet_captured_length(uint32_t& out_length)
    // This means it reads from file_stream_ directly.
    std::vector<char> local_header_buffer(pcap_packet_header_size_); // Use char for ifstream::read
    if (!file_stream_.read(local_header_buffer.data(), pcap_packet_header_size_)) {
        if (file_stream_.eof()) { // Check if EOF was reached during or before read
             if (file_stream_.gcount() == 0) { // EOF before reading anything for this header
                CoreUtils::Logger::Log(CoreUtils::LogLevel::DEBUG, "EOF before reading PCAP packet header from: " + log_filepath_);
             } else { // EOF after reading some bytes but not full header
                CoreUtils::Logger::Log(CoreUtils::LogLevel::DEBUG, "Incomplete PCAP packet header (read " + std::to_string(file_stream_.gcount()) + "/" + std::to_string(pcap_packet_header_size_) + " bytes) at EOF in: " + log_filepath_);
             }
        } else { // Actual read error
            CoreUtils::Logger::Log(CoreUtils::LogLevel::ERROR, "Failed to read PCAP packet header from: " + log_filepath_ + " due to stream error.");
        }
        return false;
    }

    // Standard PCAP packet header: ts_sec (4), ts_usec (4), incl_len (4), orig_len (4) = 16 bytes
    // incl_len (Captured Packet Length) is at offset 8.
    if (pcap_packet_header_size_ < 12) {
        CoreUtils::Logger::Log(CoreUtils::LogLevel::ERROR, "PCAP packet header size (" + std::to_string(pcap_packet_header_size_) + ") is too small to contain standard length field at offset 8.");
        return false;
    }

    memcpy(&out_length, local_header_buffer.data() + 8, sizeof(uint32_t));
    // No endian conversion here, assuming native as per user's example for their specific log files.
    // For standard PCAP files, the global header's magic number indicates endianness,
    // and packet header fields follow that. This simulator is simplified.
    return true;
}


std::vector<unsigned char> LogFilePacketSimulator::get_next_taifex_packet() {
    if (!is_open() || !has_next_packet()) {
        return {};
    }

    uint32_t captured_data_length = 0;
    // The helper read_pcap_packet_captured_length now reads from file_stream_
    if (!read_pcap_packet_captured_length(captured_data_length)) {
        // Appropriate log message already handled in read_pcap_packet_captured_length
        return {};
    }

    if (captured_data_length == 0) {
        CoreUtils::Logger::Log(CoreUtils::LogLevel::WARNING, "PCAP record indicates zero captured data length in: " + log_filepath_ + ". Skipping this record.");
        return {};
    }

    const uint32_t MAX_SANE_PACKET_LENGTH = 70000; // Max typical UDP packet is ~65507 bytes.
    if (captured_data_length > MAX_SANE_PACKET_LENGTH) {
        CoreUtils::Logger::Log(CoreUtils::LogLevel::ERROR, "PCAP record indicates excessively large captured data length (" +
                               std::to_string(captured_data_length) + ") in: " + log_filepath_ + ". This might indicate file corruption or incorrect header size configuration. Stopping further processing of this file.");
        // Corrupted length could lead to trying to read huge amounts of data.
        // It's safer to stop processing this file to avoid issues.
        close(); // Close the file to prevent further reads
        return {};
    }

    std::vector<unsigned char> captured_data_buf(captured_data_length);
    if (!file_stream_.read(reinterpret_cast<char*>(captured_data_buf.data()), captured_data_length)) {
        CoreUtils::Logger::Log(CoreUtils::LogLevel::ERROR, "Failed to read captured packet data (expected length " +
                               std::to_string(captured_data_length) + ", read " + std::to_string(file_stream_.gcount()) + " bytes) from: " + log_filepath_);
        return {};
    }

    auto it = std::find(captured_data_buf.begin(), captured_data_buf.end(), TAIFEX_ESC_CODE);

    if (it == captured_data_buf.end()) {
        CoreUtils::Logger::Log(CoreUtils::LogLevel::DEBUG, "TAIFEX ESC code (0x1B) not found in current captured packet data segment in: " + log_filepath_);
        return {};
    }

    return std::vector<unsigned char>(it, captured_data_buf.end());
}

} // namespace Utils
