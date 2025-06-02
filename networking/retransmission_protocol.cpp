#include "networking/retransmission_protocol.h"
#include "networking/endian_utils.h" // For byte order conversions
#include "logger.h"       // For logging errors

#include <vector>
#include <numeric>   // For std::accumulate (alternative for checksum, but direct loop is fine)
#include <cstring>   // For memcpy
#include <stdexcept> // For errors during string to int for password in checkcode

namespace TaifexRetransmission {

// --- Helper Function ---
uint8_t calculate_retransmission_checksum(const unsigned char* data, size_t length) {
    uint32_t sum = 0;
    for (size_t i = 0; i < length; ++i) {
        sum += data[i];
    }
    return static_cast<uint8_t>(sum % 256);
}

// --- StandardTimeFormat ---
void StandardTimeFormat::serialize(std::vector<unsigned char>& buffer) const {
    uint32_t net_epoch_s = NetworkingUtils::host_to_network_long(epoch_s);
    uint32_t net_nanosecond = NetworkingUtils::host_to_network_long(nanosecond);

    size_t original_size = buffer.size();
    buffer.resize(original_size + sizeof(net_epoch_s) + sizeof(net_nanosecond));

    memcpy(buffer.data() + original_size, &net_epoch_s, sizeof(net_epoch_s));
    memcpy(buffer.data() + original_size + sizeof(net_epoch_s), &net_nanosecond, sizeof(net_nanosecond));
}

bool StandardTimeFormat::deserialize(const unsigned char* data, size_t& offset, size_t total_len) {
    if (offset + sizeof(epoch_s) + sizeof(nanosecond) > total_len) {
        LOG_ERROR << "StandardTimeFormat deserialize: Not enough data.";
        return false;
    }

    memcpy(&epoch_s, data + offset, sizeof(epoch_s));
    offset += sizeof(epoch_s);
    epoch_s = NetworkingUtils::network_to_host_long(epoch_s);

    memcpy(&nanosecond, data + offset, sizeof(nanosecond));
    offset += sizeof(nanosecond);
    nanosecond = NetworkingUtils::network_to_host_long(nanosecond);
    return true;
}

// --- RetransmissionMsgHeader ---
void RetransmissionMsgHeader::serialize(std::vector<unsigned char>& buffer) const {
    uint16_t net_msg_size = NetworkingUtils::host_to_network_short(msg_size);
    uint16_t net_msg_type = NetworkingUtils::host_to_network_short(msg_type);
    uint32_t net_msg_seq_num = NetworkingUtils::host_to_network_long(msg_seq_num);

    size_t original_size = buffer.size();
    buffer.resize(original_size + SIZE); // SIZE includes StandardTimeFormat::SIZE

    size_t current_offset = original_size;
    memcpy(buffer.data() + current_offset, &net_msg_size, sizeof(net_msg_size));
    current_offset += sizeof(net_msg_size);
    memcpy(buffer.data() + current_offset, &net_msg_type, sizeof(net_msg_type));
    current_offset += sizeof(net_msg_type);
    memcpy(buffer.data() + current_offset, &net_msg_seq_num, sizeof(net_msg_seq_num));
    current_offset += sizeof(net_msg_seq_num);

    // Serialize msg_time into a temporary buffer then copy, or make msg_time.serialize append to existing buffer at offset
    // For simplicity, let msg_time.serialize append directly if buffer is passed by ref.
    // To use the current_offset approach with msg_time.serialize:
    std::vector<unsigned char> time_buffer;
    msg_time.serialize(time_buffer); // msg_time handles its own endianness
    memcpy(buffer.data() + current_offset, time_buffer.data(), time_buffer.size());
}

bool RetransmissionMsgHeader::deserialize(const unsigned char* data, size_t& offset, size_t total_len) {
    if (offset + SIZE > total_len) {
        LOG_ERROR << "RetransmissionMsgHeader deserialize: Not enough data for header.";
        return false;
    }

    memcpy(&msg_size, data + offset, sizeof(msg_size));
    offset += sizeof(msg_size);
    msg_size = NetworkingUtils::network_to_host_short(msg_size);

    memcpy(&msg_type, data + offset, sizeof(msg_type));
    offset += sizeof(msg_type);
    msg_type = NetworkingUtils::network_to_host_short(msg_type);

    memcpy(&msg_seq_num, data + offset, sizeof(msg_seq_num));
    offset += sizeof(msg_seq_num);
    msg_seq_num = NetworkingUtils::network_to_host_long(msg_seq_num);

    if (!msg_time.deserialize(data, offset, total_len)) {
        LOG_ERROR << "RetransmissionMsgHeader deserialize: Failed to parse msg_time.";
        return false; // msg_time.deserialize already advanced offset
    }
    return true;
}


// --- RetransmissionMsgFooter ---
void RetransmissionMsgFooter::serialize(std::vector<unsigned char>& buffer, const unsigned char* /*msg_start_for_checksum*/, size_t /*len_for_checksum*/) const {
    // Checksum is assumed to be pre-calculated and stored in this->check_sum by the specific message's serialize method.
    buffer.push_back(check_sum);
}

bool RetransmissionMsgFooter::deserialize(const unsigned char* data, size_t& offset, size_t total_len,
                                          const unsigned char* msg_start_for_checksum, size_t len_for_checksum) {
    if (offset + SIZE > total_len) {
        LOG_ERROR << "RetransmissionMsgFooter deserialize: Not enough data for footer.";
        return false;
    }

    uint8_t received_checksum = data[offset];
    offset += SIZE;

    if (!msg_start_for_checksum || len_for_checksum == 0) {
         LOG_ERROR << "RetransmissionMsgFooter deserialize: Invalid parameters for checksum calculation.";
        return false;
    }
    uint8_t calculated = calculate_retransmission_checksum(msg_start_for_checksum, len_for_checksum);
    if (calculated != received_checksum) {
        LOG_ERROR << "Retransmission checksum mismatch. Calculated: " << calculated << ", Received: " << received_checksum;
        return false;
    }
    this->check_sum = received_checksum;
    return true;
}

// --- LoginRequest020 ---
// MsgSize for 020 = (MsgType(2) + MsgSeqNum(4) + MsgTime(8)) + (MultOp(2) + CheckCode(1) + SessionID(2))
//                   = 14 (header part for msg_size) + 5 (payload part for msg_size) = 19
const uint16_t LOGIN_REQUEST_020_PAYLOAD_SIZE = sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint16_t); // MultOp + CheckCode + SessionID
const uint16_t LOGIN_REQUEST_020_MSG_SIZE_FIELD_VALUE = (RetransmissionMsgHeader::SIZE - sizeof(uint16_t) /*msg_size field itself*/) + LOGIN_REQUEST_020_PAYLOAD_SIZE;


uint8_t calculate_check_code(uint16_t mult_op, const std::string& password_str) {
    if (password_str.empty()) {
        LOG_ERROR << "Password cannot be empty for CheckCode calculation.";
        return 0;
    }
    long long password_val = 0;
    try {
        // Password may not be purely numeric, spec is unclear. If it can be alphanumeric, stoll will fail.
        // Assuming it's a string of digits for now.
        password_val = std::stoll(password_str);
    } catch (const std::invalid_argument& ia) {
        LOG_ERROR << "Invalid password format (not a number) for CheckCode: " << password_str << " Error: " << ia.what();
        return 0;
    } catch (const std::out_of_range& oor) {
        LOG_ERROR << "Password value out of range for CheckCode: " << password_str << " Error: " << oor.what();
        return 0;
    }

    long long product = static_cast<long long>(mult_op) * password_val;
    if (product < 0) product = -product;

    return static_cast<uint8_t>((product / 100) % 100);
}


void LoginRequest020::serialize(std::vector<unsigned char>& buffer, const std::string& password_str) const {
    // Ensure header member is correctly prepared before serializing it
    RetransmissionMsgHeader header_to_serialize = this->header; // Make a mutable copy
    header_to_serialize.msg_type = MESSAGE_TYPE;
    header_to_serialize.msg_size = LOGIN_REQUEST_020_MSG_SIZE_FIELD_VALUE;

    // The buffer for checksum calculation will be built up.
    // Checksum is on fields from MsgSize (inclusive) to SessionID (inclusive).
    // So, it covers: actual_msg_size_field + (header_to_serialize.msg_size value)

    size_t initial_buffer_size = buffer.size(); // In case buffer is not empty

    header_to_serialize.serialize(buffer); // Appends header fields (including its own msg_size)

    // Payload
    uint16_t net_mult_op = NetworkingUtils::host_to_network_short(multiplication_operator);
    size_t current_payload_offset = buffer.size();
    buffer.resize(current_payload_offset + LOGIN_REQUEST_020_PAYLOAD_SIZE);

    memcpy(buffer.data() + current_payload_offset, &net_mult_op, sizeof(net_mult_op));
    current_payload_offset += sizeof(net_mult_op);

    // check_code member should be pre-calculated by the caller or calculated here if method is non-const
    // If this->check_code is not set, it must be calculated:
    // uint8_t calculated_check_code = calculate_check_code(this->multiplication_operator, password_str);
    // buffer[current_payload_offset] = calculated_check_code;
    buffer[current_payload_offset] = this->check_code; // Assuming it's pre-set
    current_payload_offset++;

    uint16_t net_session_id = NetworkingUtils::host_to_network_short(session_id);
    memcpy(buffer.data() + current_payload_offset, &net_session_id, sizeof(net_session_id));
    // current_payload_offset += sizeof(net_session_id); // Not needed after last memcpy to payload section

    // Footer (Checksum)
    // Data for checksum starts from the serialized msg_size field itself.
    // Length for checksum = sizeof(uint16_t for msg_size) + LOGIN_REQUEST_020_MSG_SIZE_FIELD_VALUE
    size_t checksum_data_len = sizeof(uint16_t) + LOGIN_REQUEST_020_MSG_SIZE_FIELD_VALUE;

    RetransmissionMsgFooter footer_to_serialize = this->footer; // Make a mutable copy
    footer_to_serialize.check_sum = calculate_retransmission_checksum(buffer.data() + initial_buffer_size, checksum_data_len);
    footer_to_serialize.serialize(buffer, nullptr, 0);
}


bool LoginRequest020::deserialize(const unsigned char* data, size_t len, const std::string& password_str) {
    size_t offset = 0;
    if (!header.deserialize(data, offset, len)) {
        LOG_ERROR << "LoginRequest020 deserialize: Header parsing failed.";
        return false;
    }
    if (header.msg_type != MESSAGE_TYPE) {
        LOG_ERROR << "LoginRequest020 deserialize: Incorrect message type. Expected: " << MESSAGE_TYPE << ", Got: " << header.msg_type;
        return false;
    }
    if (header.msg_size != LOGIN_REQUEST_020_MSG_SIZE_FIELD_VALUE) {
        LOG_ERROR << "LoginRequest020 deserialize: Incorrect msg_size. Expected: " << LOGIN_REQUEST_020_MSG_SIZE_FIELD_VALUE << ", Got: " << header.msg_size;
        return false;
    }

    // Total expected message length from start of data = sizeof(MsgSize field) + header.msg_size (payload for checksum) + sizeof(CheckSum byte)
    size_t expected_full_msg_len = sizeof(uint16_t) + header.msg_size + RetransmissionMsgFooter::SIZE;
    if (len < expected_full_msg_len) {
        LOG_ERROR << "LoginRequest020 deserialize: Data length too short for declared msg_size. Expected: " << expected_full_msg_len << ", Got: " << len;
        return false;
    }


    // Payload (offset is currently after header)
    if (offset + LOGIN_REQUEST_020_PAYLOAD_SIZE > len) { // Check if enough data for payload based on offset
         LOG_ERROR << "LoginRequest020 deserialize: Not enough data for payload.";
        return false;
    }
    memcpy(&multiplication_operator, data + offset, sizeof(multiplication_operator));
    offset += sizeof(multiplication_operator);
    multiplication_operator = NetworkingUtils::network_to_host_short(multiplication_operator);

    check_code = data[offset++];

    // Optional: Validate received check_code (only if password_str is available and context allows)
    // uint8_t expected_check_code = calculate_check_code(this->multiplication_operator, password_str);
    // if (this->check_code != expected_check_code) {
    //    LOG_ERROR << "LoginRequest020 deserialize: CheckCode mismatch.";
    //    return false;
    // }

    memcpy(&session_id, data + offset, sizeof(session_id));
    offset += sizeof(session_id);
    session_id = NetworkingUtils::network_to_host_short(session_id);

    // Footer (Checksum validation)
    // Data for checksum starts from data[0] (MsgSize field) and has length = sizeof(uint16_t for MsgSize) + header.msg_size
    size_t checksum_data_len = sizeof(uint16_t) + header.msg_size;
    if (!footer.deserialize(data, offset, len, data, checksum_data_len)) {
        LOG_ERROR << "LoginRequest020 deserialize: Footer checksum validation failed.";
        return false;
    }

    return offset == expected_full_msg_len; // Ensure all bytes up to end of message were consumed
}


// Implementations for other message structs (LoginResponse030, etc.) would follow a similar pattern.


// --- LoginResponse030 ---
// MsgSize for 030 = (MsgType(2) + MsgSeqNum(4) + MsgTime(8)) + ChannelID(2) = 14 + 2 = 16
const uint16_t LOGIN_RESPONSE_030_PAYLOAD_SIZE = sizeof(uint16_t); // ChannelID
const uint16_t LOGIN_RESPONSE_030_MSG_SIZE_FIELD_VALUE = (RetransmissionMsgHeader::SIZE - sizeof(uint16_t) /*msg_size field itself*/) + LOGIN_RESPONSE_030_PAYLOAD_SIZE;

void LoginResponse030::serialize(std::vector<unsigned char>& buffer) const {
    RetransmissionMsgHeader mutable_header = header; // Make a mutable copy
    mutable_header.msg_type = MESSAGE_TYPE;
    mutable_header.msg_size = LOGIN_RESPONSE_030_MSG_SIZE_FIELD_VALUE;

    size_t initial_buffer_size = buffer.size();
    mutable_header.serialize(buffer);

    uint16_t net_channel_id = NetworkingUtils::host_to_network_short(channel_id);
    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(&net_channel_id);
    buffer.insert(buffer.end(), ptr, ptr + sizeof(net_channel_id)); // Appends to end

    RetransmissionMsgFooter mutable_footer = footer; // Make a mutable copy
    // Checksum is from MsgSize field (start of header part in buffer) up to end of payload
    // Length for checksum = sizeof(uint16_t for msg_size field) + value of msg_size field in header
    mutable_footer.check_sum = calculate_retransmission_checksum(buffer.data() + initial_buffer_size, sizeof(uint16_t) + mutable_header.msg_size);
    mutable_footer.serialize(buffer, nullptr, 0);
}

bool LoginResponse030::deserialize(const unsigned char* data, size_t len) {
    size_t offset = 0;
    if (!header.deserialize(data, offset, len)) {
         LOG_ERROR << "LoginResponse030 deserialize: Header parsing failed.";
        return false;
    }
    if (header.msg_type != MESSAGE_TYPE) {
        LOG_ERROR << "LoginResponse030: Type mismatch. Expected: " << MESSAGE_TYPE << ", Got: " << header.msg_type;
        return false;
    }
    if (header.msg_size != LOGIN_RESPONSE_030_MSG_SIZE_FIELD_VALUE) {
        LOG_ERROR << "LoginResponse030: Size mismatch. Expected: " << LOGIN_RESPONSE_030_MSG_SIZE_FIELD_VALUE << ", Got: " << header.msg_size;
        return false;
    }

    size_t expected_full_msg_len = sizeof(uint16_t) + header.msg_size + RetransmissionMsgFooter::SIZE;
    if (len < expected_full_msg_len) {
        LOG_ERROR << "LoginResponse030: Overall length too short. Expected at least: " << expected_full_msg_len << ", Got: " << len;
        return false;
    }

    if (offset + sizeof(channel_id) > len) { // Check against total length, though already covered by expected_full_msg_len indirectly
        LOG_ERROR << "LoginResponse030: Not enough data for channel_id.";
        return false;
    }
    memcpy(&channel_id, data + offset, sizeof(channel_id));
    offset += sizeof(channel_id);
    channel_id = NetworkingUtils::network_to_host_short(channel_id);

    if (!footer.deserialize(data, offset, len, data, sizeof(uint16_t) + header.msg_size)) {
        LOG_ERROR << "LoginResponse030: Footer checksum validation failed.";
        return false;
    }

    return offset == expected_full_msg_len;
}

// --- RetransmissionStart050 ---
// MsgSize for 050 = MsgType(2) + MsgSeqNum(4) + MsgTime(8) = 14 (empty payload)
const uint16_t RETRANSMISSION_START_050_PAYLOAD_SIZE = 0;
const uint16_t RETRANSMISSION_START_050_MSG_SIZE_FIELD_VALUE = RetransmissionMsgHeader::SIZE - sizeof(uint16_t) + RETRANSMISSION_START_050_PAYLOAD_SIZE;

void RetransmissionStart050::serialize(std::vector<unsigned char>& buffer) const {
    RetransmissionMsgHeader mutable_header = header;
    mutable_header.msg_type = MESSAGE_TYPE;
    mutable_header.msg_size = RETRANSMISSION_START_050_MSG_SIZE_FIELD_VALUE;

    size_t initial_buffer_size = buffer.size();
    mutable_header.serialize(buffer);

    // No payload

    RetransmissionMsgFooter mutable_footer = footer;
    mutable_footer.check_sum = calculate_retransmission_checksum(buffer.data() + initial_buffer_size, sizeof(uint16_t) + mutable_header.msg_size);
    mutable_footer.serialize(buffer, nullptr, 0);
}

bool RetransmissionStart050::deserialize(const unsigned char* data, size_t len) {
    size_t offset = 0;
    if (!header.deserialize(data, offset, len)) return false;
    if (header.msg_type != MESSAGE_TYPE) { LOG_ERROR << "RetransmissionStart050: Type mismatch."; return false; }
    if (header.msg_size != RETRANSMISSION_START_050_MSG_SIZE_FIELD_VALUE) { LOG_ERROR << "RetransmissionStart050: Size mismatch."; return false; }

    size_t expected_full_msg_len = sizeof(uint16_t) + header.msg_size + RetransmissionMsgFooter::SIZE;
    if (len < expected_full_msg_len) { LOG_ERROR << "RetransmissionStart050: Overall length too short."; return false; }

    // No payload to deserialize

    if (!footer.deserialize(data, offset, len, data, sizeof(uint16_t) + header.msg_size)) return false;

    return offset == expected_full_msg_len;
}

// --- ErrorNotification010 ---
// MsgSize for 010 = MsgType(2) + MsgSeqNum(4) + MsgTime(8) + StatusCode(1) = 14 + 1 = 15
const uint16_t ERROR_NOTIFICATION_010_PAYLOAD_SIZE = sizeof(uint8_t); // StatusCode
const uint16_t ERROR_NOTIFICATION_010_MSG_SIZE_FIELD_VALUE = RetransmissionMsgHeader::SIZE - sizeof(uint16_t) + ERROR_NOTIFICATION_010_PAYLOAD_SIZE;

void ErrorNotification010::serialize(std::vector<unsigned char>& buffer) const {
    RetransmissionMsgHeader mutable_header = header;
    mutable_header.msg_type = MESSAGE_TYPE;
    mutable_header.msg_size = ERROR_NOTIFICATION_010_MSG_SIZE_FIELD_VALUE;

    size_t initial_buffer_size = buffer.size();
    mutable_header.serialize(buffer);

    buffer.push_back(status_code); // StatusCode is uint8_t, no endian issue

    RetransmissionMsgFooter mutable_footer = footer;
    mutable_footer.check_sum = calculate_retransmission_checksum(buffer.data() + initial_buffer_size, sizeof(uint16_t) + mutable_header.msg_size);
    mutable_footer.serialize(buffer, nullptr, 0);
}

bool ErrorNotification010::deserialize(const unsigned char* data, size_t len) {
    size_t offset = 0;
    if (!header.deserialize(data, offset, len)) return false;
    if (header.msg_type != MESSAGE_TYPE) { LOG_ERROR << "ErrorNotification010: Type mismatch."; return false; }
    if (header.msg_size != ERROR_NOTIFICATION_010_MSG_SIZE_FIELD_VALUE) { LOG_ERROR << "ErrorNotification010: Size mismatch."; return false; }

    size_t expected_full_msg_len = sizeof(uint16_t) + header.msg_size + RetransmissionMsgFooter::SIZE;
    if (len < expected_full_msg_len) { LOG_ERROR << "ErrorNotification010: Overall length too short."; return false; }

    if (offset + sizeof(status_code) > len) {
        LOG_ERROR << "ErrorNotification010: Not enough data for status_code.";
        return false;
    }
    status_code = data[offset++];

    if (!footer.deserialize(data, offset, len, data, sizeof(uint16_t) + header.msg_size)) return false;

    return offset == expected_full_msg_len;
}

} // namespace TaifexRetransmission


namespace TaifexRetransmission {

// --- HeartbeatServer104 ---
// MsgSize for 104 = MsgType(2) + MsgSeqNum(4) + MsgTime(8) = 14 (empty payload)
const uint16_t HEARTBEAT_SERVER_104_PAYLOAD_SIZE = 0;
const uint16_t HEARTBEAT_SERVER_104_MSG_SIZE_FIELD_VALUE = RetransmissionMsgHeader::SIZE - sizeof(uint16_t) + HEARTBEAT_SERVER_104_PAYLOAD_SIZE;

void HeartbeatServer104::serialize(std::vector<unsigned char>& buffer) const {
    RetransmissionMsgHeader mutable_header = header;
    mutable_header.msg_type = MESSAGE_TYPE;
    mutable_header.msg_size = HEARTBEAT_SERVER_104_MSG_SIZE_FIELD_VALUE;

    size_t initial_buffer_size = buffer.size();
    mutable_header.serialize(buffer);

    RetransmissionMsgFooter mutable_footer = footer;
    mutable_footer.check_sum = calculate_retransmission_checksum(buffer.data() + initial_buffer_size, sizeof(uint16_t) + mutable_header.msg_size);
    mutable_footer.serialize(buffer, nullptr, 0);
}

bool HeartbeatServer104::deserialize(const unsigned char* data, size_t len) {
    size_t offset = 0;
    if (!header.deserialize(data, offset, len)) return false;
    if (header.msg_type != MESSAGE_TYPE) { LOG_ERROR << "HeartbeatServer104: Type mismatch."; return false; }
    if (header.msg_size != HEARTBEAT_SERVER_104_MSG_SIZE_FIELD_VALUE) { LOG_ERROR << "HeartbeatServer104: Size mismatch."; return false; }

    size_t expected_full_msg_len = sizeof(uint16_t) + header.msg_size + RetransmissionMsgFooter::SIZE;
    if (len < expected_full_msg_len) { LOG_ERROR << "HeartbeatServer104: Overall length too short."; return false; }

    if (!footer.deserialize(data, offset, len, data, sizeof(uint16_t) + header.msg_size)) return false;

    return offset == expected_full_msg_len;
}

// --- HeartbeatClient105 ---
// MsgSize for 105 = MsgType(2) + MsgSeqNum(4) + MsgTime(8) = 14 (empty payload)
const uint16_t HEARTBEAT_CLIENT_105_PAYLOAD_SIZE = 0;
const uint16_t HEARTBEAT_CLIENT_105_MSG_SIZE_FIELD_VALUE = RetransmissionMsgHeader::SIZE - sizeof(uint16_t) + HEARTBEAT_CLIENT_105_PAYLOAD_SIZE;

void HeartbeatClient105::serialize(std::vector<unsigned char>& buffer) const {
    RetransmissionMsgHeader mutable_header = header;
    mutable_header.msg_type = MESSAGE_TYPE;
    mutable_header.msg_size = HEARTBEAT_CLIENT_105_MSG_SIZE_FIELD_VALUE;

    size_t initial_buffer_size = buffer.size();
    mutable_header.serialize(buffer);

    RetransmissionMsgFooter mutable_footer = footer;
    mutable_footer.check_sum = calculate_retransmission_checksum(buffer.data() + initial_buffer_size, sizeof(uint16_t) + mutable_header.msg_size);
    mutable_footer.serialize(buffer, nullptr, 0);
}

bool HeartbeatClient105::deserialize(const unsigned char* data, size_t len) {
    size_t offset = 0;
    if (!header.deserialize(data, offset, len)) return false;
    if (header.msg_type != MESSAGE_TYPE) { LOG_ERROR << "HeartbeatClient105: Type mismatch."; return false; }
    if (header.msg_size != HEARTBEAT_CLIENT_105_MSG_SIZE_FIELD_VALUE) { LOG_ERROR << "HeartbeatClient105: Size mismatch."; return false; }

    size_t expected_full_msg_len = sizeof(uint16_t) + header.msg_size + RetransmissionMsgFooter::SIZE;
    if (len < expected_full_msg_len) { LOG_ERROR << "HeartbeatClient105: Overall length too short."; return false; }

    if (!footer.deserialize(data, offset, len, data, sizeof(uint16_t) + header.msg_size)) return false;

    return offset == expected_full_msg_len;
}

// --- DataRequest101 ---
// MsgSize for 101 = (MsgType(2) + MsgSeqNum(4) + MsgTime(8)) + (ChannelID(2) + BeginSeqNo(4) + RecoverNum(2)) = 14 + 8 = 22
const uint16_t DATA_REQUEST_101_PAYLOAD_SIZE = sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint16_t); // ChannelID + BeginSeqNo + RecoverNum
const uint16_t DATA_REQUEST_101_MSG_SIZE_FIELD_VALUE = RetransmissionMsgHeader::SIZE - sizeof(uint16_t) + DATA_REQUEST_101_PAYLOAD_SIZE;

void DataRequest101::serialize(std::vector<unsigned char>& buffer) const {
    RetransmissionMsgHeader mutable_header = header;
    mutable_header.msg_type = MESSAGE_TYPE;
    mutable_header.msg_size = DATA_REQUEST_101_MSG_SIZE_FIELD_VALUE;

    size_t initial_buffer_size = buffer.size();
    mutable_header.serialize(buffer);

    uint16_t net_channel_id = NetworkingUtils::host_to_network_short(channel_id);
    const unsigned char* ptr_ch = reinterpret_cast<const unsigned char*>(&net_channel_id);
    buffer.insert(buffer.end(), ptr_ch, ptr_ch + sizeof(net_channel_id));

    uint32_t net_begin_seq_no = NetworkingUtils::host_to_network_long(begin_seq_no);
    const unsigned char* ptr_bs = reinterpret_cast<const unsigned char*>(&net_begin_seq_no);
    buffer.insert(buffer.end(), ptr_bs, ptr_bs + sizeof(net_begin_seq_no));

    uint16_t net_recover_num = NetworkingUtils::host_to_network_short(recover_num);
    const unsigned char* ptr_rn = reinterpret_cast<const unsigned char*>(&net_recover_num);
    buffer.insert(buffer.end(), ptr_rn, ptr_rn + sizeof(net_recover_num));

    RetransmissionMsgFooter mutable_footer = footer;
    mutable_footer.check_sum = calculate_retransmission_checksum(buffer.data() + initial_buffer_size, sizeof(uint16_t) + mutable_header.msg_size);
    mutable_footer.serialize(buffer, nullptr, 0);
}

bool DataRequest101::deserialize(const unsigned char* data, size_t len) {
    size_t offset = 0;
    if (!header.deserialize(data, offset, len)) return false;
    if (header.msg_type != MESSAGE_TYPE) { LOG_ERROR << "DataRequest101: Type mismatch."; return false; }
    if (header.msg_size != DATA_REQUEST_101_MSG_SIZE_FIELD_VALUE) { LOG_ERROR << "DataRequest101: Size mismatch."; return false; }

    size_t expected_full_msg_len = sizeof(uint16_t) + header.msg_size + RetransmissionMsgFooter::SIZE;
    if (len < expected_full_msg_len) { LOG_ERROR << "DataRequest101: Overall length too short."; return false; }

    if (offset + sizeof(channel_id) > len) return false;
    memcpy(&channel_id, data + offset, sizeof(channel_id));
    offset += sizeof(channel_id);
    channel_id = NetworkingUtils::network_to_host_short(channel_id);

    if (offset + sizeof(begin_seq_no) > len) return false;
    memcpy(&begin_seq_no, data + offset, sizeof(begin_seq_no));
    offset += sizeof(begin_seq_no);
    begin_seq_no = NetworkingUtils::network_to_host_long(begin_seq_no);

    if (offset + sizeof(recover_num) > len) return false;
    memcpy(&recover_num, data + offset, sizeof(recover_num));
    offset += sizeof(recover_num);
    recover_num = NetworkingUtils::network_to_host_short(recover_num);

    if (!footer.deserialize(data, offset, len, data, sizeof(uint16_t) + header.msg_size)) return false;

    return offset == expected_full_msg_len;
}

// --- DataResponse102 ---
// Fixed payload part: ChannelID(2) + StatusCode(1) + BeginSeqNo(4) + RecoverNum(2) = 9 bytes
const uint16_t DATA_RESPONSE_102_FIXED_PAYLOAD_SIZE = sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint16_t);
// The header.msg_size will be (HeaderPartForMsgSize(14) + FixedPayloadPart(9) + VariableDataPartLength)
// So, header.msg_size = (RetransmissionMsgHeader::SIZE - sizeof(uint16_t)) + DATA_RESPONSE_102_FIXED_PAYLOAD_SIZE + retrans_data.size()

void DataResponse102::serialize(std::vector<unsigned char>& buffer, const std::vector<unsigned char>& retrans_data) const {
    RetransmissionMsgHeader mutable_header = header; // Copy
    mutable_header.msg_type = MESSAGE_TYPE;
    // Calculate actual msg_size for this message instance
    mutable_header.msg_size = (RetransmissionMsgHeader::SIZE - sizeof(uint16_t)) + DATA_RESPONSE_102_FIXED_PAYLOAD_SIZE + retrans_data.size();

    size_t initial_buffer_size = buffer.size();
    mutable_header.serialize(buffer);

    // Fixed Payload
    uint16_t net_channel_id = NetworkingUtils::host_to_network_short(channel_id);
    const unsigned char* ptr_ch = reinterpret_cast<const unsigned char*>(&net_channel_id);
    buffer.insert(buffer.end(), ptr_ch, ptr_ch + sizeof(net_channel_id));

    buffer.push_back(status_code);

    uint32_t net_begin_seq_no = NetworkingUtils::host_to_network_long(begin_seq_no);
    const unsigned char* ptr_bs = reinterpret_cast<const unsigned char*>(&net_begin_seq_no);
    buffer.insert(buffer.end(), ptr_bs, ptr_bs + sizeof(net_begin_seq_no));

    uint16_t net_recover_num = NetworkingUtils::host_to_network_short(recover_num);
    const unsigned char* ptr_rn = reinterpret_cast<const unsigned char*>(&net_recover_num);
    buffer.insert(buffer.end(), ptr_rn, ptr_rn + sizeof(net_recover_num));

    // Variable Payload (retransmitted_data)
    buffer.insert(buffer.end(), retrans_data.begin(), retrans_data.end());

    RetransmissionMsgFooter mutable_footer = footer; // Copy
    // Checksum is from MsgSize field (start of header part in buffer) up to end of variable payload
    // Length for checksum = sizeof(uint16_t for msg_size field) + value of msg_size field in header
    mutable_footer.check_sum = calculate_retransmission_checksum(buffer.data() + initial_buffer_size, sizeof(uint16_t) + mutable_header.msg_size);
    mutable_footer.serialize(buffer, nullptr, 0);
}

bool DataResponse102::deserialize(const unsigned char* data, size_t len, std::vector<unsigned char>& out_retrans_data) {
    out_retrans_data.clear();
    size_t offset = 0;
    if (!header.deserialize(data, offset, len)) {
        LOG_ERROR << "DataResponse102: Header parsing failed.";
        return false;
    }
    if (header.msg_type != MESSAGE_TYPE) {
        LOG_ERROR << "DataResponse102: Type mismatch.";
        return false;
    }

    // header.msg_size = (HeaderPartForMsgSize(14) + FixedPayloadPart(9) + VariableDataPartLength)
    uint16_t base_msg_size_content = (RetransmissionMsgHeader::SIZE - sizeof(uint16_t)) + DATA_RESPONSE_102_FIXED_PAYLOAD_SIZE;
    if (header.msg_size < base_msg_size_content) {
        LOG_ERROR << "DataResponse102: msg_size in header too small for fixed part. Expected at least " << base_msg_size_content << ", Got: " << header.msg_size;
        return false;
    }

    size_t variable_data_len = header.msg_size - base_msg_size_content;
    size_t expected_full_msg_len = sizeof(uint16_t) + header.msg_size + RetransmissionMsgFooter::SIZE;

    if (len < expected_full_msg_len) {
        LOG_ERROR << "DataResponse102: Overall length too short for declared msg_size. Expected: " << expected_full_msg_len << ", Got: " << len;
        return false;
    }

    // Fixed Payload
    if (offset + DATA_RESPONSE_102_FIXED_PAYLOAD_SIZE > len) { // Check if enough data for fixed payload
        LOG_ERROR << "DataResponse102: Not enough data for fixed payload.";
        return false;
    }
    memcpy(&channel_id, data + offset, sizeof(channel_id));
    offset += sizeof(channel_id);
    channel_id = NetworkingUtils::network_to_host_short(channel_id);

    status_code = data[offset++];

    memcpy(&begin_seq_no, data + offset, sizeof(begin_seq_no));
    offset += sizeof(begin_seq_no);
    begin_seq_no = NetworkingUtils::network_to_host_long(begin_seq_no);

    memcpy(&recover_num, data + offset, sizeof(recover_num));
    offset += sizeof(recover_num);
    recover_num = NetworkingUtils::network_to_host_short(recover_num);

    // Variable Payload (retransmitted_data)
    if (offset + variable_data_len > len) {
        LOG_ERROR << "DataResponse102: Not enough data for variable payload part (retransmitted_data).";
        return false;
    }
    if (variable_data_len > 0) {
        out_retrans_data.assign(data + offset, data + offset + variable_data_len);
    }
    offset += variable_data_len;

    // Footer (Checksum validation)
    if (!footer.deserialize(data, offset, len, data, sizeof(uint16_t) + header.msg_size)) {
        LOG_ERROR << "DataResponse102: Footer checksum validation failed.";
        return false;
    }

    return offset == expected_full_msg_len;
}


} // namespace TaifexRetransmission
