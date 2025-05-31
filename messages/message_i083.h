#ifndef MESSAGE_I083_H
#define MESSAGE_I083_H

#include <string>
#include <vector>
#include <cstdint> // For uint32_t, int64_t, uint8_t (uint64_t was not used)

namespace SpecificMessageParsers {

/**
 * @brief Represents a single order book entry in an I083 message.
 * @details Contains details for one level of the order book (bid or ask).
 */
struct MdEntryI083 {
    /** @brief X(1) 行情種類 (MD Entry Type). 0:Buy, 1:Sell, E:Derived Buy, F:Derived Sell. */
    char    md_entry_type;
    /** @brief X(1) 價格正負號 (Sign for Price). '0':Positive, '-':Negative. Typically '0' for standard prices. */
    char    sign;
    /** @brief 9(9) L5 行情價格 PACK BCD (MD Entry Price). Scaled integer. Interpret with I010.decimal_locator. */
    int64_t md_entry_px;
    /** @brief 9(8) L4 價格數量 PACK BCD (MD Entry Size/Quantity). */
    int64_t md_entry_size;
    /** @brief 9(2) L1 價格檔位 PACK BCD (MD Price Level). Indicates the price level (e.g., 1st best bid, 2nd best bid). */
    uint8_t md_price_level;
};

/**
 * @brief Structure for I083 - Order Book Snapshot Message.
 * @details Provides a full snapshot of the order book for a product,
 *          up to a certain depth, at a specific point in time.
 */
struct MessageI083 {
    /** @brief X(20) 商品代號 (Product ID). Full product identifier. */
    std::string prod_id;
    /** @brief 9(10) L5 商品行情訊息流水序號 PACK BCD (Product Message Sequence Number). */
    uint32_t    prod_msg_seq;
    /** @brief X(1) 試撮後剩餘委託簿註記 (Calculated Flag). 0:Order book message, 1:Calculated remaining order book message after matching. */
    char        calculated_flag;
    /** @brief 9(2) L1 買賣價量變更巢狀迴圈數 PACK BCD (Number of MD Entries). Count of entries in md_entries vector. */
    uint8_t     no_md_entries;

    /** @brief Repeating group of order book entries based on no_md_entries. */
    std::vector<MdEntryI083> md_entries;

    // CHECK-SUM and TERMINAL-CODE are typically handled after body parsing.
};

/**
 * @brief Parses the body of an I083 - Order Book Snapshot Message.
 * @param body_data Pointer to the raw byte data of the message body.
 * @param body_length The length of the message body, obtained from the common header.
 * @param out_msg Reference to a MessageI083 struct to be populated.
 * @return True if parsing is successful, false otherwise (e.g., insufficient length, conversion error, inconsistent entry count).
 */
bool parse_i083_body(const unsigned char* body_data,
                     uint16_t body_length,
                     MessageI083& out_msg);

} // namespace SpecificMessageParsers
#endif // MESSAGE_I083_H
