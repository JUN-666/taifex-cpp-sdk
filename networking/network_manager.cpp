#include "networking/network_manager.h"
#include "networking/multicast_receiver.h"
#include "networking/retransmission_client.h"
#include "sdk/taifex_sdk.h"
#include "common_header.h"
#include "logger.h"
#include "networking/retransmission_protocol.h" // For TaifexRetransmission::DataResponse102 etc.

#include <algorithm> // For std::find_if if needed

namespace Networking {

// Define a reasonable window for deduplication
const size_t DEDUPLICATION_WINDOW_SIZE = 100; // Store last 100 seq numbers per channel
// const std::chrono::milliseconds DEDUPLICATION_TIME_WINDOW_MS{200}; // Alternative time window (not used in this simple version)

NetworkManager::NetworkManager(Taifex::TaifexSdk* sdk_core_logic)
    : sdk_core_logic_(sdk_core_logic), running_(false), retrans_primary_active_(true) {
    if (!sdk_core_logic_) {
        // This is a critical error, NetworkManager cannot function without the SDK logic.
        // Consider throwing an exception or ensuring this condition is never met.
        LOG_CRITICAL << "NetworkManager created with null TaifexSdk pointer!";
    }
    LOG_INFO << "NetworkManager created.";
}

NetworkManager::~NetworkManager() {
    LOG_INFO << "NetworkManager shutting down...";
    stop();
}

bool NetworkManager::configure_and_start(const NetworkManagerConfig& config) {
    if (running_.load()) {
        LOG_WARNING << "NetworkManager already running. Stop first to reconfigure.";
        return false;
    }
    config_ = config;

    LOG_INFO << "NetworkManager configuring...";

    // Setup Multicast Receivers
    if (config_.multicast_feeds.empty()) {
        LOG_WARNING << "No multicast feeds configured.";
    } else {
        // Primary feed (first in the list)
        const auto& primary_feed_config = config_.multicast_feeds[0];
        primary_multicast_receiver_ = std::make_unique<MulticastReceiver>(
            [this, primary_feed_config](const unsigned char* data, size_t length, const std::string& grp_ip, uint16_t grp_port) {
                on_primary_multicast_data(data, length, grp_ip, grp_port);
            }
        );
        if(!primary_multicast_receiver_->add_subscription(primary_feed_config.group_ip, primary_feed_config.port, primary_feed_config.local_interface_ip)) {
             LOG_ERROR << "Failed to add subscription for primary multicast feed: " << primary_feed_config.group_ip;
             primary_multicast_receiver_.reset(); // Nullify if add_subscription failed
        } else {
            LOG_INFO << "Configured primary multicast feed: " << primary_feed_config.group_ip;
        }


        if (config_.dual_feed_enabled && config_.multicast_feeds.size() > 1) {
            const auto& secondary_feed_config = config_.multicast_feeds[1];
            secondary_multicast_receiver_ = std::make_unique<MulticastReceiver>(
                [this, secondary_feed_config](const unsigned char* data, size_t length, const std::string& grp_ip, uint16_t grp_port) {
                    on_secondary_multicast_data(data, length, grp_ip, grp_port);
                }
            );
            if(!secondary_multicast_receiver_->add_subscription(secondary_feed_config.group_ip, secondary_feed_config.port, secondary_feed_config.local_interface_ip)) {
                LOG_ERROR << "Failed to add subscription for secondary multicast feed: " << secondary_feed_config.group_ip;
                secondary_multicast_receiver_.reset();
            } else {
                 LOG_INFO << "Configured secondary multicast feed: " << secondary_feed_config.group_ip;
            }
        } else if (config_.dual_feed_enabled && config_.multicast_feeds.size() <= 1) {
            LOG_WARNING << "Dual feed enabled but only one or zero multicast feeds configured.";
        }
    }

    if (config_.primary_retrans_server) {
        retrans_primary_active_ = true; // Start with primary
        connect_retransmission_client(retrans_primary_active_);
    }

    running_ = true;
    bool primary_started = false;
    if (primary_multicast_receiver_) {
        if (primary_multicast_receiver_->start()) {
            primary_started = true;
        } else {
            LOG_ERROR << "Failed to start primary multicast receiver.";
            running_ = false;
        }
    }

    bool secondary_started = false;
    if (running_.load() && secondary_multicast_receiver_) { // Only start secondary if primary (or overall) is still good
        if (secondary_multicast_receiver_->start()) {
            secondary_started = true;
        } else {
            LOG_ERROR << "Failed to start secondary multicast receiver. Continuing with primary if available.";
            // Not setting running_ to false, can operate in single feed mode
        }
    }

    // Retransmission client start is done in connect_retransmission_client if config exists
    // Its start method internally launches a thread that tries to connect and login.

    if (!primary_started && !secondary_started && !retransmission_client_) { // No data sources configured or started
         LOG_WARNING << "NetworkManager started but no functional data sources (multicast/retransmission).";
         // running_ might still be true if retransmission was configured but start is async.
         // If primary multicast was configured but failed, running_ would be false.
    } else if (!running_.load()){
         LOG_ERROR << "NetworkManager failed to start essential components.";
         // cleanup partially started components
         if (primary_multicast_receiver_ && primary_started) primary_multicast_receiver_->stop();
         if (secondary_multicast_receiver_ && secondary_started) secondary_multicast_receiver_->stop();
         if (retransmission_client_) retransmission_client_->stop();
         return false;
    }

    LOG_INFO << "NetworkManager started.";
    return running_.load();
}


void NetworkManager::stop() {
    if (!running_.exchange(false)) { // Set to false and get previous value
        return;
    }
    LOG_INFO << "NetworkManager stopping...";
    if (primary_multicast_receiver_) {
        primary_multicast_receiver_->stop();
    }
    if (secondary_multicast_receiver_) {
        secondary_multicast_receiver_->stop();
    }
    if (retransmission_client_) {
        retransmission_client_->stop();
    }
    LOG_INFO << "NetworkManager stopped.";
}

void NetworkManager::on_primary_multicast_data(const unsigned char* data, size_t length,
                                               const std::string& source_group_ip, uint16_t source_group_port) {
    process_incoming_packet(data, length, true /*is_primary*/);
}

void NetworkManager::on_secondary_multicast_data(const unsigned char* data, size_t length,
                                                 const std::string& source_group_ip, uint16_t source_group_port) {
    process_incoming_packet(data, length, false /*is_primary*/);
}


void NetworkManager::process_incoming_packet(const unsigned char* data, size_t length, bool is_from_primary_feed, bool is_retransmitted) {
    if (!running_.load() || !sdk_core_logic_) return;

    CoreUtils::CommonHeader header;
    if (!CoreUtils::CommonHeader::parse(data, length, header)) {
        LOG_WARNING << "NM: Dropping packet - failed to parse common header. Source: " << (is_retransmitted ? "Retrans" : (is_from_primary_feed ? "PrimaryMC" : "SecondaryMC"));
        return;
    }
    uint32_t channel_id = header.getChannelId();
    uint64_t channel_seq = header.getChannelSeq();
    uint64_t combined_seq_key = (static_cast<uint64_t>(channel_id) << 32) | channel_seq;

    if (config_.dual_feed_enabled && !is_retransmitted) { // Deduplication for multicast feeds only
        auto now = std::chrono::steady_clock::now();
        // Check if sequence number already seen recently
        auto it = deduplication_log_.find(combined_seq_key);
        if (it != deduplication_log_.end()) {
            // Optional: Check timestamp if it's within a very small window of the first arrival to allow for network jitter
            // if (now - it->second < DEDUPLICATION_TIME_WINDOW_MS) { // Example time window check
                 LOG_DEBUG << "NM: Duplicate packet on Channel " << channel_id <<
                                   " Seq " << channel_seq << " from " << (is_from_primary_feed ? "primary" : "secondary") << ". Discarding.";
                return;
            // } else {
            //     // Seen before but outside time window, treat as new (or log as stale duplicate)
            //     it->second = now; // Update timestamp
            // }
        } else {
            deduplication_log_[combined_seq_key] = now;
            // Prune old entries from deduplication_log_ to prevent unbounded growth
            // This simple pruning removes the element with the smallest combined_seq_key, which might not be the oldest in time.
            // A time-based circular buffer or sorted list by time would be more accurate for time-window pruning.
            if (deduplication_log_.size() > DEDUPLICATION_WINDOW_SIZE * config_.multicast_feeds.size()) { // Rough estimate
                deduplication_log_.erase(deduplication_log_.begin());
            }
        }
    }
    forward_to_sdk(data, length);
}


void NetworkManager::on_retransmitted_market_data(const unsigned char* data, size_t length) {
    if (!running_.load() || !sdk_core_logic_) return;
    LOG_DEBUG << "NM: Received retransmitted market data (len: " << length << ")";
    process_incoming_packet(data, length, false /*is_primary, doesn't matter*/, true /*is_retransmitted*/);
}

void NetworkManager::forward_to_sdk(const unsigned char* data, size_t length) {
     if (sdk_core_logic_) {
        sdk_core_logic_->process_message(data, length);
    }
}


void NetworkManager::trigger_retransmission_request(uint16_t channel_id, uint32_t start_seq_num, uint16_t count) {
    if (!running_.load()) {
        LOG_WARNING << "NM: Cannot trigger retransmission, NetworkManager not running.";
        return;
    }
    if (retransmission_client_) {
        LOG_INFO << "NM: Triggering retransmission for Channel " << channel_id <<
                               " from Seq " << start_seq_num << ", Count " << count;
        if(!retransmission_client_->request_retransmission(channel_id, start_seq_num, count)) {
            LOG_WARNING << "NM: Failed to send retransmission request. Client might not be logged in or connected.";
            // Optionally, try to connect/login retransmission client again if it's not in a good state
            // This could be complex if the client is already in its reconnect loop.
        }
    } else {
        LOG_WARNING << "NM: Retransmission client not configured, cannot request retransmission.";
    }
}

void NetworkManager::connect_retransmission_client(bool use_primary_server) {
    const auto& server_config_opt = use_primary_server ? config_.primary_retrans_server : config_.backup_retrans_server;
    std::string server_type = use_primary_server ? "primary" : "backup";

    if (server_config_opt) {
        retransmission_client_ = std::make_unique<RetransmissionClient>(
            server_config_opt->ip,
            server_config_opt->port,
            server_config_opt->session_id,
            server_config_opt->password,
            [this](const unsigned char* data, size_t length) { this->on_retransmitted_market_data(data, length); },
            [this](const TaifexRetransmission::DataResponse102& resp, const std::vector<unsigned char>& retrans_data) { this->on_retransmission_status(resp); }, // Adjusted for new sig
            [this](const TaifexRetransmission::ErrorNotification010& err_msg) { this->on_retransmission_error(err_msg); }, // Adjusted
            [this]() { this->on_retransmission_disconnected(); },
            [this]() { LOG_INFO << "NM: Retransmission client logged in to " << (retrans_primary_active_ ? "primary" : "backup") << " server.";}
        );
        LOG_INFO << "NM: Configured " << server_type << " retransmission server: " << server_config_opt->ip;
        if (!retransmission_client_->start()) {
             LOG_ERROR << "NM: Failed to start retransmission client for " << server_type << " server.";
        }
    } else {
         LOG_WARNING << "NM: No " << server_type << " retransmission server configured.";
    }
}


// Other callback implementations (on_retransmission_status, etc.)
void NetworkManager::on_retransmission_status(const TaifexRetransmission::DataResponse102& response) { // Added retrans_data
    LOG_INFO << "NM: Retransmission status update. Channel: " << response.channel_id <<
                           ", Status: " << response.status_code <<
                           ", BeginSeq: " << response.begin_seq_no <<
                           ", RecoverNum: " << response.recover_num;
    // If DataResponse102 status indicates an error (e.g. "no data", "request error"),
    // this might be where logic to switch to a backup retransmission server could be triggered,
    // or to notify the application layer of failure to recover.
}

void NetworkManager::on_retransmission_error(const TaifexRetransmission::ErrorNotification010& error_msg) { // Changed to take struct
    LOG_ERROR << "NM: Retransmission client error notification. Status Code: " << error_msg.status_code;
    // ErrorNotification 010 from server usually means server will disconnect.
    // RetransmissionClient's receive_loop should detect this and call on_retransmission_disconnected.
}

void NetworkManager::on_retransmission_disconnected() {
    LOG_WARNING << "NM: Retransmission client disconnected.";
    // Simple strategy: if primary was active and backup is configured, try switching to backup.
    // More complex logic would involve retry counts, timers, etc.
    if (retrans_primary_active_ && config_.backup_retrans_server) {
        LOG_INFO << "NM: Primary retransmission server disconnected. Attempting to switch to backup.";
        if (retransmission_client_) { // Stop existing client first
            retransmission_client_->stop();
        }
        retrans_primary_active_ = false;
        connect_retransmission_client(retrans_primary_active_);
    } else if (!retrans_primary_active_ && config_.primary_retrans_server) {
        // Was on backup, try switching back to primary (or just keep retrying current one via its internal loop)
        LOG_INFO << "NM: Backup retransmission server disconnected. RetransmissionClient will attempt to reconnect.";
        // RetransmissionClient's own receive_loop will try to reconnect to its configured server.
        // If we want NetworkManager to manage failover back to primary, more state is needed.
    }
    // If no backup or already on backup, the RetransmissionClient's internal reconnect logic will handle retries to its current target.
}


} // namespace Networking
