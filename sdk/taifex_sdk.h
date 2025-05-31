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
    class Logger;
    struct CommonHeader;
    enum class MessageType : uint16_t;
}

namespace SpecificMessageParsers {
    struct MessageI010;
    struct MessageI081;
    struct MessageI083;
}

namespace OrderBookManagement {
    class OrderBook;
}

namespace Taifex {

/**
 * @brief Main class for the TAIFEX Market Data SDK.
 *
 * This SDK provides functionalities to process raw TAIFEX market data messages,
 * maintain order books for various products, and manage product information.
 * It handles message parsing, validation (checksum, sequence numbers), and
 * dispatches data to update relevant internal states. Users can then query
 * the SDK for product information or reconstructed order books.
 *
 * The SDK is designed to be initialized once and then process messages sequentially.
 * It is not inherently thread-safe for concurrent calls to `process_message` or
 * state-modifying methods on the same instance without external locking.
 */
class TaifexSdk {
public:
    /**
     * @brief Default constructor for TaifexSdk.
     * Initializes internal structures but does not make the SDK fully operational
     * until `initialize()` is called.
     */
    TaifexSdk();

    /**
     * @brief Destructor for TaifexSdk.
     * Cleans up any resources managed by the SDK.
     */
    ~TaifexSdk();

    /**
     * @brief Initializes the SDK and prepares it for message processing.
     * This method should be called once before any calls to `process_message`.
     * Future versions might accept configuration parameters (e.g., logging settings,
     * specific operational modes).
     *
     * @param config_params Placeholder for potential future configuration parameters.
     * @return True if initialization is successful, false otherwise. Errors during
     *         initialization will be logged.
     */
    bool initialize(/* Potential config parameters */);

    /**
     * @brief Processes a single raw incoming TAIFEX market data message.
     *
     * This is the primary method for feeding data into the SDK. It performs several steps:
     * 1. Validates the message (checksum, overall length based on header). If invalid,
     *    an error is logged, and processing for this message stops.
     * 2. Parses the common message header to identify message type, body length, channel ID, and sequence number.
     * 3. Validates channel sequence numbers using the `CHANNEL-SEQ` from the header.
     *    Out-of-order or gap messages are logged. The SDK attempts to resynchronize on gaps.
     *    Messages deemed invalid due to sequence (e.g., replay) may be dropped.
     * 4. Dispatches to a specific message body parser based on the identified message type.
     * 5. Updates internal state:
     *    - For I010 (Product Basic Data): Product information is parsed and cached.
     *    - For I081 (Order Book Update) & I083 (Order Book Snapshot): The relevant order book is retrieved or created
     *      (if I010 data is available for it) and updated with the message content.
     *    - For I002 (Sequence Reset): Resets relevant order books and channel sequence tracking.
     *    - For I001 (Heartbeat): Primarily updates channel sequence tracking.
     *
     * Errors encountered during parsing or processing (e.g., malformed body, missing prerequisite I010 data
     * for an order book message) are logged.
     * This method is not designed to be called concurrently from multiple threads on the same `TaifexSdk` instance.
     * External synchronization is required if concurrent processing is attempted.
     *
     * @param raw_message Pointer to the raw byte array containing the full message
     *                    (including TAIFEX framing: ESC, header, body, checksum, terminal code).
     * @param length The total length of the `raw_message` in bytes.
     */
    void process_message(const unsigned char* raw_message, size_t length);

    /**
     * @brief Retrieves a read-only view of the order book for a specified product ID.
     *
     * The product ID typically corresponds to the `PROD-ID` field found in I081 or I083 messages
     * (which can be up to 20 characters).
     *
     * @param product_id The unique identifier of the product for which the order book is requested.
     * @return An `std::optional<std::reference_wrapper<const OrderBookManagement::OrderBook>>`.
     *         If the order book for the given `product_id` exists and the SDK is initialized,
     *         the optional will contain a const reference to the `OrderBook` object.
     *         Otherwise (e.g., SDK not initialized, no such product, or I010 data was missing to create the book),
     *         it returns `std::nullopt`.
     * @note The returned reference is const, providing read-only access. Its lifetime is tied to
     *       the `TaifexSdk` instance and the internal `order_books_` map. The reference
     *       remains valid as long as the `TaifexSdk` instance exists and no methods are called
     *       that might invalidate references to map elements (though `process_message` typically
     *       modifies existing `OrderBook` objects or adds new ones, not directly invalidating references
     *       to other existing books).
     */
    std::optional<std::reference_wrapper<const OrderBookManagement::OrderBook>>
    get_order_book(const std::string& product_id) const;

    /**
     * @brief Retrieves read-only product information (I010 data) for a given product ID.
     *
     * The product ID typically corresponds to the `PROD-ID-S` field from an I010 message
     * (usually up to 10 characters, after trimming).
     *
     * @param product_id The product identifier (typically the `PROD-ID-S` or a key that maps to it)
     *                   for which product information is requested.
     * @return An `std::optional<std::reference_wrapper<const SpecificMessageParsers::MessageI010>>`.
     *         If product information for the given `product_id` is found in the cache and the SDK is initialized,
     *         the optional will contain a const reference to the `MessageI010` struct.
     *         Otherwise (e.g., SDK not initialized, no I010 message processed for this ID),
     *         it returns `std::nullopt`.
     * @note The returned reference is const. Its lifetime is tied to the `TaifexSdk` instance
     *       and the internal `product_info_cache_`.
     */
    std::optional<std::reference_wrapper<const SpecificMessageParsers::MessageI010>>
    get_product_info(const std::string& product_id) const;

    // TODO: Add callback registration mechanism if needed (e.g., for specific message types or order book updates).

private:
    // --- Private Helper Methods for Message Processing ---
    void dispatch_message_body(const unsigned char* body_ptr,
                               uint16_t body_len,
                               CoreUtils::MessageType msg_type,
                               const CoreUtils::CommonHeader& header,
                               const std::string& product_id_from_header);

    void handle_i010(const unsigned char* body_ptr, uint16_t body_len, const CoreUtils::CommonHeader& header);
    void handle_i081(const unsigned char* body_ptr, uint16_t body_len, const CoreUtils::CommonHeader& header, const std::string& product_id);
    void handle_i083(const unsigned char* body_ptr, uint16_t body_len, const CoreUtils::CommonHeader& header, const std::string& product_id);
    void handle_i001(const CoreUtils::CommonHeader& header);
    void handle_i002(const CoreUtils::CommonHeader& header);

    OrderBookManagement::OrderBook* get_or_create_order_book(const std::string& product_id);
    bool is_sequence_valid(const CoreUtils::CommonHeader& header);


    // --- State Management Data Members ---
    std::map<std::string, SpecificMessageParsers::MessageI010> product_info_cache_;
    std::map<std::string, OrderBookManagement::OrderBook> order_books_;
    std::map<uint32_t, uint64_t> channel_sequences_;
    // std::unique_ptr<CoreUtils::Logger> logger_;
    bool initialized_ = false;
};

} // namespace Taifex
#endif // TAIFEX_SDK_H
