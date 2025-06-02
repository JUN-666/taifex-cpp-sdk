#include "networking/retransmission_client.h"
#include "networking/endian_utils.h"
#include "logger.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> // For close(), read(), write(), usleep()
#include <cstring>  // For memset, strerror, memcpy
#include <cerrno>   // For errno
#include <chrono>   // For sleep, system_clock

namespace Networking {

// Default connect and read/write timeouts (can be made configurable)
// const int DEFAULT_CONNECT_TIMEOUT_SEC = 5; // Connect timeout is harder to set portably for blocking connect
const int DEFAULT_RECV_TIMEOUT_SEC = 5; // For socket recv operations (reduced from 10 for more responsiveness)
// const int DEFAULT_HEARTBEAT_INTERVAL_SEC = 30; // Not used directly in client logic yet

RetransmissionClient::RetransmissionClient(
    std::string server_ip, uint16_t server_port,
    uint16_t session_id, std::string password,
    MarketDataCallback market_cb,
    StatusCallback status_cb,
    ErrorCallback error_cb,
    DisconnectedCallback disconnected_cb,
    LoggedInCallback logged_in_cb)
    : server_ip_(std::move(server_ip)),
      server_port_(server_port),
      session_id_(session_id),
      password_(std::move(password)),
      market_data_callback_(std::move(market_cb)),
      status_callback_(std::move(status_cb)),
      error_callback_(std::move(error_cb)),
      disconnected_callback_(std::move(disconnected_cb)),
      logged_in_callback_(std::move(logged_in_cb)),
      socket_fd_(-1),
      running_(false),
      connected_(false),
      logged_in_(false),
      client_msg_seq_num_(0) {
    LOG_INFO << "RetransmissionClient created for " << server_ip_ << ":" << server_port_;
}

RetransmissionClient::~RetransmissionClient() {
    LOG_INFO << "RetransmissionClient for " << server_ip_ << " shutting down...";
    stop();
}

bool RetransmissionClient::connect_to_server() {
    if (connected_.load() && socket_fd_ >= 0) {
        return true;
    }
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    connected_ = false;
    logged_in_ = false;

    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        LOG_ERROR << "RetransmissionClient: Failed to create TCP socket: " << strerror(errno);
        return false;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port_);
    if (inet_pton(AF_INET, server_ip_.c_str(), &serv_addr.sin_addr) <= 0) {
        LOG_ERROR << "RetransmissionClient: Invalid server IP address: " << server_ip_;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    LOG_INFO << "RetransmissionClient: Connecting to " << server_ip_ << ":" << server_port_ << "...";
    if (connect(socket_fd_, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        LOG_ERROR << "RetransmissionClient: Connection failed to " << server_ip_ << ":" << server_port_ << ": " << strerror(errno);
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    struct timeval tv;
    tv.tv_sec = DEFAULT_RECV_TIMEOUT_SEC;
    tv.tv_usec = 0;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
        LOG_WARNING << "RetransmissionClient: Failed to set SO_RCVTIMEO. Recv calls may block longer than expected.";
    }

    LOG_INFO << "RetransmissionClient: Connected successfully to " << server_ip_ << ":" << server_port_;
    connected_ = true;
    return true;
}

bool RetransmissionClient::perform_login() {
    if (!connected_.load()) {
        LOG_WARNING << "RetransmissionClient: Not connected, cannot perform login.";
        return false;
    }
    if (logged_in_.load()) {
         LOG_INFO << "RetransmissionClient: Already logged in.";
        return true;
    }

    client_msg_seq_num_ = 0;

    TaifexRetransmission::LoginRequest020 login_req;

    auto now = std::chrono::system_clock::now();
    auto epoch_s = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    auto ns_since_epoch = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    login_req.header.msg_time.epoch_s = static_cast<uint32_t>(epoch_s);
    login_req.header.msg_time.nanosecond = static_cast<uint32_t>(ns_since_epoch % 1000000000ULL);
    login_req.header.msg_seq_num = client_msg_seq_num_++;


    login_req.multiplication_operator = 168;
    login_req.session_id = session_id_;
    login_req.check_code = TaifexRetransmission::calculate_check_code(login_req.multiplication_operator, password_);

    std::vector<unsigned char> buffer;
    login_req.serialize(buffer, password_);

    LOG_INFO << "RetransmissionClient: Sending LoginRequest020 (MsgSeq: " << login_req.header.msg_seq_num << ")";
    send_tcp_message(buffer);

    return true;
}


void RetransmissionClient::send_tcp_message(const std::vector<unsigned char>& message_bytes) {
    if (!connected_.load() || socket_fd_ < 0) {
        LOG_ERROR << "RetransmissionClient: Not connected, cannot send message.";
        // error_callback_ might be too strong here, as it's for protocol errors. Disconnected is more appropriate.
        // If not connected, the receive_loop should handle invoking disconnected_callback_.
        return;
    }

    ssize_t total_sent = 0;
    ssize_t remaining = message_bytes.size();
    const unsigned char* data_ptr = message_bytes.data();

    while (total_sent < message_bytes.size()) {
        ssize_t sent_this_call = send(socket_fd_, data_ptr + total_sent, remaining, 0);
        if (sent_this_call < 0) {
            LOG_ERROR << "RetransmissionClient: send() error: " << strerror(errno);
            close(socket_fd_); // Close socket on send error
            socket_fd_ = -1;
            connected_ = false;
            logged_in_ = false;
            if (disconnected_callback_) disconnected_callback_();
            return;
        }
        if (sent_this_call == 0) { // Should not happen with blocking socket unless error
             LOG_ERROR << "RetransmissionClient: send() returned 0, treating as disconnect.";
            close(socket_fd_);
            socket_fd_ = -1;
            connected_ = false;
            logged_in_ = false;
            if (disconnected_callback_) disconnected_callback_();
            return;
        }
        total_sent += sent_this_call;
        remaining -= sent_this_call;
    }
    LOG_DEBUG << "RetransmissionClient: Sent " << total_sent << " bytes.";
}


void RetransmissionClient::receive_loop() {
    LOG_INFO << "RetransmissionClient: Receive loop started for " << server_ip_;
    const size_t RECV_BUFFER_SIZE = 8192;
    std::vector<unsigned char> temp_recv_buf(RECV_BUFFER_SIZE);

    while (running_.load()) {
        if (!connected_.load() || socket_fd_ < 0) {
            if (!running_.load()) break;
            LOG_INFO << "RetransmissionClient: Not connected. Attempting to reconnect...";
            std::this_thread::sleep_for(std::chrono::seconds(5));
            if (!connect_to_server()) {
                continue;
            }
            if (!perform_login()) {
                 LOG_WARNING << "RetransmissionClient: Re-login attempt initiated, awaiting response.";
            }
        }

        if (socket_fd_ < 0) {
             std::this_thread::sleep_for(std::chrono::milliseconds(100));
             continue;
        }

        ssize_t bytes_received = recv(socket_fd_, temp_recv_buf.data(), RECV_BUFFER_SIZE, 0);

        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout is fine, check running_ and continue
                if (!running_.load()) break;
                continue;
            }
            if (!running_.load() && (errno == EINTR || errno == EBADF)) {
                LOG_INFO << "RetransmissionClient: recv interrupted or socket closed, likely due to stop().";
                break;
            }
            LOG_ERROR << "RetransmissionClient: recv() error: " << strerror(errno);
            close(socket_fd_);
            socket_fd_ = -1;
            connected_ = false;
            logged_in_ = false;
            if (disconnected_callback_) disconnected_callback_();
            continue;
        }

        if (bytes_received == 0) {
            LOG_INFO << "RetransmissionClient: Server closed connection.";
            close(socket_fd_);
            socket_fd_ = -1;
            connected_ = false;
            logged_in_ = false;
            if (disconnected_callback_) disconnected_callback_();
            continue;
        }

        if (bytes_received > 0) {
            // Pass the raw chunk of data to process_incoming_data
            process_incoming_data(temp_recv_buf.data(), static_cast<size_t>(bytes_received));
        }
    }
    LOG_INFO << "RetransmissionClient: Receive loop ended for " << server_ip_;
}


bool RetransmissionClient::start() {
    if (running_.exchange(true)) { // Set to true and get previous value
        LOG_WARNING << "RetransmissionClient: Already running or start signal sent.";
        return true;
    }
    // Thread will handle connect and login sequence
    client_thread_ = std::make_unique<std::thread>(&RetransmissionClient::receive_loop, this);
    LOG_INFO << "RetransmissionClient: Started and attempting connection/login.";
    return true;
}

void RetransmissionClient::stop() {
    if (!running_.exchange(false)) { // Set to false and get previous value
        return; // Already stopped or stop signal sent
    }
    LOG_INFO << "RetransmissionClient: Stopping...";

    if (socket_fd_ >= 0) {
        // No need for shutdown() typically before close() for client sockets unless specific linger options are set.
        // Closing the socket will cause blocking operations like recv() in receive_loop to fail (e.g., with EBADF or return 0).
        close(socket_fd_);
        socket_fd_ = -1;
    }

    if (client_thread_ && client_thread_->joinable()) {
        client_thread_->join();
    }

    connected_ = false;
    logged_in_ = false;
    LOG_INFO << "RetransmissionClient: Stopped.";
}

#include "core_utils/common_header.h"     // For parsing retransmitted market data headers
#include "core_utils/message_identifier.h"// For identifying market data message types if needed


namespace Networking {

// ... (constructor, destructor, connect_to_server, perform_login, send_tcp_message, start, stop, receive_loop) ...

bool RetransmissionClient::request_retransmission(uint16_t channel_id, uint32_t begin_seq_no, uint16_t count) {
    if (!logged_in_.load()) {
        LOG_WARNING << "RetransmissionClient: Not logged in, cannot send DataRequest101.";
        // error_callback_ might be too generic here. This is a client-side state issue.
        // Consider just returning false or logging. For now, let's not call error_callback_.
        return false;
    }

    TaifexRetransmission::DataRequest101 req;
    req.header.msg_seq_num = client_msg_seq_num_++;
    // Set msg_time
    auto now = std::chrono::system_clock::now();
    auto epoch_s_count = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    auto ns_since_epoch_count = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    req.header.msg_time.epoch_s = static_cast<uint32_t>(epoch_s_count);
    req.header.msg_time.nanosecond = static_cast<uint32_t>(ns_since_epoch_count % 1000000000ULL);

    req.channel_id = channel_id;
    req.begin_seq_no = begin_seq_no;
    req.recover_num = count;

    std::vector<unsigned char> buffer;
    req.serialize(buffer);

    LOG_INFO << "RetransmissionClient: Sending DataRequest101 (ClientMsgSeq: " << req.header.msg_seq_num <<
                           ", Channel: " << channel_id << ", Begin: " << begin_seq_no << ", Count: " << count << ")";
    send_tcp_message(buffer);
    return true;
}

bool RetransmissionClient::send_client_heartbeat() { // Renamed for clarity in header
    if (!connected_.load()) {
        LOG_WARNING << "RetransmissionClient: Not connected, cannot send HeartbeatClient105.";
        return false;
    }

    TaifexRetransmission::HeartbeatClient105 hb_resp;
    hb_resp.header.msg_seq_num = client_msg_seq_num_++;
    // Set msg_time
    auto now = std::chrono::system_clock::now();
    auto epoch_s_count = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    auto ns_since_epoch_count = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    hb_resp.header.msg_time.epoch_s = static_cast<uint32_t>(epoch_s_count);
    hb_resp.header.msg_time.nanosecond = static_cast<uint32_t>(ns_since_epoch_count % 1000000000ULL);

    std::vector<unsigned char> buffer;
    hb_resp.serialize(buffer);

    LOG_INFO << "RetransmissionClient: Sending HeartbeatClient105 (ClientMsgSeq: " << hb_resp.header.msg_seq_num << ")";
    send_tcp_message(buffer);
    return true;
}

void RetransmissionClient::process_incoming_data(const unsigned char* data_chunk, size_t length) {
    tcp_receive_buffer_.insert(tcp_receive_buffer_.end(), data_chunk, data_chunk + length);

    while (running_.load() && !tcp_receive_buffer_.empty()) {
        const unsigned char* current_data = tcp_receive_buffer_.data();
        size_t current_size = tcp_receive_buffer_.size();

        if (current_data[0] == 0x1B) { // Potential Market Data Message (starts with ESC)
            if (current_size < CoreUtils::CommonHeader::HEADER_SIZE) {
                LOG_DEBUG << "RetransmissionClient: Buffer has 0x1B but not enough data for market data header. Size: " << current_size;
                break;
            }
            CoreUtils::CommonHeader market_header;
            // Try to parse. Note: CommonHeader::parse might expect full message length for some internal checks,
            // but for just getting body_length from header, current_size should be enough if > HEADER_SIZE.
            // Let's assume parse can work with at least HEADER_SIZE bytes to get body_length.
            if (CoreUtils::CommonHeader::parse(current_data, current_size, market_header)) {
                uint16_t market_body_len = market_header.getBodyLength();
                size_t full_market_msg_len = CoreUtils::CommonHeader::HEADER_SIZE + market_body_len + 1 + 2; // +checksum+term_code

                if (current_size >= full_market_msg_len) {
                    LOG_DEBUG << "RetransmissionClient: Received retransmitted market data message (len: " << full_market_msg_len << ")";
                    if (market_data_callback_) {
                        market_data_callback_(current_data, full_market_msg_len);
                    }
                    tcp_receive_buffer_.erase(tcp_receive_buffer_.begin(), tcp_receive_buffer_.begin() + full_market_msg_len);
                    continue;
                } else {
                    LOG_DEBUG << "RetransmissionClient: Buffer has market data header, but needs more data for full message. Have: " << current_size << ", Need: " << full_market_msg_len;
                    break;
                }
            } else {
                LOG_ERROR << "RetransmissionClient: Data starts with 0x1B but not a valid market data header. Buffer size: " << current_size << ". Clearing buffer to prevent loop.";
                tcp_receive_buffer_.clear();
                break;
            }
        } else { // Assume Retransmission Protocol Message
            // Need at least enough for the MsgSize field of the retransmission header to know message length.
            if (current_size < sizeof(uint16_t)) { // sizeof(RetransmissionMsgHeader::msg_size)
                 LOG_DEBUG << "RetransmissionClient: Buffer too small for even retrans MsgSize field. Size: " << current_size;
                break;
            }

            uint16_t msg_size_from_stream; // This is the value of the MsgSize field itself.
            memcpy(&msg_size_from_stream, current_data, sizeof(uint16_t));
            msg_size_from_stream = NetworkingUtils::network_to_host_short(msg_size_from_stream);

            // Full retransmission message length: sizeof(MsgSize field) + value_of_MsgSize_field + sizeof(Checksum field)
            size_t expected_retrans_msg_len = sizeof(uint16_t) + msg_size_from_stream + sizeof(uint8_t);

            if (current_size >= expected_retrans_msg_len) {
                // We have a complete retransmission protocol message.
                // Deserialize the header fully now.
                TaifexRetransmission::RetransmissionMsgHeader retrans_header;
                size_t temp_offset = 0;
                // Important: Pass expected_retrans_msg_len or current_size for deserialize's total_len check,
                // ensuring it doesn't read past what we know is a full message or buffer end.
                // Since we determined expected_retrans_msg_len is available, use that.
                if (!retrans_header.deserialize(current_data, temp_offset, expected_retrans_msg_len)) {
                     LOG_ERROR << "RetransmissionClient: Failed to deserialize retransmission message header from complete segment.";
                     tcp_receive_buffer_.erase(tcp_receive_buffer_.begin(), tcp_receive_buffer_.begin() + expected_retrans_msg_len); // Remove malformed part
                     continue; // Try next message if any
                }
                // Note: retrans_header.msg_size should match msg_size_from_stream. This is validated in deserialize.

                handle_protocol_message(retrans_header, current_data, expected_retrans_msg_len);

                tcp_receive_buffer_.erase(tcp_receive_buffer_.begin(), tcp_receive_buffer_.begin() + expected_retrans_msg_len);
                continue;
            } else {
                LOG_DEBUG << "RetransmissionClient: Buffer has start of retrans message, but needs more data. Have: " << current_size << ", Need: " << expected_retrans_msg_len;
                break;
            }
        }
    }
}

void RetransmissionClient::handle_protocol_message(
    const TaifexRetransmission::RetransmissionMsgHeader& temp_parsed_header,
    const unsigned char* full_message_data, size_t full_message_length) {

    LOG_DEBUG << "RetransmissionClient: Handling protocol message type: " << temp_parsed_header.msg_type;

    switch (temp_parsed_header.msg_type) {
        case TaifexRetransmission::LoginResponse030::MESSAGE_TYPE: {
            TaifexRetransmission::LoginResponse030 msg;
            if (msg.deserialize(full_message_data, full_message_length)) {
                LOG_INFO << "RetransmissionClient: Received LoginResponse030 for ChannelID: " << msg.channel_id;
            } else {
                LOG_ERROR << "RetransmissionClient: Failed to deserialize LoginResponse030.";
            }
            break;
        }
        case TaifexRetransmission::RetransmissionStart050::MESSAGE_TYPE: {
            TaifexRetransmission::RetransmissionStart050 msg;
            if (msg.deserialize(full_message_data, full_message_length)) {
                LOG_INFO << "RetransmissionClient: Received RetransmissionStart050. Login successful.";
                logged_in_ = true;
                if (logged_in_callback_) logged_in_callback_();
            } else {
                LOG_ERROR << "RetransmissionClient: Failed to deserialize RetransmissionStart050.";
            }
            break;
        }
        case TaifexRetransmission::DataResponse102::MESSAGE_TYPE: {
            TaifexRetransmission::DataResponse102 msg;
            std::vector<unsigned char> retransmitted_data_payload; // To store variable part
            // Pass password_ as empty string as it's not used by DataResponse102::deserialize
            if (msg.deserialize(full_message_data, full_message_length, retransmitted_data_payload)) {
                LOG_INFO << "RetransmissionClient: Received DataResponse102. Status: " << msg.status_code <<
                                       " for Channel: " << msg.channel_id << ", BeginSeq: " << msg.begin_seq_no <<
                                       ", Retransmitted data size: " << retransmitted_data_payload.size();
                if (status_callback_) status_callback_(msg, retransmitted_data_payload);
                // After DataResponse102 with status 0, server sends market data messages directly (0x1B type).
                // These will be handled by the market data path in process_incoming_data.
            } else {
                LOG_ERROR << "RetransmissionClient: Failed to deserialize DataResponse102.";
            }
            break;
        }
        case TaifexRetransmission::HeartbeatServer104::MESSAGE_TYPE: {
            TaifexRetransmission::HeartbeatServer104 msg;
             if (msg.deserialize(full_message_data, full_message_length)) {
                LOG_INFO << "RetransmissionClient: Received HeartbeatServer104. Sending response.";
                send_client_heartbeat();
            } else {
                LOG_ERROR << "RetransmissionClient: Failed to deserialize HeartbeatServer104.";
            }
            break;
        }
        case TaifexRetransmission::ErrorNotification010::MESSAGE_TYPE: {
            TaifexRetransmission::ErrorNotification010 msg;
            if (msg.deserialize(full_message_data, full_message_length)) {
                LOG_ERROR << "RetransmissionClient: Received ErrorNotification010. Status: " << msg.status_code;
                if (error_callback_) error_callback_(msg); // Changed to pass struct
                logged_in_ = false;
            } else {
                LOG_ERROR << "RetransmissionClient: Failed to deserialize ErrorNotification010.";
            }
            break;
        }
        default:
            LOG_WARNING << "RetransmissionClient: Received unknown retransmission protocol message type: " << temp_parsed_header.msg_type;
            break;
    }
}

} // namespace Networking
