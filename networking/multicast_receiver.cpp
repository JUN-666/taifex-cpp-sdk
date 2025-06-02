#include "networking/multicast_receiver.h"
#include "logger.h" // Assuming a logger is available

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> // For close()
#include <cstring>  // For memset, strerror
#include <cerrno>   // For errno
#include <iostream> // For error logging if logger isn't fully set up
#include <vector>   // For buffer in receive_loop

namespace Networking {

// ActiveSubscription Destructor - needed if unique_ptr owns it and thread needs explicit joining here.
// However, MulticastReceiver::stop() is designed to join threads, so this might be simple.
MulticastReceiver::ActiveSubscription::~ActiveSubscription() {
    // If thread was dynamically allocated and needs deletion, or if socket needs closing here.
    // But typically, MulticastReceiver::stop() handles cleanup of resources associated with
    // ActiveSubscription before the unique_ptr is destroyed.
    // If receiver_thread is still joinable here, it's a bug in stop() logic.
    if (receiver_thread.joinable()) {
        // This might indicate an issue if stop() wasn't called or didn't complete.
        // Forcing a detach or join here could be problematic depending on state.
        // CoreUtils::Logger::Log(CoreUtils::LogLevel::WARNING, "ActiveSubscription destroyed with joinable thread for " + config.group_ip);
        // To be safe, if it's still joinable and we are destroying, try to join.
        // This assumes thread_running_flag is set to false elsewhere.
        // receiver_thread.join(); // This could block if thread is not exiting.
    }
    if (socket_fd >= 0) {
        // CoreUtils::Logger::Log(CoreUtils::LogLevel::WARNING, "ActiveSubscription destroyed with open socket for " + config.group_ip);
        // close(socket_fd); // Socket should be closed by stop()
    }
}


MulticastReceiver::MulticastReceiver(DataCallback callback)
    : data_callback_(std::move(callback)), running_(false) {
    LOG_INFO << "MulticastReceiver created.";
}

MulticastReceiver::~MulticastReceiver() {
    LOG_INFO << "MulticastReceiver shutting down...";
    stop();
}

bool MulticastReceiver::add_subscription(const std::string& group_ip, uint16_t port, const std::string& local_interface_ip) {
    if (running_) {
        LOG_WARNING << "Cannot add subscription while receiver is running.";
        return false;
    }
    MulticastGroupSubscription config;
    config.group_ip = group_ip;
    config.port = port;
    config.local_interface_ip = local_interface_ip;

    auto sub_ptr = std::make_unique<ActiveSubscription>(config); // Pass config to constructor
    // socket_fd is initialized to -1 by ActiveSubscription constructor
    // thread_running_flag is initialized to false by ActiveSubscription constructor

    subscriptions_.push_back(std::move(sub_ptr));
    LOG_INFO << "Added subscription for " << group_ip << ":" << port;
    return true;
}

bool MulticastReceiver::start() {
    if (running_) {
        LOG_WARNING << "MulticastReceiver already running.";
        return true;
    }
    if (subscriptions_.empty()) {
        LOG_WARNING << "No subscriptions to start.";
        return false;
    }

    running_ = true; // Set overall running flag
    bool any_started_successfully = false;

    for (auto& sub_unique_ptr : subscriptions_) {
        ActiveSubscription* sub = sub_unique_ptr.get();

        sub->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sub->socket_fd < 0) {
            LOG_ERROR << "Failed to create socket for " << sub->config.group_ip << ": " << strerror(errno);
            continue;
        }

        int reuse = 1;
        if (setsockopt(sub->socket_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
            LOG_ERROR << "Failed to set SO_REUSEADDR for " << sub->config.group_ip << ": " << strerror(errno);
            close(sub->socket_fd);
            sub->socket_fd = -1;
            continue;
        }

        struct sockaddr_in local_sockaddr;
        memset(&local_sockaddr, 0, sizeof(local_sockaddr));
        local_sockaddr.sin_family = AF_INET;
        local_sockaddr.sin_port = htons(sub->config.port);
        local_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY); // Bind to 0.0.0.0 to receive on specified port from any interface

        if (bind(sub->socket_fd, (struct sockaddr*)&local_sockaddr, sizeof(local_sockaddr)) < 0) {
            LOG_ERROR << "Failed to bind socket for " << sub->config.group_ip << ":" << sub->config.port << ": " << strerror(errno);
            close(sub->socket_fd);
            sub->socket_fd = -1;
            continue;
        }

        struct ip_mreq group_req;
        group_req.imr_multiaddr.s_addr = inet_addr(sub->config.group_ip.c_str());
        if (!sub->config.local_interface_ip.empty()) {
            group_req.imr_interface.s_addr = inet_addr(sub->config.local_interface_ip.c_str());
        } else {
            group_req.imr_interface.s_addr = htonl(INADDR_ANY);
        }

        if (setsockopt(sub->socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group_req, sizeof(group_req)) < 0) {
            LOG_ERROR << "Failed to join multicast group " << sub->config.group_ip << ": " << strerror(errno);
            close(sub->socket_fd);
            sub->socket_fd = -1;
            continue;
        }

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        if (setsockopt(sub->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
            LOG_WARNING << "Failed to set SO_RCVTIMEO for " << sub->config.group_ip << ". Loop may be less responsive to stop().";
        }

        sub->thread_running_flag = true; // Signal this specific thread it can run
        sub->receiver_thread = std::thread(&MulticastReceiver::receive_loop, this, sub);
        LOG_INFO << "Started listening on " << sub->config.group_ip << ":" << sub->config.port;
        any_started_successfully = true;
    }

    if (!any_started_successfully && !subscriptions_.empty()) {
        LOG_ERROR << "No multicast subscriptions could be started.";
        running_ = false; // Reset overall running flag if nothing started
        return false;
    }

    return true;
}

void MulticastReceiver::stop() {
    if (!running_.exchange(false)) { // Set to false and get previous value
        return; // Already stopped or stopping
    }
    LOG_INFO << "Stopping MulticastReceiver threads...";

    for (auto& sub_unique_ptr : subscriptions_) {
        if (sub_unique_ptr) {
            sub_unique_ptr->thread_running_flag = false; // Signal individual thread to stop
            if (sub_unique_ptr->receiver_thread.joinable()) {
                sub_unique_ptr->receiver_thread.join();
                LOG_DEBUG << "Joined thread for " << sub_unique_ptr->config.group_ip;
            }
            if (sub_unique_ptr->socket_fd >= 0) {
                close(sub_unique_ptr->socket_fd);
                sub_unique_ptr->socket_fd = -1;
                LOG_DEBUG << "Closed socket for " << sub_unique_ptr->config.group_ip;
            }
        }
    }
    subscriptions_.clear();
    LOG_INFO << "MulticastReceiver stopped.";
}

void MulticastReceiver::receive_loop(ActiveSubscription* sub_ptr) {
    if (!sub_ptr || sub_ptr->socket_fd < 0) {
        LOG_ERROR << "Invalid subscription or socket for receive_loop for " << (sub_ptr ? sub_ptr->config.group_ip : "unknown");
        return;
    }

    const int BUFFER_SIZE = 65535;
    std::vector<unsigned char> buffer(BUFFER_SIZE);
    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);

    LOG_DEBUG << "Receive loop started for " << sub_ptr->config.group_ip << ":" << sub_ptr->config.port;

    // Use thread_running_flag from the specific ActiveSubscription
    while (running_ && sub_ptr->thread_running_flag) {
        ssize_t bytes_received = recvfrom(sub_ptr->socket_fd, buffer.data(), BUFFER_SIZE, 0,
                                          (struct sockaddr*)&sender_addr, &sender_addr_len);

        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Timeout, check running flags
            }
            // Check flags again before logging error, as stop() might have closed the socket
            if (!(running_ && sub_ptr->thread_running_flag) && (errno == EINTR || errno == EBADF)) {
                 LOG_INFO << "recvfrom interrupted or socket closed on " << sub_ptr->config.group_ip << ", likely due to stop().";
                break;
            }
            LOG_ERROR << "recvfrom error on " << sub_ptr->config.group_ip << ": " << strerror(errno);
            if (running_ && sub_ptr->thread_running_flag) std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (bytes_received > 0 && data_callback_) {
            data_callback_(buffer.data(), static_cast<size_t>(bytes_received),
                           sub_ptr->config.group_ip, sub_ptr->config.port);
        }
    }
    LOG_DEBUG << "Receive loop ended for " << sub_ptr->config.group_ip << ":" << sub_ptr->config.port;
}

} // namespace Networking
