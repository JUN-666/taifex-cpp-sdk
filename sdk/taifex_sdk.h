#ifndef TAIFEX_SDK_H
#define TAIFEX_SDK_H

#include <string>
#include <vector>
#include <map>
#include <memory> // For std::unique_ptr if managing OrderBooks that way, or just direct objects in map.
#include <optional>
#include <functional> // For std::reference_wrapper if returning const references via optional

// Forward declarations for types from other modules
namespace CoreUtils {
    class Logger; // Assuming Logger exists and might be used
    struct CommonHeader;
    enum class MessageType : uint16_t; // From message_identifier.h
}

namespace SpecificMessageParsers {
    struct MessageI010;
    struct MessageI081;
    struct MessageI083;
    // Other message structs if TaifexSdk is to handle them directly
}

namespace OrderBookManagement {
    class OrderBook;
}

namespace Taifex {

class TaifexSdk {
public:
    TaifexSdk();
    ~TaifexSdk();

    /**
     * @brief Initializes the SDK. (e.g., set up logging).
     * @return True if initialization is successful, false otherwise.
     */
    bool initialize(/* Potential config parameters */);

    /**
     * @brief Processes a raw incoming message.
     * This is the main entry point for feeding data into the SDK.
     * @param raw_message Pointer to the byte array of the message.
     * @param length Length of the raw_message in bytes.
     */
    void process_message(const unsigned char* raw_message, size_t length);

    /**
     * @brief Retrieves a const reference to the order book for a given product ID.
     * @param product_id The product identifier.
     * @return An std::optional containing a const reference to the OrderBook if found,
     *         otherwise std::nullopt.
     * @note The lifetime of the reference is tied to the TaifexSdk instance and its internal map.
     */
    std::optional<std::reference_wrapper<const OrderBookManagement::OrderBook>>
    get_order_book(const std::string& product_id) const;

    /**
     * @brief Retrieves a const reference to the product information (I010 data) for a given product ID.
     * @param product_id The product identifier (PROD-ID-S from I010).
     * @return An std::optional containing a const reference to the MessageI010 if found,
     *         otherwise std::nullopt.
     * @note The lifetime of the reference is tied to the TaifexSdk instance and its internal cache.
     */
    std::optional<std::reference_wrapper<const SpecificMessageParsers::MessageI010>>
    get_product_info(const std::string& product_id) const;

    // TODO: Add callback registration mechanism if needed.

private:
    // --- Private Helper Methods for Message Processing ---
    void dispatch_message_body(const unsigned char* body_ptr,
                               uint16_t body_len,
                               CoreUtils::MessageType msg_type,
                               const CoreUtils::CommonHeader& header,
                               const std::string& product_id_from_header); // product_id can be extracted from I081/I083 body

    void handle_i010(const unsigned char* body_ptr, uint16_t body_len, const CoreUtils::CommonHeader& header);
    void handle_i081(const unsigned char* body_ptr, uint16_t body_len, const CoreUtils::CommonHeader& header, const std::string& product_id);
    void handle_i083(const unsigned char* body_ptr, uint16_t body_len, const CoreUtils::CommonHeader& header, const std::string& product_id);
    void handle_i001(const CoreUtils::CommonHeader& header); // Heartbeat
    void handle_i002(const CoreUtils::CommonHeader& header); // Sequence Reset

    // Helper to get or create an order book.
    // Returns nullptr if product info (esp. decimal_locator) is not available for a new book.
    OrderBookManagement::OrderBook* get_or_create_order_book(const std::string& product_id);

    // Helper for sequence number validation
    bool is_sequence_valid(const CoreUtils::CommonHeader& header);


    // --- State Management Data Members ---
    std::map<std::string, SpecificMessageParsers::MessageI010> product_info_cache_;
    std::map<std::string, OrderBookManagement::OrderBook> order_books_;

    // Key: Channel ID, Value: Last seen Channel Sequence Number
    std::map<uint32_t, uint64_t> channel_sequences_;

    // TODO: Logger instance. For now, assume global or direct use of CoreUtils::Logger::Log().
    // std::unique_ptr<CoreUtils::Logger> logger_;

    bool initialized_ = false;
};

} // namespace Taifex
#endif // TAIFEX_SDK_H
