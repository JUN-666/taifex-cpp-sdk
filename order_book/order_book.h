#ifndef ORDER_BOOK_H
#define ORDER_BOOK_H

#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <functional> // For std::greater, std::less
#include <optional>   // For potentially absent derived quotes if a more complex struct is used

// Forward declare message structs from Department C that will be used by OrderBook methods.
// This avoids including the full message headers in order_book.h if only references/pointers are used in method signatures.
// However, for methods taking them by const reference, full inclusion is often needed.
// For now, let's assume we'll include them in order_book.cpp and forward declare here if possible,
// or include them here if method signatures require full types (e.g. const MessageI083&).
// For initial structure, forward declaration is fine for members, but methods will dictate includes.
namespace SpecificMessageParsers {
    struct MessageI010; // Needed for decimal locators
    struct MessageI081;
    struct MessageI083;
}

namespace OrderBookManagement {

// Define PriceType and QuantityType for clarity and potential future changes.
using PriceType = int64_t;    // Scaled integer representation of price
using QuantityType = uint64_t;  // Order quantity

/**
 * @brief Represents a price and quantity pair, used for derived quotes.
 */
struct PriceQuantityLevel {
    PriceType price;
    QuantityType quantity;
    // Add a generation counter or timestamp if needed for tie-breaking or staleness, not required by current spec for OrderBook itself.
};


class OrderBook {
public:
    /**
     * @brief Constructs an OrderBook for a specific product.
     * @param prod_id The product identifier for this order book.
     * @param decimal_loc The decimal locator for this product's prices, taken from I010.
     *                    This is crucial for interpreting incoming prices correctly.
     */
    OrderBook(const std::string& prod_id, uint8_t decimal_loc);
    OrderBook(); // Default constructor, perhaps for use in maps, then initialize separately. Consider if needed.

    /**
     * @brief Rebuilds the order book from a snapshot message (I083).
     * Clears existing book data before applying the snapshot.
     * @param i083_msg The parsed I083 message content.
     *                 Prices in i083_msg are raw; they will be scaled using the stored decimal_locator.
     */
    void apply_snapshot(const SpecificMessageParsers::MessageI083& i083_msg);

    /**
     * @brief Updates the order book from a differential update message (I081).
     * Applies changes sequentially based on MD-UPDATE-ACTION.
     * @param i081_msg The parsed I081 message content.
     *                 Prices in i081_msg are raw; they will be scaled using the stored decimal_locator.
     */
    void apply_update(const SpecificMessageParsers::MessageI081& i081_msg);

    /**
     * @brief Resets the order book upon receiving a Sequence Reset message (I002).
     * Clears all bids, asks, derived quotes, and resets the last_prod_msg_seq.
     */
    void reset();

    /**
     * @brief Gets the product ID associated with this order book.
     */
    const std::string& get_product_id() const;

    /**
     * @brief Gets the last processed product message sequence number for this order book.
     */
    uint32_t get_last_prod_msg_seq() const;

    /**
     * @brief Retrieves the top N bid levels.
     * @param n The number of levels to retrieve.
     * @return A vector of PriceQuantityLevel, ordered from best (highest) price.
     */
    std::vector<PriceQuantityLevel> get_top_bids(size_t n) const;

    /**
     * @brief Retrieves the top N ask levels.
     * @param n The number of levels to retrieve.
     * @return A vector of PriceQuantityLevel, ordered from best (lowest) price.
     */
    std::vector<PriceQuantityLevel> get_top_asks(size_t n) const;

    /**
     * @brief Retrieves the derived bid quote.
     * @return An std::optional containing PriceQuantityLevel if derived bid is present, else std::nullopt.
     */
    std::optional<PriceQuantityLevel> get_derived_bid() const;

    /**
     * @brief Retrieves the derived ask quote.
     * @return An std::optional containing PriceQuantityLevel if derived ask is present, else std::nullopt.
     */
    std::optional<PriceQuantityLevel> get_derived_ask() const;


private:
    // Helper function to scale raw prices from messages using the product's decimal_locator.
    PriceType scale_price(int64_t raw_price) const;
    // Helper function to unscale prices if needed for external representation (not typically stored unscaled).
    // int64_t unscale_price(PriceType scaled_price) const; // Example, might not be needed internally.

    std::string product_id_;
    uint8_t decimal_locator_; // Stores the decimal locator for this product.
    uint32_t last_prod_msg_seq_;

    // Bids: Highest price first
    std::map<PriceType, QuantityType, std::greater<PriceType>> bids_;
    // Asks: Lowest price first
    std::map<PriceType, QuantityType, std::less<PriceType>> asks_;

    // Storage for derived quotes. Using std::optional for clarity if a quote might not be present.
    std::optional<PriceQuantityLevel> derived_bid_;
    std::optional<PriceQuantityLevel> derived_ask_;

    // Max depth for bids_/asks_ can be managed dynamically or capped if spec requires.
    // The spec examples show depth 5. The I081/I083 MD-PRICE-LEVEL suggests fixed slots.
    // This needs careful handling in apply_update based on "伍、委託簿管理方式".
    // For std::map, it stores all levels. get_top_n would then retrieve.
    // If the book should only ever contain N levels, then logic in apply_update needs to enforce this.
    // The examples (e.g. "新增委託並向下調整其它價格檔位") imply that the book maintains more than N levels internally,
    // and the "揭示深度" (disclosed depth) is what's limited to N.
    // My current std::map approach stores all levels provided. Query methods provide top N.
    // If MD-PRICE-LEVEL in I081 refers to an absolute slot that can be empty or overwritten,
    // std::map might not be the perfect fit directly without a layer or if fixed size array/vector is better.
    // However, map handles sparse levels well. Re-evaluating based on "伍、委託簿管理方式":
    // "期交所不另送出刪除訊息" when a level is pushed out of depth by a new better price.
    // This means if we add a new best bid, and the book depth is 5, the old 5th bid is implicitly dropped.
    // std::map naturally handles sorting. We'd need to prune after an insert if depth is strictly managed.
    // Or, the map stores the "known book" and queries provide "viewable depth".
    // The spec "MD-PRICE-LEVEL -> 5 第5檔" for a new order implies it can be directly inserted at a level.
    // This is where the direct map<price,qty> differs from a list of fixed levels.
    // For now, map<Price, Qty> is a good starting point. The apply_update logic will be key.
};

} // namespace OrderBookManagement
#endif // ORDER_BOOK_H
