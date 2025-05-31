#ifndef MULTICAST_RECEIVER_H
#define MULTICAST_RECEIVER_H

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>

namespace Networking {

/**
 * @brief Configuration for a single multicast group subscription.
 */
struct MulticastGroupSubscription {
    /** @brief The multicast group IP address (e.g., "225.0.1.1"). */
    std::string group_ip;
    /** @brief The port number for the multicast group. */
    uint16_t port;
    /**
     * @brief IP address of the local network interface to use for joining the multicast group.
     *        If empty, the system will choose an appropriate interface.
     */
    std::string local_interface_ip;
};

/**
 * @brief Receives data from one or more multicast groups.
 *
 * This class allows subscribing to multiple multicast feeds. Each subscription
 * runs in its own thread, receiving data and invoking a user-provided callback.
 * It handles socket creation, binding, joining multicast groups, and thread management.
 */
class MulticastReceiver {
public:
    /**
     * @brief Callback function type for received multicast data.
     * @param data Pointer to the received data buffer.
     * @param length Length of the received data in bytes.
     * @param source_group_ip IP address of the multicast group from which data was received.
     * @param source_group_port Port number of the multicast group.
     */
    using DataCallback = std::function<void(const unsigned char* data, size_t length,
                                            const std::string& source_group_ip, uint16_t source_group_port)>;

    /**
     * @brief Constructs a MulticastReceiver.
     * @param callback The callback function to be invoked when data is received on any subscribed group.
     */
    explicit MulticastReceiver(DataCallback callback);

    /**
     * @brief Destructor. Stops all receiving threads and cleans up resources.
     */
    ~MulticastReceiver();

    // Disable copy and assignment to prevent issues with resource management (threads, sockets).
    MulticastReceiver(const MulticastReceiver&) = delete;
    MulticastReceiver& operator=(const MulticastReceiver&) = delete;

    /**
     * @brief Adds a multicast group subscription to the receiver.
     * This method must be called before `start()`. Subscriptions cannot be added while running.
     * @param group_ip The multicast group IP address to subscribe to.
     * @param port The port number of the multicast group.
     * @param local_interface_ip Optional IP address of the local network interface to use.
     *                           If empty, the system chooses the interface.
     * @return True if the subscription was successfully added for later startup, false if the receiver is already running.
     */
    bool add_subscription(const std::string& group_ip, uint16_t port, const std::string& local_interface_ip = "");

    /**
     * @brief Starts the multicast receiver.
     * This initiates listening on all added subscriptions, each in its own thread.
     * @return True if all subscriptions were started successfully (or if at least one started and others
     *         were problematic but not critical failures preventing any operation).
     *         Returns false if no subscriptions are configured or if a critical error occurs
     *         preventing any subscription from starting.
     */
    bool start();

    /**
     * @brief Stops the multicast receiver.
     * Signals all receiving threads to terminate, joins them, and closes all sockets.
     */
    void stop();

private:
    /**
     * @brief Internal structure to manage an active subscription's state.
     * Holds configuration, socket descriptor, receiver thread, and a flag for thread control.
     */
    struct ActiveSubscription {
        MulticastGroupSubscription config;
        int socket_fd = -1;
        std::thread receiver_thread;
        std::atomic<bool> thread_running_flag{false};

        ActiveSubscription(const MulticastGroupSubscription& cfg); // Defined in .cpp
        ~ActiveSubscription(); // Defined in .cpp
    };

    /**
     * @brief The main loop executed by each receiver thread for an active subscription.
     * @param sub_ptr Pointer to the ActiveSubscription this thread is managing.
     */
    void receive_loop(ActiveSubscription* sub_ptr);

    DataCallback data_callback_;
    std::vector<std::unique_ptr<ActiveSubscription>> subscriptions_;
    std::atomic<bool> running_;
};

} // namespace Networking
#endif // MULTICAST_RECEIVER_H
