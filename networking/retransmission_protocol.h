#ifndef RETRANSMISSION_PROTOCOL_H
#define RETRANSMISSION_PROTOCOL_H

#include <cstdint>
#include <vector>
#include <string>

namespace TaifexRetransmission {

#pragma pack(push, 1) // Ensure packed structures for network messages

/**
 * @brief Represents time in seconds since Epoch and nanoseconds.
 * Used in RetransmissionMsgHeader.
 */
struct StandardTimeFormat {
    /** @brief Seconds since UNIX Epoch (00:00:00 UTC, January 1, 1970). Network Byte Order. */
    uint32_t epoch_s;
    /** @brief Nanoseconds part of the timestamp. Network Byte Order. */
    uint32_t nanosecond;

    /** @brief Serializes this struct into the provided buffer. Handles endian conversion. */
    void serialize(std::vector<unsigned char>& buffer) const;
    /** @brief Deserializes this struct from the provided data. Handles endian conversion.
     *  @param offset Is advanced by the number of bytes read.
     */
    bool deserialize(const unsigned char* data, size_t& offset, size_t total_len);
    /** @brief Fixed size of the serialized StandardTimeFormat struct (4 + 4 = 8 bytes). */
    static constexpr size_t SIZE = sizeof(uint32_t) + sizeof(uint32_t);
};

/**
 * @brief Common header for all TAIFEX retransmission protocol messages.
 */
struct RetransmissionMsgHeader {
    /**
     * @brief Message size. Defined as the length in bytes from the field immediately
     *        following MsgSize (i.e., MsgType) up to, but not including, the CheckSum field.
     *        Network Byte Order.
     */
    uint16_t msg_size;
    /** @brief Message type identifier (e.g., 20 for LoginRequest020). Network Byte Order. */
    uint16_t msg_type;
    /**
     * @brief Message sequence number, assigned by the sender.
     *        Resets to 0 after each successful login for client-sent messages.
     *        Server also maintains its own sequence for messages it sends.
     *        Network Byte Order.
     */
    uint32_t msg_seq_num;
    /** @brief Timestamp of when the message was generated. */
    StandardTimeFormat msg_time;

    /** @brief Serializes this struct into the provided buffer. Handles endian conversion. */
    void serialize(std::vector<unsigned char>& buffer) const;
    /** @brief Deserializes this struct from the provided data. Handles endian conversion.
     *  @param offset Is advanced by the number of bytes read.
     */
    bool deserialize(const unsigned char* data, size_t& offset, size_t total_len);
    /** @brief Fixed size of the serialized RetransmissionMsgHeader struct (2+2+4+8 = 16 bytes). */
    static constexpr size_t SIZE = sizeof(uint16_t) * 2 + sizeof(uint32_t) + StandardTimeFormat::SIZE;
};

/**
 * @brief Common footer for all TAIFEX retransmission protocol messages.
 */
struct RetransmissionMsgFooter {
    /**
     * @brief Checksum. Calculated as the sum of bytes from the MsgSize field (inclusive)
     *        up to the byte immediately preceding the CheckSum field, modulo 256.
     */
    uint8_t check_sum;

    /** @brief Serializes this struct into the provided buffer. Assumes check_sum member is pre-calculated. */
    void serialize(std::vector<unsigned char>& buffer, const unsigned char* msg_start_for_checksum, size_t len_for_checksum) const;
    /**
     * @brief Deserializes this struct from the provided data and validates the checksum.
     * @param offset Is advanced by the number of bytes read.
     * @param msg_start_for_checksum Pointer to the start of the data range for checksum calculation (i.e., start of MsgSize field).
     * @param len_for_checksum Length of the data range for checksum calculation.
     */
    bool deserialize(const unsigned char* data, size_t& offset, size_t total_len,
                     const unsigned char* msg_start_for_checksum, size_t len_for_checksum);
    /** @brief Fixed size of the serialized RetransmissionMsgFooter struct (1 byte). */
    static constexpr size_t SIZE = sizeof(uint8_t);
};


// --- Specific Message Structs ---

/**
 * @brief Login Request message (MsgType 20). Sent by client to initiate a session.
 */
struct LoginRequest020 {
    RetransmissionMsgHeader header;
    /** @brief Multiplication Operator (>1). Used with password to calculate CheckCode. Network Byte Order. */
    uint16_t multiplication_operator;
    /** @brief Check Code. Calculated as: `(multiplication_operator * password_numeric_value) / 100 % 100`. */
    uint8_t  check_code;
    /** @brief Session ID provided by TAIFEX. Network Byte Order. */
    uint16_t session_id;
    RetransmissionMsgFooter footer;

    static const uint16_t MESSAGE_TYPE = 20;
    /** @brief Serializes LoginRequest020. `password_str` is used if check_code needs to be calculated by serialize. */
    void serialize(std::vector<unsigned char>& buffer, const std::string& password_str) const;
    /** @brief Deserializes LoginRequest020. `password_str` can be used to validate check_code if needed. */
    bool deserialize(const unsigned char* data, size_t len, const std::string& password_str);
};

/**
 * @brief Login Response message (MsgType 030). Sent by server in response to LoginRequest020.
 * Confirms receipt of login and provides the Channel ID for subsequent data requests if applicable.
 * Multiple 030 messages might be sent if the user is subscribed to multiple channels.
 * Successful login is typically fully confirmed by a RetransmissionStart050 message.
 */
struct LoginResponse030 {
    RetransmissionMsgHeader header;
    /** @brief Channel ID assigned or confirmed for the session. Network Byte Order. */
    uint16_t channel_id;
    RetransmissionMsgFooter footer;

    static const uint16_t MESSAGE_TYPE = 30;
    void serialize(std::vector<unsigned char>& buffer) const;
    bool deserialize(const unsigned char* data, size_t len);
};

/**
 * @brief Retransmission Start message (MsgType 050). Sent by server to indicate readiness
 * to accept data requests after a successful login.
 */
struct RetransmissionStart050 {
    RetransmissionMsgHeader header;
    RetransmissionMsgFooter footer;

    static const uint16_t MESSAGE_TYPE = 50;
    void serialize(std::vector<unsigned char>& buffer) const;
    bool deserialize(const unsigned char* data, size_t len);
};

/**
 * @brief Heartbeat message sent by the server (MsgType 104).
 * Used to maintain the connection and check liveness. Client should respond with HeartbeatClient105.
 */
struct HeartbeatServer104 {
    RetransmissionMsgHeader header;
    RetransmissionMsgFooter footer;

    static const uint16_t MESSAGE_TYPE = 104;
    void serialize(std::vector<unsigned char>& buffer) const;
    bool deserialize(const unsigned char* data, size_t len);
};

/**
 * @brief Heartbeat message sent by the client (MsgType 105) in response to HeartbeatServer104.
 */
struct HeartbeatClient105 {
    RetransmissionMsgHeader header;
    RetransmissionMsgFooter footer;

    static const uint16_t MESSAGE_TYPE = 105;
    void serialize(std::vector<unsigned char>& buffer) const;
    bool deserialize(const unsigned char* data, size_t len);
};

/**
 * @brief Data Request message (MsgType 101). Sent by client to request retransmission of messages.
 */
struct DataRequest101 {
    RetransmissionMsgHeader header;
    /** @brief Channel ID from which to retransmit messages. Network Byte Order. */
    uint16_t channel_id;
    /** @brief Starting sequence number (CHANNEL-SEQ) for retransmission. Network Byte Order. */
    uint32_t begin_seq_no;
    /** @brief Number of messages to recover. If <=1, recovers only the message with `begin_seq_no`. Network Byte Order. */
    uint16_t recover_num;
    RetransmissionMsgFooter footer;

    static const uint16_t MESSAGE_TYPE = 101;
    void serialize(std::vector<unsigned char>& buffer) const;
    bool deserialize(const unsigned char* data, size_t len);
};

/**
 * @brief Data Response message (MsgType 102). Sent by server in response to DataRequest101.
 * Indicates status of the request and is followed by the actual retransmitted market data messages (if successful).
 * The retransmitted market data messages are standard TAIFEX messages (starting with 0x1B) and are
 * not encapsulated further by this protocol, they are streamed directly after this 102 message if Status Code is 0.
 */
struct DataResponse102 {
    RetransmissionMsgHeader header;
    /** @brief Channel ID for which this response applies. Network Byte Order. */
    uint16_t channel_id;
    /** @brief Status code indicating success or failure of the data request. (e.g., 0: OK, 2: No Data, 7: Request Error). */
    uint8_t  status_code;
    /** @brief Begin sequence number from the request. Network Byte Order. */
    uint32_t begin_seq_no;
    /** @brief Number of messages recovered from the request. Network Byte Order. */
    uint16_t recover_num;
    // Variable length retransmitted data follows this fixed part, before the footer.
    // This data is handled by serialize/deserialize methods, not as a direct struct member.
    RetransmissionMsgFooter footer;

    static const uint16_t MESSAGE_TYPE = 102;
    /** @brief Serializes DataResponse102. `retrans_data` contains the raw market data messages to be sent after the fixed part. */
    void serialize(std::vector<unsigned char>& buffer, const std::vector<unsigned char>& retrans_data) const;
    /** @brief Deserializes DataResponse102. Populates `out_retrans_data` with the variable retransmitted market data. */
    bool deserialize(const unsigned char* data, size_t len, std::vector<unsigned char>& out_retrans_data);
};

/**
 * @brief Error Notification message (MsgType 010). Sent by server to indicate an error condition.
 * The server will typically disconnect after sending this message.
 */
struct ErrorNotification010 {
    RetransmissionMsgHeader header;
    /** @brief Status code indicating the type of error. */
    uint8_t  status_code;
    RetransmissionMsgFooter footer;

    static const uint16_t MESSAGE_TYPE = 10;
    void serialize(std::vector<unsigned char>& buffer) const;
    bool deserialize(const unsigned char* data, size_t len);
};

#pragma pack(pop)

/**
 * @brief Calculates the checksum for TAIFEX retransmission protocol messages.
 * The checksum is the sum of bytes from the 'MsgSize' field (inclusive) up to
 * the byte immediately preceding the 'CheckSum' field, modulo 256.
 * @param data Pointer to the start of the data range for checksum calculation (i.e., start of MsgSize field).
 * @param length The length of the data range.
 * @return The calculated 8-bit checksum.
 */
uint8_t calculate_retransmission_checksum(const unsigned char* data, size_t length);

} // namespace TaifexRetransmission
#endif // RETRANSMISSION_PROTOCOL_H
