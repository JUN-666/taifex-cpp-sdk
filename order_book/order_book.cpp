#include "order_book.h"
#include "messages/message_i081.h" // Required for apply_update signature
#include "messages/message_i083.h" // Required for apply_snapshot signature
// SpecificMessageParsers::MessageI010 is forward declared, not directly used in these initial methods beyond constructor param.

#include <algorithm> // For std::min

namespace OrderBookManagement {

OrderBook::OrderBook(const std::string& prod_id, uint8_t decimal_loc)
    : product_id_(prod_id),
      decimal_locator_(decimal_loc),
      last_prod_msg_seq_(0) {
    // Initialization complete via member initializers
}

// Default constructor for map usage, etc. Requires subsequent proper initialization if used.
// Or, ensure OrderBookManager always constructs with product_id and decimal_locator.
// For now, provide a basic default state.
OrderBook::OrderBook()
    : product_id_(""),
      decimal_locator_(0),
      last_prod_msg_seq_(0) {
}

void OrderBook::reset() {
    bids_.clear();
    asks_.clear();
    derived_bid_.reset();
    derived_ask_.reset();
    last_prod_msg_seq_ = 0;
    // product_id_ and decimal_locator_ remain, as they define the book's identity.
}

const std::string& OrderBook::get_product_id() const {
    return product_id_;
}

uint32_t OrderBook::get_last_prod_msg_seq() const {
    return last_prod_msg_seq_;
}

std::vector<PriceQuantityLevel> OrderBook::get_top_bids(size_t n) const {
    std::vector<PriceQuantityLevel> top_levels;
    if (n == 0) return top_levels; // Handle n=0 case explicitly
    top_levels.reserve(std::min(n, bids_.size()));
    for (const auto& pair : bids_) { // Using structured binding from C++17, ensure compiler supports if not already. CMake sets C++11, this needs C++17. Will use pair.first/pair.second for C++11.
        if (top_levels.size() >= n) {
            break;
        }
        top_levels.push_back({pair.first, pair.second});
    }
    return top_levels;
}

std::vector<PriceQuantityLevel> OrderBook::get_top_asks(size_t n) const {
    std::vector<PriceQuantityLevel> top_levels;
    if (n == 0) return top_levels; // Handle n=0 case
    top_levels.reserve(std::min(n, asks_.size()));
    for (const auto& pair : asks_) { // Using structured binding from C++17, ensure compiler supports if not already. CMake sets C++11, this needs C++17. Will use pair.first/pair.second for C++11.
        if (top_levels.size() >= n) {
            break;
        }
        top_levels.push_back({pair.first, pair.second});
    }
    return top_levels;
}

std::optional<PriceQuantityLevel> OrderBook::get_derived_bid() const {
    return derived_bid_;
}

std::optional<PriceQuantityLevel> OrderBook::get_derived_ask() const {
    return derived_ask_;
}

// --- Placeholder for methods to be implemented in next subtasks ---

// Prices from MessageI08x.md_entry_px are already scaled integers (e.g., 12345 for 123.45 if locator is 2).
// So, a scale_price helper might not be strictly needed if its input is already this scaled int64_t.
// The decimal_locator_ is stored for context or if unscaling is ever needed.
// For C++11 compatibility as per CMakeLists.txt, structured bindings [price, quantity] are avoided in loops above.
// Instead, iter->first and iter->second or pair.first and pair.second would be used. The provided code uses `pair`.

// Helper to apply sign to price magnitude
PriceType apply_sign_to_price(int64_t price_magnitude, char sign_char) {
    if (sign_char == '-') {
        // Ensure positive magnitude becomes negative. If already negative (e.g. special -999..), it stays.
        return (price_magnitude > 0) ? -price_magnitude : price_magnitude;
    }
    return price_magnitude; // Assuming '0' or other means positive
}

PriceType OrderBook::scale_price(int64_t raw_price) const {
    // Assuming raw_price from SpecificMessageParsers is already correctly scaled as per message spec.
    // This function primarily serves as a pass-through or for future adjustments if needed.
    // If the raw_price was, for example, a double like 123.45, then scaling would be:
    // int64_t scale_factor = 1;
    // for(uint8_t i = 0; i < decimal_locator_; ++i) { scale_factor *= 10; }
    // return static_cast<PriceType>(raw_price * scale_factor);
    return static_cast<PriceType>(raw_price); // Direct cast if PriceType is different from int64_t, otherwise redundant.
}


void OrderBook::apply_snapshot(const SpecificMessageParsers::MessageI083& i083_msg) {
    reset(); // Clear the book first as per specification for I083

    last_prod_msg_seq_ = i083_msg.prod_msg_seq;

    for (const auto& entry : i083_msg.md_entries) { // C++11: const SpecificMessageParsers::MdEntryI083& entry : i083_msg.md_entries
        PriceType current_price = apply_sign_to_price(entry.md_entry_px, entry.sign);
        QuantityType current_quantity = static_cast<QuantityType>(entry.md_entry_size);

        // Per spec for I083: "若該委託簿委託檔數不滿揭示深度檔數,則僅揭示委託簿內檔數。"
        // "若此委託簿內有衍生買賣價量,則接續賣邊價量後,繼續揭示最佳一檔衍生買賣價量。"
        // calculated_flag = '1' (試撮後): Prices 999999999 (market buy) / -999999999 (market sell) are possible.
        // "試撮階段無衍生委託單。" (No derived orders if calculated_flag == '1')

        switch (entry.md_entry_type) {
            case '0': // Buy
                if (current_quantity > 0) { // Only add if there's quantity
                    bids_[current_price] = current_quantity;
                } else {
                    // Quantity 0 could imply removal, but I083 is a full snapshot.
                    // If a price level from snapshot has 0 qty, it shouldn't be in the active book.
                    // However, spec for I083 usually lists active levels.
                    // If calculated_flag='1', market orders (price 999... or -999...) might have 0 quantity
                    // if they are fully matched, but the snapshot should show remaining.
                    // For simplicity, if quantity is 0, we don't add it.
                }
                break;
            case '1': // Sell
                if (current_quantity > 0) {
                    asks_[current_price] = current_quantity;
                }
                break;
            case 'E': // Derived Buy
                if (i083_msg.calculated_flag == '0') { // Derived only if not call auction
                    if (current_quantity > 0 || current_price != 0) { // Store if qty > 0 or price non-zero (as per I081 overlay logic)
                        derived_bid_ = PriceQuantityLevel{current_price, current_quantity}; // C++11: derived_bid_ = PriceQuantityLevel{current_price, current_quantity}; or make_optional
                    } else {
                        derived_bid_.reset(); // Explicitly clear if qty and price are zero
                    }
                }
                break;
            case 'F': // Derived Sell
                if (i083_msg.calculated_flag == '0') { // Derived only if not call auction
                     if (current_quantity > 0 || current_price != 0) {
                        derived_ask_ = PriceQuantityLevel{current_price, current_quantity}; // C++11: derived_ask_ = PriceQuantityLevel{current_price, current_quantity}; or make_optional
                    } else {
                        derived_ask_.reset();
                    }
                }
                break;
            default:
                // Unknown md_entry_type, log or handle error
                // For now, ignore.
                break;
        }
    }
}

void OrderBook::apply_update(const SpecificMessageParsers::MessageI081& i081_msg) {
    // It's important to process entries sequentially as per TAIFEX spec.
    // "若訊息內有兩組價量更新資訊,應先處理完第一組價量之差異更新後,
    // 再依委託簿更新的結果,繼續更新第二組價量資訊,始可獲得正確之委託簿資訊。"

    // Consider if prod_msg_seq should be checked for out-of-order updates here,
    // or if that's a higher-level (SDK) responsibility. For now, just update.
    if (i081_msg.prod_msg_seq > last_prod_msg_seq_ || last_prod_msg_seq_ == 0) { // Basic check
      last_prod_msg_seq_ = i081_msg.prod_msg_seq;
    } else if (i081_msg.prod_msg_seq < last_prod_msg_seq_) {
      // Potentially an old message, log and ignore? Or let SDK handle.
      // For now, skipping if it's an older sequence.
      // std::cerr << "Warning: Received out-of-order I081 for " << product_id_
      //           << ". Current seq: " << last_prod_msg_seq_
      //           << ", received seq: " << i081_msg.prod_msg_seq << std::endl;
      // return; // Or some other strategy
    }


    for (const auto& entry : i081_msg.md_entries) { // C++11: const SpecificMessageParsers::MdEntryI081& entry : i081_msg.md_entries
        PriceType current_price = apply_sign_to_price(entry.md_entry_px, entry.sign);
        QuantityType current_quantity = static_cast<QuantityType>(entry.md_entry_size);

        char update_action = entry.md_update_action;
        char entry_type = entry.md_entry_type;
        // uint8_t price_level = entry.md_price_level; // Not directly used by map if price is the key.
                                                       // It's for context or if book was array-based.

        switch (entry_type) {
            case '0': // Buy Side
                if (update_action == '0') { // New
                    if (current_quantity > 0) {
                        bids_[current_price] = current_quantity;
                    } else {
                        // Adding a new level with 0 quantity effectively means no level or remove if exists.
                        // For "New", it implies it wasn't there. So, adding with 0 has no effect or could be an error.
                        // Safest is to ignore if quantity is 0 for a "New" action.
                    }
                } else if (update_action == '1') { // Change
                    auto it = bids_.find(current_price);
                    if (it != bids_.end()) {
                        if (current_quantity > 0) {
                            it->second = current_quantity;
                        } else {
                            bids_.erase(it); // Quantity 0 means delete the level
                        }
                    } else {
                        // Change for a non-existent level? Could be an error or an implicit "New".
                        // Spec usually implies Change is for existing levels.
                        // If quantity > 0, could treat as New. For now, assume Change targets existing.
                         if (current_quantity > 0) { // Treat as new if it doesn't exist
                            bids_[current_price] = current_quantity;
                         }
                    }
                } else if (update_action == '2') { // Delete
                    bids_.erase(current_price);
                }
                // Overlay ('5') is not for regular bids/asks per spec section "七" & "八"
                break;

            case '1': // Sell Side
                if (update_action == '0') { // New
                    if (current_quantity > 0) {
                        asks_[current_price] = current_quantity;
                    }
                } else if (update_action == '1') { // Change
                    auto it = asks_.find(current_price);
                    if (it != asks_.end()) {
                        if (current_quantity > 0) {
                            it->second = current_quantity;
                        } else {
                            asks_.erase(it);
                        }
                    } else {
                         if (current_quantity > 0) { // Treat as new
                            asks_[current_price] = current_quantity;
                         }
                    }
                } else if (update_action == '2') { // Delete
                    asks_.erase(current_price);
                }
                // Overlay ('5') is not for regular bids/asks
                break;

            case 'E': // Derived Buy
                if (update_action == '5') { // Overlay
                    if (current_quantity > 0 || current_price != 0) { // Price can be 0 if qty is also 0 for deletion
                        derived_bid_ = PriceQuantityLevel{current_price, current_quantity}; // C++11: derived_bid_ = PriceQuantityLevel{current_price, current_quantity}; or make_optional
                    } else {
                        derived_bid_.reset(); // Both 0 means delete/clear
                    }
                }
                // Other actions (New, Change, Delete) are not typical for derived via I081 based on common usage.
                // The spec focuses on Overlay for derived.
                break;

            case 'F': // Derived Sell
                if (update_action == '5') { // Overlay
                    if (current_quantity > 0 || current_price != 0) {
                        derived_ask_ = PriceQuantityLevel{current_price, current_quantity}; // C++11: derived_ask_ = PriceQuantityLevel{current_price, current_quantity}; or make_optional
                    } else {
                        derived_ask_.reset();
                    }
                }
                break;
            default:
                // Unknown md_entry_type
                break;
        }
    }
    // Pruning to fixed depth (e.g. 5 levels) after updates if required by strict interpretation
    // of "向下調整其它價格檔位，並刪除超出委託簿揭示深度之檔位"
    // For now, map stores all levels, get_top_X provides the view.
    // If strict depth:
    // while (bids_.size() > MAX_DEPTH) { bids_.erase(std::prev(bids_.end())); } // std::map greater sorts high to low, last is lowest
    // while (asks_.size() > MAX_DEPTH) { asks_.erase(std::prev(asks_.end())); } // std::map less sorts low to high, last is highest
    // This pruning is complex because "delete" on level 2 doesn't mean level 6 becomes visible if MAX_DEPTH is 5.
    // The spec implies the view is managed by the exchange sending updates to fill the view.
    // The current std::map approach of storing all known levels is simpler and likely correct for data retention.
}

} // namespace OrderBookManagement
