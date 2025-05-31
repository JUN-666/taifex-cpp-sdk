# TAIFEX Message Processing C++ SDK (MessageProcessingSDK)

## 1. Overview

This project is a C++ SDK designed to process real-time market data messages from the Taiwan Futures Exchange (TAIFEX). It provides functionalities for parsing various message types, maintaining order books for financial products, handling network communication (UDP multicast for live data, TCP/IP for retransmission), and offering a structured API for client applications.

The SDK is organized into several modules, each responsible for a specific part of the processing pipeline.

## 2. Modules

The SDK is composed of the following main libraries/modules:

*   **CoreUtils (`libcore_utils.a`)**:
    *   Provides fundamental utilities used across the SDK.
    *   Includes:
        *   Checksum calculation (TAIFEX market data specific).
        *   PACK BCD (Packed Binary Coded Decimal) conversion.
        *   String manipulation utilities.
        *   Logging framework (`CoreUtils::Logger`).
        *   Common Header parsing (`CoreUtils::CommonHeader`) for all TAIFEX messages.
        *   Message type identification (`CoreUtils::MessageIdentifier`).
        *   Error code definitions and custom exceptions.

*   **SpecificMessageParsers (`libspecific_message_parsers.a`)**:
    *   Contains parsers for the body of specific TAIFEX message types.
    *   Input is the raw message body bytes and length (obtained after CommonHeader parsing).
    *   Output is a C++ struct representing the fields of that specific message.
    *   Supported messages include:
        *   I010 - Product Basic Data
        *   I081 - Order Book Update (Differential)
        *   I083 - Order Book Snapshot
        *   I001 - Heartbeat
        *   I002 - Sequence Reset
    *   Headers are typically found in `include/SpecificMessageParsers/`.

*   **OrderBookLib (`liborder_book_lib.a`)**:
    *   Implements the `OrderBookManagement::OrderBook` class.
    *   Manages the order book (bids, asks, derived quotes) for a single financial product.
    *   Processes I083 (snapshot) and I081 (update) messages to maintain book state.
    *   Handles sequence resets (I002).
    *   Uses scaled integers (`int64_t`) for price representation based on `decimal_locator` from I010.
    *   Header: `include/OrderBookManagement/order_book.h`.

*   **TaifexNetworkingLib (`libtaifex_networking_lib.a`)**:
    *   Handles network communication for receiving market data.
    *   Key class: `Networking::NetworkManager`.
    *   Supports:
        *   UDP Multicast: Receiving live market data from multiple TAIFEX channels.
        *   TCP/IP Retransmission: Connecting to TAIFEX retransmission servers to request and receive missed packets, implementing the TAIFEX binary retransmission protocol.
        *   Dual-Feed Deduplication (Basic): If configured with two multicast feeds, performs deduplication of packets based on Channel ID and Sequence Number.
    *   Provides raw, validated byte streams to the `TaifexSdk` core for processing.
    *   Headers: `include/TaifexNetworking/network_manager.h`, `retransmission_protocol.h`, `endian_utils.h`.

*   **TaifexSdk (`libtaifex_sdk_lib.a`)**:
    *   The main SDK facade library, integrating all other modules.
    *   Key class: `Taifex::TaifexSdk`.
    *   Responsibilities:
        *   Orchestrating the message processing pipeline: checksum validation, header parsing, message identification, dispatch to specific body parsers.
        *   Managing state:
            *   Cache for product information (from I010 messages).
            *   Collection of `OrderBook` instances for various products.
            *   Tracking channel sequence numbers and performing basic validation (gap detection, replay).
        *   Providing a public API for client applications to:
            *   Initialize the SDK.
            *   Submit raw market data messages (`process_message`).
            *   Query product information (`get_product_info`).
            *   Query order book state (`get_order_book`).
    *   Main public header: `include/Taifex/taifex_sdk.h`.

*   **Utilities (`utils/`)**
    *   `LogFilePacketSimulator`: A utility class to read PCAP-like log files and extract raw TAIFEX messages for replaying into the SDK. Useful for testing and simulation. (Not built into a library by default, used in examples).

## 3. Build Instructions

The project uses CMake for building.

### Prerequisites
*   A C++ compiler supporting C++11 (or C++17 if using `std::optional` extensively without polyfills - see notes).
*   CMake (version 3.10 or higher).
*   Standard POSIX environment for networking (sockets, pthreads). For Windows, Winsock is used.
*   (No other external library dependencies are explicitly required by the core SDK).

### Building
1.  Create a build directory:
    ```bash
    mkdir build
    cd build
    ```
2.  Run CMake to configure the project:
    ```bash
    cmake ..
    ```
    You can specify a generator if needed (e.g., `-G "MinGW Makefiles"` or `-G "Visual Studio 16 2019"`).
    To build shared libraries (e.g., .so, .dll), you can add `-DBUILD_SHARED_LIBS=ON`. Default is static libraries.

3.  Compile the project:
    ```bash
    cmake --build .
    ```
    Or use the build system directly (e.g., `make` on Linux/macOS, or open the solution in Visual Studio).

4.  (Optional) Install the libraries and headers:
    ```bash
    cmake --install . --prefix "your/install/path"
    ```
    This will install libraries into `your/install/path/lib` and headers into `your/install/path/include`.

5.  (Optional) Run tests:
    ```bash
    ctest
    ```
    This will execute all defined unit tests.

## 4. Basic Usage Example (`TaifexSdk`)

```cpp
#include "Taifex/taifex_sdk.h"
#include "CoreUtils/logger.h" // For logging
#include "TaifexNetworking/network_manager.h" // For NetworkManager and its config
#include "utils/log_file_packet_simulator.h" // For file replay example

#include <iostream>
#include <vector> // For raw_message_data
#include <thread> // For std::this_thread
#include <chrono> // For std::chrono

// Assume raw_message_data is a std::vector<unsigned char> or const unsigned char*
// containing a full TAIFEX message (header, body, checksum, terminal code).

int main(int argc, char* argv[]) {
    // Configure logger
    CoreUtils::Logger::SetLevel(CoreUtils::LogLevel::INFO);

    // 1. Initialize SDK components
    Taifex::TaifexSdk sdk;
    if (!sdk.initialize()) {
        CoreUtils::Logger::Log(CoreUtils::LogLevel::FATAL, "SDK initialization failed!");
        return 1;
    }
    CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "TaifexSdk initialized.");

    // --- Option A: Using NetworkManager for live data (conceptual) ---
    // Networking::NetworkManagerConfig net_config;
    // // Populate net_config with multicast group IPs/ports, retransmission server details.
    // // Example for one multicast feed:
    // net_config.multicast_feeds.push_back({"225.0.140.140", 14000, "your_local_interface_ip"}); // is_primary_for_dual_feed defaults to false
    // // net_config.dual_feed_enabled = false; // Default
    // // if you have a retransmission server:
    // // net_config.primary_retrans_server = { "retrans_ip", 9000, 12345, "password" };
    //
    // Networking::NetworkManager network_manager(&sdk); // Pass SDK pointer
    // if (!network_manager.configure_and_start(net_config)) {
    //    CoreUtils::Logger::Log(CoreUtils::LogLevel::FATAL, "NetworkManager failed to start!");
    //    // return 1; // In a real app, might try to run without network or exit
    // } else {
    //    CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "NetworkManager started.");
    //    // Data will now flow from network_manager to sdk.process_message via internal callbacks.
    //    // Application would typically wait or do other work here.
    //    // For this example, we might just sleep or proceed to file replay.
    // }


    // --- Option B: Processing a single message manually (example) ---
    // const unsigned char sample_raw_message_data[] = { /* ... bytes of a full TAIFEX message ... */ };
    // size_t message_length = sizeof(sample_raw_message_data);
    // if (message_length > 0) {
    //     sdk.process_message(sample_raw_message_data, message_length);
    // }


    // --- Option C: Using LogFilePacketSimulator (if you have a log file) ---
    if (argc < 2) {
        CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "No log file provided for replay. Skipping file replay.");
    } else {
        std::string log_filepath = argv[1];
        CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "Attempting to replay from: " + log_filepath);
        Utils::LogFilePacketSimulator simulator(log_filepath);
        if (simulator.open()) {
            CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "Log file opened. Starting replay...");
            long packet_count = 0;
            while (simulator.has_next_packet()) {
                std::vector<unsigned char> packet = simulator.get_next_taifex_packet();
                if (!packet.empty()) {
                    sdk.process_message(packet.data(), packet.size());
                    packet_count++;
                    if (packet_count % 1000 == 0) { // Log progress every 1000 packets
                        CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "Processed " + std::to_string(packet_count) + " packets...");
                    }
                }
            }
            simulator.close();
            CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "Finished replaying. Total packets processed: " + std::to_string(packet_count));
        } else {
            CoreUtils::Logger::Log(CoreUtils::LogLevel::ERROR, "Failed to open log file: " + log_filepath);
        }
    }


    // 4. Accessing data from the SDK (examples)
    std::string product_id_for_info = "TXF"; // Example PROD-ID-S (typically 10 chars or less, space-padded if needed by I010)
    auto prod_info_opt = sdk.get_product_info(product_id_for_info);
    if (prod_info_opt) {
        const auto& info = prod_info_opt->get(); // info is std::reference_wrapper<const MessageI010>
        // Access members like info.prod_id_s, info.decimal_locator
        std::cout << "Product " << product_id_for_info << " Info: Decimal Locator = "
                  << static_cast<int>(info.decimal_locator) << std::endl;
    } else {
        std::cout << "Product info for " << product_id_for_info << " not found." << std::endl;
    }

    std::string product_id_for_book = "TXF202409"; // Example full product ID for order book (often longer)
    auto order_book_opt = sdk.get_order_book(product_id_for_book);
    if (order_book_opt) {
        const auto& book = order_book_opt->get(); // book is std::reference_wrapper<const OrderBook>
        std::cout << "Order book for " << book.get_product_id()
                  << ", Last Msg Seq: " << book.get_last_prod_msg_seq() << std::endl;
        auto top_bids = book.get_top_bids(1);
        if (!top_bids.empty()) {
            std::cout << "  Best Bid: Price=" << top_bids[0].price
                      << " Qty=" << top_bids[0].quantity << std::endl;
        } else {
             std::cout << "  Best Bid: (empty)" << std::endl;
        }
    } else {
        std::cout << "Order book for " << product_id_for_book << " not found." << std::endl;
    }

    // if (network_manager.is_running_or_configured_to_run_something) { // Fictitious check
    //    CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "Stopping NetworkManager...");
    //    network_manager.stop();
    // }

    CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "Example finished.");
    return 0;
}
```

## 5. Dependencies and System Requirements

*   **C++ Standard**: Primarily C++11. Some components (like `OrderBook` and `TaifexSdk` getters using `std::optional`, and some test files using `if constexpr`) leverage C++17 features. If a strict C++11 compiler without access to these features (e.g., via `<experimental/optional>` or backports) is used, these parts might need adjustments (e.g., returning raw pointers instead of `std::optional<std::reference_wrapper<...>>`). The CMake `CMAKE_CXX_STANDARD` is currently set to 11. Consider changing to 17 if these features are heavily used and widely available in target environments.
*   **Threading**: The `CoreUtils` (logger, if thread-safe features are added) and `TaifexNetworkingLib` (multicast, TCP client) use `std::thread`. Linkage against a pthreads-compatible library is required (handled by `find_package(Threads REQUIRED)` in CMake).
*   **Networking**: Uses standard POSIX sockets for UDP/TCP operations on Linux/macOS. On Windows, Winsock (`ws2_32.lib`) is used (typically linked automatically by CMake/compiler when Windows headers are included).
*   **Endianness**: The retransmission protocol messages are defined with fields in Network Byte Order (Big Endian). `NetworkingUtils::endian_utils.h` provides helpers for byte order conversions. Market data messages received via multicast are typically processed as is, with field parsing respecting their defined formats (often PACK BCD or direct binary).
*   **Build System**: CMake (version 3.10 or higher recommended).
*   **Logging**: The SDK uses `CoreUtils::Logger`. By default, it logs to standard output. Its verbosity level can be configured programmatically (e.g., `CoreUtils::Logger::SetLevel(CoreUtils::LogLevel::DEBUG);`).

## 6. Future Enhancements / To-Do (Examples)
*   More robust error propagation mechanisms from `TaifexSdk::process_message` and other key operations.
*   Implementation of callback mechanisms in `TaifexSdk` for asynchronous event notifications (e.g., order book updates, specific trade events, system events).
*   Full implementation of advanced dual-feed reordering logic in `NetworkManager`, possibly using the `dual_feed_reorder_window_ms`.
*   Support for additional TAIFEX message types (e.g., I024 - Trading Session Status, I025 - FCM Data, I140 - RFQ).
*   More comprehensive unit and integration tests, particularly for networking components using mock servers or loopback tests.
*   Refinement of BCD conversion utilities in tests to use a centralized, robust BCD packing/unpacking utility instead of simplified placeholder logic in test data generation helpers.
*   Consideration for making the SDK more thread-safe if concurrent access to `TaifexSdk` instances or shared data (like order books) is required.
*   Formalize product ID mapping for `get_or_create_order_book` if complex product IDs have non-obvious relations to `PROD-ID-S` in I010 messages.
```
