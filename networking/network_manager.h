#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <chrono> // For dual feed timestamping
#include <optional> // For optional retransmission config

// Forward declare TaifexSdk to pass pointer
namespace Taifex {
    class TaifexSdk;
}

// Forward declare types from retransmission_protocol.h to avoid including the whole file here
namespace TaifexRetransmission {
    struct DataResponse102;
    struct ErrorNotification010;
}


namespace Networking {

// Forward declare internal classes
class MulticastReceiver;
class RetransmissionClient;
// MulticastGroupSubscription is defined in multicast_receiver.h, which might need to be included if used directly in NetworkManagerConfig.
// For now, let's keep it as a sub-struct within NetworkManagerConfig to avoid include here.


struct NetworkManagerConfig {
    struct MulticastFeedConfig {
        std::string group_ip;
        uint16_t port;
        std::string local_interface_ip;
        bool is_primary_for_dual_feed = false; // Default to false
    };
    std::vector<MulticastFeedConfig> multicast_feeds; // Can contain one (single feed) or two (dual feed)

    struct RetransmissionServerConfig {
        std::string ip;
        uint16_t port;
        uint16_t session_id;
        std::string password;
    };
    // Use std::optional for retransmission servers as they might not always be configured
    std::optional<RetransmissionServerConfig> primary_retrans_server;
    std::optional<RetransmissionServerConfig> backup_retrans_server; // Currently unused by RetransmissionClient logic directly, but good for config

    bool dual_feed_enabled = false;
    std::chrono::milliseconds dual_feed_reorder_window_ms{100}; // Example default
};


class NetworkManager {
public:
    explicit NetworkManager(Taifex::TaifexSdk* sdk_core_logic);
    ~NetworkManager();

    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    bool configure_and_start(const NetworkManagerConfig& config); // Combined configure and start
    void stop();

    // Called by TaifexSdk when a gap is detected and retransmission is needed
    void trigger_retransmission_request(uint16_t channel_id, uint32_t start_seq_num, uint16_t count);

private:
    // Callbacks from MulticastReceiver
    void on_primary_multicast_data(const unsigned char* data, size_t length,
                                   const std::string& source_group_ip, uint16_t source_group_port);
    void on_secondary_multicast_data(const unsigned char* data, size_t length,
                                     const std::string& source_group_ip, uint16_t source_group_port);

    // Callbacks from RetransmissionClient
    void on_retransmitted_market_data(const unsigned char* data, size_t length);
    void on_retransmission_status(const TaifexRetransmission::DataResponse102& response, const std::vector<unsigned char>& retrans_data);
    void on_retransmission_error(const TaifexRetransmission::ErrorNotification010& error_msg);
    void on_retransmission_disconnected();
    void on_retransmission_logged_in();


    void process_incoming_packet(const unsigned char* data, size_t length, bool is_from_primary_feed, bool is_retransmitted = false);
    void forward_to_sdk(const unsigned char* data, size_t length);

    void connect_retransmission_client(bool use_primary_server);


    Taifex::TaifexSdk* sdk_core_logic_; // Non-owning pointer
    NetworkManagerConfig config_;

    std::unique_ptr<MulticastReceiver> primary_multicast_receiver_;
    std::unique_ptr<MulticastReceiver> secondary_multicast_receiver_;
    std::unique_ptr<RetransmissionClient> retransmission_client_;

    // Dual feed state: Key: Channel ID + Channel Seq (combined or pair), Value: Timestamp of first arrival
    // This is a simplified deduplication buffer. A more robust one might store the packet itself for reordering.
    std::map<uint64_t, std::chrono::steady_clock::time_point> deduplication_log_;
    // Example key: (static_cast<uint64_t>(channel_id) << 32) | channel_seq;

    std::atomic<bool> running_;
    // Add any other necessary state, e.g., current retransmission server, connection status flags for retransmission.
    bool retrans_primary_active_ = true;
};

} // namespace Networking
#endif // NETWORK_MANAGER_H
