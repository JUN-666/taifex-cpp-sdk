#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <chrono>
#include <optional>
#include <atomic>

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

class MulticastReceiver;
class RetransmissionClient;

/**
 * @brief Configuration for the NetworkManager.
 *
 * This structure holds all necessary parameters to configure multicast feeds
 * and retransmission services.
 */
struct NetworkManagerConfig {
    /**
     * @brief Configuration for a single multicast feed.
     */
    struct MulticastFeedConfig {
        /** @brief Multicast group IP address (e.g., "225.0.140.140"). */
        std::string group_ip;
        /** @brief Multicast group port number. */
        uint16_t port;
        /**
         * @brief IP address of the local network interface to use for joining the multicast group.
         *        Leave empty to let the system choose an appropriate interface.
         *        Specify (e.g., "192.168.1.100") to bind to a specific local NIC.
         */
        std::string local_interface_ip;
        /**
         * @brief If `NetworkManagerConfig::dual_feed_enabled` is true, this flag is illustrative.
         *        The NetworkManager will treat the first entry in `multicast_feeds` as primary
         *        and the second as secondary for internal callback routing if dual feed is active.
         *        Actual data processing logic in NetworkManager uses an `is_primary` flag passed by callbacks.
         */
        bool is_primary_for_dual_feed = false;
    };
    /**
     * @brief Vector of multicast feed configurations.
     *        Typically contains one entry for a single feed, or two entries if `dual_feed_enabled` is true.
     *        If dual feed is enabled, the first entry is considered the primary feed, and the second is secondary.
     */
    std::vector<MulticastFeedConfig> multicast_feeds;

    /**
     * @brief Configuration for a TAIFEX Retransmission Server.
     */
    struct RetransmissionServerConfig {
        /** @brief IP address of the TAIFEX Retransmission Server. */
        std::string ip;
        /** @brief Port number of the TAIFEX Retransmission Server. */
        uint16_t port;
        /** @brief Session ID provided by TAIFEX for the retransmission service. */
        uint16_t session_id;
        /** @brief Password associated with the Session ID, used for calculating CheckCode in LoginRequest020. */
        std::string password;
    };
    /**
     * @brief Configuration for the primary retransmission server.
     *        Set this if retransmission functionality is required.
     */
    std::optional<RetransmissionServerConfig> primary_retrans_server;
    /**
     * @brief Configuration for a backup retransmission server. Optional.
     *        NetworkManager may attempt to switch to this if the primary server becomes unavailable.
     */
    std::optional<RetransmissionServerConfig> backup_retrans_server;

    /**
     * @brief Enables dual-feed processing logic (primarily deduplication).
     *        If true, requires at least two entries in `multicast_feeds` to be effective.
     */
    bool dual_feed_enabled = false;
    /**
     * @brief Time window for considering packets with the same sequence number as duplicates in dual-feed mode.
     *        Note: The current simple deduplication in `NetworkManager` is primarily sequence-number based and uses
     *        `DEDUPLICATION_WINDOW_SIZE` (a count) for pruning its log, not this time window directly for discarding.
     *        This field is for future enhancement or more sophisticated reordering/deduplication logic.
     */
    std::chrono::milliseconds dual_feed_reorder_window_ms{100};
};

/**
 * @brief Manages network communication for TAIFEX market data.
 *
 * This class is responsible for:
 * - Receiving market data via multicast (supports single or dual feed).
 * - Handling dual-feed data deduplication.
 * - Connecting to the TAIFEX Retransmission Server for data recovery.
 * - Forwarding processed and unique market data packets to the `TaifexSdk` core logic.
 * - Triggering retransmission requests based on instructions from `TaifexSdk`.
 *
 * It orchestrates `MulticastReceiver` and `RetransmissionClient` instances.
 */
class NetworkManager {
public:
    /**
     * @brief Constructs a NetworkManager.
     * @param sdk_core_logic A non-owning pointer to the `TaifexSdk` instance that will
     *                       process the received market data. This pointer must remain valid
     *                       for the lifetime of the NetworkManager.
     */
    explicit NetworkManager(Taifex::TaifexSdk* sdk_core_logic);

    /**
     * @brief Destructor. Stops all network activity and cleans up resources.
     */
    ~NetworkManager();

    // Prevent copying and assignment
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    /**
     * @brief Configures and starts the network components (multicast receivers, retransmission client).
     * @param config The `NetworkManagerConfig` structure containing all necessary settings.
     * @return True if configuration is applied and components are started successfully (or at least,
     *         the attempt to start them was initiated). False if critical configuration is missing
     *         or initial startup of essential components fails.
     * @note If dual feed is enabled in `config`, it expects at least two entries in `config.multicast_feeds`.
     */
    bool configure_and_start(const NetworkManagerConfig& config);

    /**
     * @brief Stops all network activity.
     * Stops multicast receivers and disconnects the retransmission client.
     * Joins any running network threads.
     */
    void stop();

    /**
     * @brief Triggers a retransmission request for a specified channel and sequence range.
     * This method is typically called by `TaifexSdk` when its sequence validation logic detects a gap.
     * @param channel_id The Channel ID for which data is being requested.
     * @param start_seq_num The starting sequence number for retransmission.
     * @param count The number of messages to recover.
     */
    void trigger_retransmission_request(uint16_t channel_id, uint32_t start_seq_num, uint16_t count);

#ifdef ENABLE_TEST_HOOKS // Example for enabling test hooks
public: // Or make the test class a friend
    void on_primary_multicast_data_for_test(const unsigned char* data, size_t length, const std::string& grp_ip, uint16_t grp_port) {
        on_primary_multicast_data(data, length, grp_ip, grp_port);
    }
    void on_secondary_multicast_data_for_test(const unsigned char* data, size_t length, const std::string& grp_ip, uint16_t grp_port) {
        on_secondary_multicast_data(data, length, grp_ip, grp_port);
    }
#endif

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


    Taifex::TaifexSdk* sdk_core_logic_;
    NetworkManagerConfig config_;

    std::unique_ptr<MulticastReceiver> primary_multicast_receiver_;
    std::unique_ptr<MulticastReceiver> secondary_multicast_receiver_;
    std::unique_ptr<RetransmissionClient> retransmission_client_;

    std::map<uint64_t, std::chrono::steady_clock::time_point> deduplication_log_;

    std::atomic<bool> running_;
    bool retrans_primary_active_ = true;
};

} // namespace Networking
#endif // NETWORK_MANAGER_H
