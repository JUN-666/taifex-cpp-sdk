#ifndef MULTICAST_RECEIVER_H
#define MULTICAST_RECEIVER_H

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <memory> // For std::unique_ptr if threads are managed this way

namespace Networking {

struct MulticastGroupSubscription {
    std::string group_ip;
    uint16_t port;
    std::string local_interface_ip; // IP of the local interface to join on
    // Internal state:
    // int socket_fd = -1; // Managed by ActiveSubscription
};

class MulticastReceiver {
public:
    using DataCallback = std::function<void(const unsigned char* data, size_t length,
                                            const std::string& source_group_ip, uint16_t source_group_port)>;

    explicit MulticastReceiver(DataCallback callback);
    ~MulticastReceiver();

    MulticastReceiver(const MulticastReceiver&) = delete;
    MulticastReceiver& operator=(const MulticastReceiver&) = delete;

    bool add_subscription(const std::string& group_ip, uint16_t port, const std::string& local_interface_ip = "");
    bool start();
    void stop();

private:
    // Forward declare ActiveSubscription to hide its details from the header if possible,
    // or define it here if it needs to be known by std::vector<std::unique_ptr<ActiveSubscription>>.
    // For std::unique_ptr, the type must be complete at point of destruction, which usually means
    // the definition must be known by ~MulticastReceiver(). So, often defined in .cpp and only forward-declared here
    // if destructor and other methods needing full type are in .cpp.
    // For this structure, let's keep it internal to .cpp by using PIMPL or careful unique_ptr management.
    // However, the provided structure has `std::vector<std::unique_ptr<ActiveSubscription>> subscriptions_;`
    // which requires ActiveSubscription to be a complete type if `std::unique_ptr` needs to call delete on it.
    // So, we should define ActiveSubscription here or make it a PIMPL.
    // The prompt shows it defined here.
    struct ActiveSubscription {
        MulticastGroupSubscription config;
        int socket_fd = -1;
        std::thread receiver_thread;
        std::atomic<bool> thread_running_flag{false}; // To signal the thread to stop

        // Need to ensure thread is joinable or detached appropriately.
        // If joinable, destructor of ActiveSubscription or MulticastReceiver needs to handle it.
        ActiveSubscription(const MulticastGroupSubscription& cfg) : config(cfg), socket_fd(-1) {}
        // Custom destructor to manage thread might be needed if not handled in MulticastReceiver::stop
        ~ActiveSubscription();
    };

    void receive_loop(ActiveSubscription* sub_ptr);

    DataCallback data_callback_;
    std::vector<std::unique_ptr<ActiveSubscription>> subscriptions_;
    std::atomic<bool> running_; // Overall running state for starting/stopping all threads
};

} // namespace Networking
#endif // MULTICAST_RECEIVER_H
