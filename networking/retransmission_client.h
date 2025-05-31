#ifndef RETRANSMISSION_CLIENT_H
#define RETRANSMISSION_CLIENT_H

#include "retransmission_protocol.h"
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <cstdint>
#include <memory>

namespace Networking {

/**
 * @brief Client for handling TAIFEX retransmission service communication.
 *
 * This class manages a TCP connection to a TAIFEX Retransmission Server.
 * It handles:
 * - Connecting to the server.
 * - Performing login (MsgType 020).
 * - Sending data requests (MsgType 101).
 * - Receiving and processing responses (e.g., 030, 050, 102, 104, 010).
 * - Receiving retransmitted market data messages (standard TAIFEX format starting with 0x1B).
 * - Sending client heartbeats (MsgType 105) in response to server heartbeats (MsgType 104).
 * - Managing message sequencing and providing data/status through callbacks.
 *
 * It operates its own network thread for receiving and processing messages.
 */
class RetransmissionClient {
public:
    /**
     * @brief Callback for received retransmitted market data messages.
     * These are raw TAIFEX messages (header, body, checksum, term_code).
     */
    using MarketDataCallback = std::function<void(const unsigned char* data, size_t length)>;
    /**
     * @brief Callback for DataResponse (MsgType 102) messages.
     * Provides the parsed DataResponse102 struct and any associated variable retransmitted data.
     */
    using StatusCallback = std::function<void(const TaifexRetransmission::DataResponse102& response, const std::vector<unsigned char>& retrans_data)>;
    /**
     * @brief Callback for ErrorNotification (MsgType 010) messages.
     * Provides the parsed ErrorNotification010 struct.
     */
    using ErrorCallback = std::function<void(const TaifexRetransmission::ErrorNotification010& error_msg)>;
    /** @brief Callback invoked when the client disconnects from the server. */
    using DisconnectedCallback = std::function<void()>;
    /** @brief Callback invoked upon successful login (after receiving RetransmissionStart050). */
    using LoggedInCallback = std::function<void()>;

    /**
     * @brief Constructs a RetransmissionClient.
     * @param server_ip IP address of the retransmission server.
     * @param server_port Port number of the retransmission server.
     * @param session_id Session ID for login.
     * @param password Password for login (used to calculate CheckCode).
     * @param market_cb Callback for retransmitted market data.
     * @param status_cb Callback for DataResponse102 status messages.
     * @param error_cb Callback for ErrorNotification010 messages.
     * @param disconnected_cb Callback for disconnect events.
     * @param logged_in_cb Callback for successful login confirmation.
     */
    RetransmissionClient(std::string server_ip, uint16_t server_port,
                         uint16_t session_id, std::string password,
                         MarketDataCallback market_cb,
                         StatusCallback status_cb,
                         ErrorCallback error_cb,
                         DisconnectedCallback disconnected_cb,
                         LoggedInCallback logged_in_cb);
    /**
     * @brief Destructor. Stops the client and cleans up resources.
     */
    ~RetransmissionClient();

    // Disable copy and assignment.
    RetransmissionClient(const RetransmissionClient&) = delete;
    RetransmissionClient& operator=(const RetransmissionClient&) = delete;

    /**
     * @brief Starts the client's operation.
     * This initiates a connection attempt to the server in a separate thread.
     * If connection is successful, it attempts to log in.
     * The client thread will then listen for incoming messages.
     * @return True if the client thread was successfully started, false otherwise.
     *         Note: This does not guarantee immediate connection or login success.
     */
    bool start();

    /**
     * @brief Stops the client's operation.
     * Signals the client thread to terminate, disconnects from the server, and joins the thread.
     */
    void stop();

    /**
     * @brief Sends a data retransmission request (MsgType 101) to the server.
     * Can only be called after a successful login.
     * @param channel_id The Channel ID for which data is requested.
     * @param begin_seq_no The starting sequence number (CHANNEL-SEQ) of messages to recover.
     * @param count The number of messages to recover. If <=1, recovers only the message with `begin_seq_no`.
     * @return True if the request was sent successfully, false if not logged in or send failed.
     */
    bool request_retransmission(uint16_t channel_id, uint32_t begin_seq_no, uint16_t count);

    /**
     * @brief Sends a client heartbeat message (MsgType 105).
     * This is typically called in response to receiving a HeartbeatServer104 message.
     * @return True if the heartbeat was sent successfully, false if not connected or send failed.
     */
    bool send_client_heartbeat();

private:
    bool connect_to_server();
    bool perform_login();
    void send_tcp_message(const std::vector<unsigned char>& message_bytes);
    void receive_loop();
    void process_incoming_data(const unsigned char* data_chunk, size_t length);
    // Changed handle_complete_message to take full message data for specific deserializers
    void handle_protocol_message(const TaifexRetransmission::RetransmissionMsgHeader& header,
                                 const unsigned char* full_message_data, size_t full_message_length);

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
    std::atomic<bool> logged_in_;
    uint32_t client_msg_seq_num_;

    std::unique_ptr<std::thread> client_thread_;
    std::vector<unsigned char> tcp_receive_buffer_;
};

} // namespace Networking
#endif // RETRANSMISSION_CLIENT_H
