#include "utils/log_file_packet_simulator.h" // As per Step 3
#include "sdk/taifex_sdk.h"                 // As per Dept E
#include "logger.h"              // For logging control/output

#include <iostream>
#include <string>
#include <vector>
#include <chrono>   // For std::chrono::microseconds (optional delay)
#include <thread>   // For std::this_thread::sleep_for (optional delay)


int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <log_file_path>" << std::endl;
        return 1;
    }

    std::string log_filepath = argv[1];

    // Optional: Configure logger level for more verbose output during replay
    CoreUtils::setLogLevel(CoreUtils::LogLevel::INFO); // Or DEBUG for more details

    LOG_INFO << "Starting PCAP Replay Example...";

    // 1. Instantiate the SDK
    Taifex::TaifexSdk sdk;
    if (!sdk.initialize()) {
        LOG_ERROR << "Failed to initialize TaifexSdk.";
        return 1;
    }
    LOG_INFO << "TaifexSdk initialized.";

    // 2. Instantiate the LogFilePacketSimulator
    // Assuming constructor with default header sizes is used, or adjust if specific sizes are needed for the pcap.
    Utils::LogFilePacketSimulator simulator(log_filepath);
    if (!simulator.open()) {
        LOG_ERROR << "Failed to open log file: " + log_filepath;
        return 1;
    }
    LOG_INFO << "Log file opened: " + log_filepath;

    // 3. Loop and process packets
    long packet_count = 0;
    while (simulator.has_next_packet()) {
        std::vector<unsigned char> taifex_packet = simulator.get_next_taifex_packet();
        if (!taifex_packet.empty()) {
            packet_count++;
            LOG_DEBUG << "Replaying packet " + std::to_string(packet_count) +
                                                            " (size: " + std::to_string(taifex_packet.size()) + " bytes)";
            sdk.process_message(taifex_packet.data(), taifex_packet.size());

            // Optional: Add a small delay if simulating real-time arrival
            // std::this_thread::sleep_for(std::chrono::microseconds(100));
        } else {
            // get_next_taifex_packet() might return empty if it couldn't parse a record,
            // found no ESC code, or hit EOF during packet read.
            // The simulator logs these issues internally.
            // Check if it's truly EOF or just a problematic record to decide whether to break or continue.
            if (!simulator.has_next_packet() && simulator.is_open()) {
                // If has_next_packet is false after get_next_taifex_packet returned empty,
                // it's likely a clean EOF or a stream error that has_next_packet can also detect.
                LOG_INFO << "End of file or stream error reached after processing " + std::to_string(packet_count) + " packets.";
                break; // Exit loop on EOF or persistent error
            } else if (simulator.is_open()) {
                // Still open and !has_next_packet was false before, but got empty packet.
                // This implies a single problematic record was skipped by the simulator.
                LOG_WARNING << "Simulator returned an empty TAIFEX packet but file stream seems ok. Possibly a malformed PCAP record or non-TAIFEX data. Continuing...";
            } else {
                // File is no longer open (e.g., simulator closed it due to critical error)
                 LOG_ERROR << "Simulator file stream is no longer open after attempting to get a packet. Processed " + std::to_string(packet_count) + " packets.";
                break;
            }
        }
    }

    LOG_INFO << "Finished replaying log file. Total TAIFEX packets processed: " + std::to_string(packet_count);

    // 4. Close the simulator (which closes the file)
    simulator.close();

    // Example: Retrieve and print some info after processing
    // This part depends on what product IDs are in the log file.
    // std::string sample_product_id_s = "TXF"; // Replace with an actual PROD-ID-S from your log
    // auto product_info = sdk.get_product_info(sample_product_id_s);
    // if (product_info) {
    //     const auto& info = product_info->get();
    //     CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "Product Info for " + sample_product_id_s +
    //         ": Found. Kind: " + std::string(1, info.prod_kind) +
    //         ", DecLoc: " + std::to_string(info.decimal_locator));
    // } else {
    //     CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "Product Info for " + sample_product_id_s + ": Not found.");
    // }

    // std::string sample_ob_product_id = "TXF202403"; // Replace with an actual order book PROD-ID from your log
    // auto order_book_opt = sdk.get_order_book(sample_ob_product_id);
    // if (order_book_opt) {
    //     const auto& ob = order_book_opt->get();
    //     CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "Order Book for " + sample_ob_product_id +
    //         ": Found. Last Update Seq: " + std::to_string(ob.get_last_prod_msg_seq()));
    //     auto bids = ob.get_top_bids(1);
    //     if (!bids.empty()) {
    //         CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "  Best Bid: Price=" + std::to_string(bids[0].price) +
    //                                                        " Qty=" + std::to_string(bids[0].quantity));
    //     } else {
    //         CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "  Best Bid: (empty)");
    //     }
    //     auto asks = ob.get_top_asks(1);
    //     if (!asks.empty()) {
    //         CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "  Best Ask: Price=" + std::to_string(asks[0].price) +
    //                                                        " Qty=" + std::to_string(asks[0].quantity));
    //     } else {
    //         CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "  Best Ask: (empty)");
    //     }
    // } else {
    //     CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "Order Book for " + sample_ob_product_id + ": Not found.");
    // }

    return 0;
}
