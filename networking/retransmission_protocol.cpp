#include "networking/retransmission_protocol.h"
#include "networking/endian_utils.h"
#include "core_utils/logger.h"

#include <vector>
#include <numeric>
#include <cstring>
#include <stdexcept>
#include <span>
#include <cstddef>


namespace TaifexRetransmission {

// --- Helper Function ---
uint8_t calculate_retransmission_checksum(std::span<const std::byte> data_segment) {
    uint32_t sum = 0;
    for (std::byte b : data_segment) {
        sum += static_cast<unsigned char>(b);
    }
    return static_cast<uint8_t>(sum % 256);
}

// --- StandardTimeFormat ---
void StandardTimeFormat::serialize(std::vector<unsigned char>& buffer) const {
    uint32_t net_epoch_s = NetworkingUtils::host_to_network_long(epoch_s);
    uint32_t net_nanosecond = NetworkingUtils::host_to_network_long(nanosecond);

    size_t original_size = buffer.size();
    buffer.resize(original_size + SIZE);

    memcpy(buffer.data() + original_size, &net_epoch_s, sizeof(net_epoch_s));
    memcpy(buffer.data() + original_size + sizeof(net_epoch_s), &net_nanosecond, sizeof(net_nanosecond));
}

bool StandardTimeFormat::deserialize(const unsigned char* data, size_t& offset, size_t total_len) {
    if (offset + SIZE > total_len) {
        LOG_ERROR << "StandardTimeFormat deserialize: Not enough data. Required: " << SIZE << ", Available: " << (total_len - offset);
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
    buffer.resize(original_size + (SIZE - StandardTimeFormat::SIZE));

    size_t current_offset = original_size;
    memcpy(buffer.data() + current_offset, &net_msg_size, sizeof(net_msg_size));
    current_offset += sizeof(net_msg_size);
    memcpy(buffer.data() + current_offset, &net_msg_type, sizeof(net_msg_type));
    current_offset += sizeof(net_msg_type);
    memcpy(buffer.data() + current_offset, &net_msg_seq_num, sizeof(net_msg_seq_num));

    msg_time.serialize(buffer);
}

bool RetransmissionMsgHeader::deserialize(const unsigned char* data, size_t& offset, size_t total_len) {
    if (offset + SIZE > total_len) {
        LOG_ERROR << "RetransmissionMsgHeader deserialize: Not enough data for header. Required: " << SIZE << ", Available: " << (total_len - offset);
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
        return false;
    }
    return true;
}


// --- RetransmissionMsgFooter ---
void RetransmissionMsgFooter::serialize(std::vector<unsigned char>& buffer, const unsigned char* /*msg_start_for_checksum*/, size_t /*len_for_checksum*/) const {
    buffer.push_back(check_sum);
}

bool RetransmissionMsgFooter::deserialize(const unsigned char* data, size_t& offset, size_t total_len,
                                          const unsigned char* msg_start_for_checksum, size_t len_for_checksum) {
    if (offset + SIZE > total_len) {
        LOG_ERROR << "RetransmissionMsgFooter deserialize: Not enough data for footer. Required: " << SIZE << ", Available: " << (total_len - offset);
        return false;
    }

    uint8_t received_checksum = data[offset];
    offset += SIZE;

    if (!msg_start_for_checksum || len_for_checksum == 0) {
         LOG_ERROR << "RetransmissionMsgFooter deserialize: Invalid parameters for checksum calculation.";
        return false;
    }
    std::span<const unsigned char> checksum_data_uchars(msg_start_for_checksum, len_for_checksum);
    uint8_t calculated = calculate_retransmission_checksum(std::as_bytes(checksum_data_uchars));
    if (calculated != received_checksum) {
        LOG_ERROR << "Retransmission checksum mismatch. Calculated: " << static_cast<int>(calculated) << ", Received: " << static_cast<int>(received_checksum);
        return false;
    }
    this->check_sum = received_checksum;
    return true;
}

// --- LoginRequest020 ---
const uint16_t LOGIN_REQUEST_020_PAYLOAD_SIZE = sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint16_t);
const uint16_t LOGIN_REQUEST_020_MSG_SIZE_FIELD_VALUE = (RetransmissionMsgHeader::SIZE - sizeof(uint16_t)) + LOGIN_REQUEST_020_PAYLOAD_SIZE;

uint8_t calculate_check_code(uint16_t mult_op, const std::string& password_str) {
    if (password_str.empty()) {
        LOG_ERROR << "Password cannot be empty for CheckCode calculation.";
        return 0;
    }
    long long password_val = 0;
    try {
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
    RetransmissionMsgHeader header_to_serialize = this->header;
    header_to_serialize.msg_type = MESSAGE_TYPE;
    header_to_serialize.msg_size = LOGIN_REQUEST_020_MSG_SIZE_FIELD_VALUE;

    size_t initial_buffer_size = buffer.size();
    header_to_serialize.serialize(buffer);

    uint16_t net_mult_op = NetworkingUtils::host_to_network_short(multiplication_operator);
    buffer.insert(buffer.end(), reinterpret_cast<const unsigned char*>(&net_mult_op), reinterpret_cast<const unsigned char*>(&net_mult_op) + sizeof(net_mult_op));
    buffer.push_back(this->check_code);
    uint16_t net_session_id = NetworkingUtils::host_to_network_short(session_id);
    buffer.insert(buffer.end(), reinterpret_cast<const unsigned char*>(&net_session_id), reinterpret_cast<const unsigned char*>(&net_session_id) + sizeof(net_session_id));

    RetransmissionMsgFooter footer_to_serialize = this->footer;
    size_t checksum_data_len = sizeof(uint16_t) + LOGIN_REQUEST_020_MSG_SIZE_FIELD_VALUE;
    std::span<const unsigned char> checksum_data_uchars(buffer.data() + initial_buffer_size, checksum_data_len);
    footer_to_serialize.check_sum = calculate_retransmission_checksum(std::as_bytes(checksum_data_uchars));
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
    size_t expected_full_msg_len = sizeof(uint16_t) + header.msg_size + RetransmissionMsgFooter::SIZE;
    if (len < expected_full_msg_len) {
        LOG_ERROR << "LoginRequest020 deserialize: Data length too short. Expected: " << expected_full_msg_len << ", Got: " << len;
        return false;
    }
    if (offset + LOGIN_REQUEST_020_PAYLOAD_SIZE > len) {
         LOG_ERROR << "LoginRequest020 deserialize: Not enough data for payload. Required: " << LOGIN_REQUEST_020_PAYLOAD_SIZE << ", Available: " << (len - offset);
        return false;
    }
    memcpy(&multiplication_operator, data + offset, sizeof(multiplication_operator));
    offset += sizeof(multiplication_operator);
    multiplication_operator = NetworkingUtils::network_to_host_short(multiplication_operator);
    check_code = data[offset++];
    memcpy(&session_id, data + offset, sizeof(session_id));
    offset += sizeof(session_id);
    session_id = NetworkingUtils::network_to_host_short(session_id);
    size_t checksum_data_len = sizeof(uint16_t) + header.msg_size;
    if (!footer.deserialize(data, offset, len, data, checksum_data_len)) {
        LOG_ERROR << "LoginRequest020 deserialize: Footer checksum validation failed.";
        return false;
    }
    return offset == expected_full_msg_len;
}

// --- LoginResponse030 ---
const uint16_t LOGIN_RESPONSE_030_PAYLOAD_SIZE = sizeof(uint16_t);
const uint16_t LOGIN_RESPONSE_030_MSG_SIZE_FIELD_VALUE = (RetransmissionMsgHeader::SIZE - sizeof(uint16_t)) + LOGIN_RESPONSE_030_PAYLOAD_SIZE;

void LoginResponse030::serialize(std::vector<unsigned char>& buffer) const {
    RetransmissionMsgHeader mutable_header = header;
    mutable_header.msg_type = MESSAGE_TYPE;
    mutable_header.msg_size = LOGIN_RESPONSE_030_MSG_SIZE_FIELD_VALUE;
    size_t initial_buffer_size = buffer.size();
    mutable_header.serialize(buffer);
    uint16_t net_channel_id = NetworkingUtils::host_to_network_short(channel_id);
    buffer.insert(buffer.end(), reinterpret_cast<const unsigned char*>(&net_channel_id), reinterpret_cast<const unsigned char*>(&net_channel_id) + sizeof(net_channel_id));
    RetransmissionMsgFooter mutable_footer = footer;
    std::span<const unsigned char> checksum_data_uchars(buffer.data() + initial_buffer_size, sizeof(uint16_t) + mutable_header.msg_size);
    mutable_footer.check_sum = calculate_retransmission_checksum(std::as_bytes(checksum_data_uchars));
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
    if (offset + sizeof(channel_id) > len) {
        LOG_ERROR << "LoginResponse030: Not enough data for channel_id. Required: " << sizeof(channel_id) << ", Available: " << (len - offset);
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
const uint16_t RETRANSMISSION_START_050_PAYLOAD_SIZE = 0;
const uint16_t RETRANSMISSION_START_050_MSG_SIZE_FIELD_VALUE = RetransmissionMsgHeader::SIZE - sizeof(uint16_t) + RETRANSMISSION_START_050_PAYLOAD_SIZE;

void RetransmissionStart050::serialize(std::vector<unsigned char>& buffer) const {
    RetransmissionMsgHeader mutable_header = header;
    mutable_header.msg_type = MESSAGE_TYPE;
    mutable_header.msg_size = RETRANSMISSION_START_050_MSG_SIZE_FIELD_VALUE;
    size_t initial_buffer_size = buffer.size();
    mutable_header.serialize(buffer);
    RetransmissionMsgFooter mutable_footer = footer;
    std::span<const unsigned char> checksum_data_uchars(buffer.data() + initial_buffer_size, sizeof(uint16_t) + mutable_header.msg_size);
    mutable_footer.check_sum = calculate_retransmission_checksum(std::as_bytes(checksum_data_uchars));
    mutable_footer.serialize(buffer, nullptr, 0);
}

bool RetransmissionStart050::deserialize(const unsigned char* data, size_t len) {
    size_t offset = 0;
    if (!header.deserialize(data, offset, len)) return false;
    if (header.msg_type != MESSAGE_TYPE) { LOG_ERROR << "RetransmissionStart050: Type mismatch."; return false; }
    if (header.msg_size != RETRANSMISSION_START_050_MSG_SIZE_FIELD_VALUE) { LOG_ERROR << "RetransmissionStart050: Size mismatch."; return false; }
    size_t expected_full_msg_len = sizeof(uint16_t) + header.msg_size + RetransmissionMsgFooter::SIZE;
    if (len < expected_full_msg_len) { LOG_ERROR << "RetransmissionStart050: Overall length too short."; return false; }
    if (!footer.deserialize(data, offset, len, data, sizeof(uint16_t) + header.msg_size)) return false;
    return offset == expected_full_msg_len;
}

// --- ErrorNotification010 ---
const uint16_t ERROR_NOTIFICATION_010_PAYLOAD_SIZE = sizeof(uint8_t);
const uint16_t ERROR_NOTIFICATION_010_MSG_SIZE_FIELD_VALUE = RetransmissionMsgHeader::SIZE - sizeof(uint16_t) + ERROR_NOTIFICATION_010_PAYLOAD_SIZE;

void ErrorNotification010::serialize(std::vector<unsigned char>& buffer) const {
    RetransmissionMsgHeader mutable_header = header;
    mutable_header.msg_type = MESSAGE_TYPE;
    mutable_header.msg_size = ERROR_NOTIFICATION_010_MSG_SIZE_FIELD_VALUE;
    size_t initial_buffer_size = buffer.size();
    mutable_header.serialize(buffer);
    buffer.push_back(status_code);
    RetransmissionMsgFooter mutable_footer = footer;
    std::span<const unsigned char> checksum_data_uchars(buffer.data() + initial_buffer_size, sizeof(uint16_t) + mutable_header.msg_size);
    mutable_footer.check_sum = calculate_retransmission_checksum(std::as_bytes(checksum_data_uchars));
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

// --- HeartbeatServer104 ---
const uint16_t HEARTBEAT_SERVER_104_PAYLOAD_SIZE = 0;
const uint16_t HEARTBEAT_SERVER_104_MSG_SIZE_FIELD_VALUE = RetransmissionMsgHeader::SIZE - sizeof(uint16_t) + HEARTBEAT_SERVER_104_PAYLOAD_SIZE;

void HeartbeatServer104::serialize(std::vector<unsigned char>& buffer) const {
    RetransmissionMsgHeader mutable_header = header;
    mutable_header.msg_type = MESSAGE_TYPE;
    mutable_header.msg_size = HEARTBEAT_SERVER_104_MSG_SIZE_FIELD_VALUE;
    size_t initial_buffer_size = buffer.size();
    mutable_header.serialize(buffer);
    RetransmissionMsgFooter mutable_footer = footer;
    std::span<const unsigned char> checksum_data_uchars(buffer.data() + initial_buffer_size, sizeof(uint16_t) + mutable_header.msg_size);
    mutable_footer.check_sum = calculate_retransmission_checksum(std::as_bytes(checksum_data_uchars));
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
const uint16_t HEARTBEAT_CLIENT_105_PAYLOAD_SIZE = 0;
const uint16_t HEARTBEAT_CLIENT_105_MSG_SIZE_FIELD_VALUE = RetransmissionMsgHeader::SIZE - sizeof(uint16_t) + HEARTBEAT_CLIENT_105_PAYLOAD_SIZE;

void HeartbeatClient105::serialize(std::vector<unsigned char>& buffer) const {
    RetransmissionMsgHeader mutable_header = header;
    mutable_header.msg_type = MESSAGE_TYPE;
    mutable_header.msg_size = HEARTBEAT_CLIENT_105_MSG_SIZE_FIELD_VALUE;
    size_t initial_buffer_size = buffer.size();
    mutable_header.serialize(buffer);
    RetransmissionMsgFooter mutable_footer = footer;
    std::span<const unsigned char> checksum_data_uchars(buffer.data() + initial_buffer_size, sizeof(uint16_t) + mutable_header.msg_size);
    mutable_footer.check_sum = calculate_retransmission_checksum(std::as_bytes(checksum_data_uchars));
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
const uint16_t DATA_REQUEST_101_PAYLOAD_SIZE = sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint16_t);
const uint16_t DATA_REQUEST_101_MSG_SIZE_FIELD_VALUE = RetransmissionMsgHeader::SIZE - sizeof(uint16_t) + DATA_REQUEST_101_PAYLOAD_SIZE;

void DataRequest101::serialize(std::vector<unsigned char>& buffer) const {
    RetransmissionMsgHeader mutable_header = header;
    mutable_header.msg_type = MESSAGE_TYPE;
    mutable_header.msg_size = DATA_REQUEST_101_MSG_SIZE_FIELD_VALUE;
    size_t initial_buffer_size = buffer.size();
    mutable_header.serialize(buffer);
    uint16_t net_channel_id = NetworkingUtils::host_to_network_short(channel_id);
    buffer.insert(buffer.end(), reinterpret_cast<const unsigned char*>(&net_channel_id), reinterpret_cast<const unsigned char*>(&net_channel_id) + sizeof(net_channel_id));
    uint32_t net_begin_seq_no = NetworkingUtils::host_to_network_long(begin_seq_no);
    buffer.insert(buffer.end(), reinterpret_cast<const unsigned char*>(&net_begin_seq_no), reinterpret_cast<const unsigned char*>(&net_begin_seq_no) + sizeof(net_begin_seq_no));
    uint16_t net_recover_num = NetworkingUtils::host_to_network_short(recover_num);
    buffer.insert(buffer.end(), reinterpret_cast<const unsigned char*>(&net_recover_num), reinterpret_cast<const unsigned char*>(&net_recover_num) + sizeof(net_recover_num));
    RetransmissionMsgFooter mutable_footer = footer;
    std::span<const unsigned char> checksum_data_uchars(buffer.data() + initial_buffer_size, sizeof(uint16_t) + mutable_header.msg_size);
    mutable_footer.check_sum = calculate_retransmission_checksum(std::as_bytes(checksum_data_uchars));
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
const uint16_t DATA_RESPONSE_102_FIXED_PAYLOAD_SIZE = sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint16_t);

void DataResponse102::serialize(std::vector<unsigned char>& buffer, const std::vector<unsigned char>& retrans_data) const {
    RetransmissionMsgHeader mutable_header = header;
    mutable_header.msg_type = MESSAGE_TYPE;
    mutable_header.msg_size = (RetransmissionMsgHeader::SIZE - sizeof(uint16_t)) + DATA_RESPONSE_102_FIXED_PAYLOAD_SIZE + retrans_data.size();
    size_t initial_buffer_size = buffer.size();
    mutable_header.serialize(buffer);
    uint16_t net_channel_id = NetworkingUtils::host_to_network_short(channel_id);
    buffer.insert(buffer.end(), reinterpret_cast<const unsigned char*>(&net_channel_id), reinterpret_cast<const unsigned char*>(&net_channel_id) + sizeof(net_channel_id));
    buffer.push_back(status_code);
    uint32_t net_begin_seq_no = NetworkingUtils::host_to_network_long(begin_seq_no);
    buffer.insert(buffer.end(), reinterpret_cast<const unsigned char*>(&net_begin_seq_no), reinterpret_cast<const unsigned char*>(&net_begin_seq_no) + sizeof(net_begin_seq_no));
    uint16_t net_recover_num = NetworkingUtils::host_to_network_short(recover_num);
    buffer.insert(buffer.end(), reinterpret_cast<const unsigned char*>(&net_recover_num), reinterpret_cast<const unsigned char*>(&net_recover_num) + sizeof(net_recover_num));
    buffer.insert(buffer.end(), retrans_data.begin(), retrans_data.end());
    RetransmissionMsgFooter mutable_footer = footer;
    std::span<const unsigned char> checksum_data_uchars(buffer.data() + initial_buffer_size, sizeof(uint16_t) + mutable_header.msg_size);
    mutable_footer.check_sum = calculate_retransmission_checksum(std::as_bytes(checksum_data_uchars));
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
    uint16_t base_msg_size_content = (RetransmissionMsgHeader::SIZE - sizeof(uint16_t)) + DATA_RESPONSE_102_FIXED_PAYLOAD_SIZE;
    if (header.msg_size < base_msg_size_content) {
        LOG_ERROR << "DataResponse102: msg_size in header (" << header.msg_size << ") too small for fixed part (" << base_msg_size_content << ").";
        return false;
    }
    size_t variable_data_len = header.msg_size - base_msg_size_content;
    size_t expected_full_msg_len = sizeof(uint16_t) + header.msg_size + RetransmissionMsgFooter::SIZE;
    if (len < expected_full_msg_len) {
        LOG_ERROR << "DataResponse102: Overall length (" << len << ") too short for declared msg_size. Expected: " << expected_full_msg_len;
        return false;
    }
    if (offset + DATA_RESPONSE_102_FIXED_PAYLOAD_SIZE > len) {
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
    if (offset + variable_data_len > len) { // Check against overall length 'len'
        LOG_ERROR << "DataResponse102: Not enough data for variable payload part (retransmitted_data). Required: " << variable_data_len << ", Available: " << (len - offset) ;
        return false;
    }
    if (variable_data_len > 0) {
        out_retrans_data.assign(data + offset, data + offset + variable_data_len);
    }
    offset += variable_data_len;
    if (!footer.deserialize(data, offset, len, data, sizeof(uint16_t) + header.msg_size)) {
        LOG_ERROR << "DataResponse102: Footer checksum validation failed.";
        return false;
    }
    return offset == expected_full_msg_len;
}

} // namespace TaifexRetransmission
