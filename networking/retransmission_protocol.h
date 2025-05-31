#ifndef RETRANSMISSION_PROTOCOL_H
#define RETRANSMISSION_PROTOCOL_H

#include <cstdint>
#include <vector>
#include <string> // Required for CheckCode calculation if password is std::string

namespace TaifexRetransmission {

// Max password length if needed for CheckCode context, not directly in protocol structs
// const size_t MAX_RETRANS_PASSWORD_LEN = 16; // Example

#pragma pack(push, 1) // Ensure packed structures for network messages

struct StandardTimeFormat {
    uint32_t epoch_s;    // Seconds since Epoch (00:00:00 UTC 1970/1/1)
    uint32_t nanosecond;

    void serialize(std::vector<unsigned char>& buffer) const;
    bool deserialize(const unsigned char* data, size_t& offset, size_t total_len);
};

struct RetransmissionMsgHeader {
    uint16_t msg_size;   // Length from field after MsgSize to CheckSum (exclusive of CheckSum)
    uint16_t msg_type;
    uint32_t msg_seq_num; // Resets to 0 after each login
    StandardTimeFormat msg_time;

    void serialize(std::vector<unsigned char>& buffer) const;
    bool deserialize(const unsigned char* data, size_t& offset, size_t total_len);
    static constexpr size_t SIZE = 2 + 2 + 4 + (4 + 4);
};

struct RetransmissionMsgFooter {
    uint8_t check_sum; // Sum of bytes from MsgSize to byte before CheckSum, then modulo 256

    void serialize(std::vector<unsigned char>& buffer, const unsigned char* msg_start_for_checksum, size_t len_for_checksum) const;
    bool deserialize(const unsigned char* data, size_t& offset, size_t total_len,
                     const unsigned char* msg_start_for_checksum, size_t len_for_checksum);
    static constexpr size_t SIZE = 1;
};


// --- Specific Message Structs ---

struct LoginRequest020 {
    RetransmissionMsgHeader header;
    uint16_t multiplication_operator; // > 1
    uint8_t  check_code; // (multiplication_operator * password) take thousands and hundreds digit
    uint16_t session_id;
    RetransmissionMsgFooter footer;

    static const uint16_t MESSAGE_TYPE = 20;
    void serialize(std::vector<unsigned char>& buffer, const std::string& password_str) const; // Password needed for check_code
    bool deserialize(const unsigned char* data, size_t len, const std::string& password_str); // For validation if needed
};

struct LoginResponse030 {
    RetransmissionMsgHeader header;
    uint16_t channel_id;
    RetransmissionMsgFooter footer;

    static const uint16_t MESSAGE_TYPE = 30;
    void serialize(std::vector<unsigned char>& buffer) const;
    bool deserialize(const unsigned char* data, size_t len);
};

struct RetransmissionStart050 {
    RetransmissionMsgHeader header;
    RetransmissionMsgFooter footer;

    static const uint16_t MESSAGE_TYPE = 50;
    void serialize(std::vector<unsigned char>& buffer) const;
    bool deserialize(const unsigned char* data, size_t len);
};

struct HeartbeatServer104 {
    RetransmissionMsgHeader header;
    RetransmissionMsgFooter footer;

    static const uint16_t MESSAGE_TYPE = 104;
    void serialize(std::vector<unsigned char>& buffer) const;
    bool deserialize(const unsigned char* data, size_t len);
};

struct HeartbeatClient105 {
    RetransmissionMsgHeader header;
    RetransmissionMsgFooter footer;

    static const uint16_t MESSAGE_TYPE = 105;
    void serialize(std::vector<unsigned char>& buffer) const;
    bool deserialize(const unsigned char* data, size_t len);
};

struct DataRequest101 {
    RetransmissionMsgHeader header;
    uint16_t channel_id;
    uint32_t begin_seq_no;
    uint16_t recover_num; // <=1 means recover that one message
    RetransmissionMsgFooter footer;

    static const uint16_t MESSAGE_TYPE = 101;
    void serialize(std::vector<unsigned char>& buffer) const;
    bool deserialize(const unsigned char* data, size_t len);
};

struct DataResponse102 {
    RetransmissionMsgHeader header;
    uint16_t channel_id;
    uint8_t  status_code; // See status code list in spec
    uint32_t begin_seq_no;
    uint16_t recover_num;
    // Followed by variable length data (the retransmitted messages themselves)
    // std::vector<unsigned char> retransmitted_data; // Not part of fixed struct, handled in (de)serialization
    RetransmissionMsgFooter footer; // Footer is after variable data

    static const uint16_t MESSAGE_TYPE = 102;
    // Serialization/deserialization needs to handle the variable data part.
    void serialize(std::vector<unsigned char>& buffer, const std::vector<unsigned char>& retrans_data) const;
    bool deserialize(const unsigned char* data, size_t len, std::vector<unsigned char>& out_retrans_data);
};

struct ErrorNotification010 {
    RetransmissionMsgHeader header;
    uint8_t  status_code;
    RetransmissionMsgFooter footer;

    static const uint16_t MESSAGE_TYPE = 10;
    void serialize(std::vector<unsigned char>& buffer) const;
    bool deserialize(const unsigned char* data, size_t len);
};

#pragma pack(pop)

// Helper function to calculate retransmission checksum
uint8_t calculate_retransmission_checksum(const unsigned char* data, size_t length);

} // namespace TaifexRetransmission
#endif // RETRANSMISSION_PROTOCOL_H
