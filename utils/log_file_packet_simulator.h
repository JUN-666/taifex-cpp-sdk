#ifndef LOG_FILE_PACKET_SIMULATOR_H
#define LOG_FILE_PACKET_SIMULATOR_H

#include <string>
#include <vector>
#include <fstream> // For std::ifstream
#include <cstdint> // For uint32_t etc.

namespace Utils {

/**
 * @brief Simulates packet delivery by reading from a PCAP-like log file.
 *
 * This class reads a binary log file that is expected to have a structure similar
 * to PCAP files (though simplified here):
 * 1. A global header (e.g., 24 bytes), which is skipped.
 * 2. A sequence of packet records, each starting with a packet header
 *    (e.g., 16 bytes).
 * 3. The packet header contains information about the captured packet data, including
 *    its length. The relevant field for this simulator is the "captured length"
 *    of the packet data.
 * 4. The actual TAIFEX message (which typically starts with an 0x1B ESC code) is assumed
 *    to be embedded within this captured packet data. This simulator will search for
 *    the 0x1B code to identify the start of the TAIFEX message.
 *
 * The simulator extracts these raw TAIFEX messages one by one.
 */
class LogFilePacketSimulator {
public:
    /**
     * @brief Constructor.
     * @param filepath Path to the PCAP-like log file.
     * @param global_header_size The size of the global file header to skip (e.g., 24 for standard PCAP).
     * @param packet_header_size The size of each packet record's header (e.g., 16 for standard PCAP packet header).
     */
    explicit LogFilePacketSimulator(const std::string& filepath,
                                    size_t global_header_size = DEFAULT_GLOBAL_HEADER_SIZE,
                                    size_t packet_header_size = DEFAULT_PACKET_HEADER_SIZE);

    /**
     * @brief Destructor. Ensures the log file is closed.
     */
    ~LogFilePacketSimulator();

    // Disable copy and assignment to prevent issues with file stream ownership.
    LogFilePacketSimulator(const LogFilePacketSimulator&) = delete;
    LogFilePacketSimulator& operator=(const LogFilePacketSimulator&) = delete;

    /**
     * @brief Opens the log file and prepares it for reading.
     * Skips the global file header.
     * @return True if the file was opened successfully and the global header was skipped, false otherwise.
     */
    bool open();

    /**
     * @brief Closes the log file if it is open.
     */
    void close();

    /**
     * @brief Checks if the log file is currently open and in a good state for reading.
     * @return True if open and readable, false otherwise.
     */
    bool is_open() const;

    /**
     * @brief Checks if there are potentially more packets to read from the log file.
     * This primarily checks for End-Of-File (EOF) and error states of the file stream.
     * @return True if more packets might be available, false if EOF is reached or an error occurred.
     */
    bool has_next_packet();

    /**
     * @brief Reads the next packet record from the log file and extracts the TAIFEX message.
     *
     * This method performs the following steps:
     * 1. Reads the PCAP-like packet header (e.g., 16 bytes).
     * 2. Extracts the "captured length" of the packet data from this header.
     *    (Assumes this length is at a fixed offset and is a 4-byte integer).
     * 3. Reads the packet data block of the "captured length".
     * 4. Searches for the `TAIFEX_ESC_CODE` (0x1B) within this data block.
     * 5. If found, returns the rest of the data block from the ESC code onwards as the TAIFEX message.
     *
     * @return A vector of unsigned char containing the raw TAIFEX message (starting with 0x1B).
     *         Returns an empty vector if reading fails (e.g., EOF reached prematurely,
     *         packet header read error, data read error) or if the TAIFEX ESC code is not found
     *         within the captured packet data.
     */
    std::vector<unsigned char> get_next_taifex_packet();

private:
    // Default sizes based on standard PCAP format.
    static const size_t DEFAULT_GLOBAL_HEADER_SIZE = 24;
    static const size_t DEFAULT_PACKET_HEADER_SIZE = 16;
    static const unsigned char TAIFEX_ESC_CODE = 0x1B;

    std::string log_filepath_;
    std::ifstream file_stream_;
    bool is_file_open_;

    size_t global_header_to_skip_;
    size_t pcap_packet_header_size_; // Size of the per-packet header (e.g., 16 bytes in PCAP)

    /**
     * @brief Reads the "captured length" field from a PCAP-like packet header.
     * Assumes the PCAP packet header has been read into a buffer.
     * The standard PCAP packet header (16 bytes) has "Captured Packet Length" (incl_len)
     * as a 4-byte field, typically at offset 8, in the file's native endianness.
     * @param header_buffer A buffer containing the raw bytes of the packet header.
     *                      Must be at least `pcap_packet_header_size_` bytes long.
     * @param out_length Reference to store the extracted captured length.
     * @return True if the length was successfully read, false otherwise (e.g. read error from stream).
     */
    bool read_pcap_packet_captured_length(uint32_t& out_length);
};

} // namespace Utils
#endif // LOG_FILE_PACKET_SIMULATOR_H
