#ifndef RETRANSMISSION_CLIENT_H
#define RETRANSMISSION_CLIENT_H

#include "retransmission_protocol.h" // For message structs
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <cstdint>
#include <memory> // For std::unique_ptr for thread

namespace Networking {

// Forward declare TaifexSdk if needed for callback signatures, but better to use generic callbacks
// namespace Taifex { class TaifexSdk; }

class RetransmissionClient {
public:
    using MarketDataCallback = std::function<void(const unsigned char* data, size_t length)>;
    // Callback for 102 response, providing context of the original request
    using StatusCallback = std::function<void(const TaifexRetransmission::DataResponse102& response, const std::vector<unsigned char>& retrans_data)>;
    using ErrorCallback = std::function<void(const TaifexRetransmission::ErrorNotification010& error_msg)>;
    using DisconnectedCallback = std::function<void()>;
    using LoggedInCallback = std::function<void()>;


    RetransmissionClient(std::string server_ip, uint16_t server_port,
                         uint16_t session_id, std::string password, // Password for CheckCode
                         MarketDataCallback market_cb,
                         StatusCallback status_cb,
                         ErrorCallback error_cb,
                         DisconnectedCallback disconnected_cb,
                         LoggedInCallback logged_in_cb);
    ~RetransmissionClient();

    RetransmissionClient(const RetransmissionClient&) = delete;
    RetransmissionClient& operator=(const RetransmissionClient&) = delete;

    bool start(); // Connects, logs in, starts listening thread
    void stop();  // Disconnects, stops thread

    bool request_retransmission(uint16_t channel_id, uint32_t begin_seq_no, uint16_t count);

    // Method to send HeartbeatClient105 in response to HeartbeatServer104
    bool send_client_heartbeat();

private:
    bool connect_to_server();
    bool perform_login();
    // send_tcp_message should take const ref to avoid copying vector
    void send_tcp_message(const std::vector<unsigned char>& message_bytes);
    void receive_loop();
    void process_incoming_data(const unsigned char* data_chunk, size_t length); // Process chunk of data
    void handle_complete_message(const unsigned char* message_data, size_t message_length); // Handle one full retrans message

    std::string server_ip_;
    uint16_t server_port_;
    uint16_t session_id_;
    std::string password_;

    MarketDataCallback market_data_callback_;
    StatusCallback status_callback_;
    ErrorCallback error_callback_;
    DisconnectedCallback disconnected_callback_;
    LoggedInCallback logged_in_callback_;

    int socket_fd_;
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    std::atomic<bool> logged_in_; // To track login state
    uint32_t client_msg_seq_num_;

    std::unique_ptr<std::thread> client_thread_; // Use unique_ptr for better RAII
    std::vector<unsigned char> tcp_receive_buffer_;
};

} // namespace Networking
#endif // RETRANSMISSION_CLIENT_H
