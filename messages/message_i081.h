#ifndef MESSAGE_I081_H
#define MESSAGE_I081_H

#include <string>
#include <vector>
#include <cstdint> // For uint32_t, int64_t, uint8_t

namespace SpecificMessageParsers {

/**
 * @brief Represents a single order book update instruction in an I081 message.
 * @details Describes an update to a specific level of the order book.
 */
struct MdEntryI081 {
    /** @brief X(1) 行情揭示方式 (MD Update Action). 0:New, 1:Change, 2:Delete, 5:Overlay. */
    char    md_update_action;
    /** @brief X(1) 行情種類 (MD Entry Type). 0:Buy, 1:Sell, E:Derived Buy, F:Derived Sell. */
    char    md_entry_type;
    /** @brief X(1) 價格正負號 (Sign for Price). '0':Positive, '-':Negative. */
    char    sign;
    /** @brief 9(9) L5 行情價格 PACK BCD (MD Entry Price). Scaled integer. Interpret with I010.decimal_locator. */
    int64_t md_entry_px;
    /** @brief 9(8) L4 價格數量 PACK BCD (MD Entry Size/Quantity). */
    int64_t md_entry_size;
    /** @brief 9(2) L1 價格檔位 PACK BCD (MD Price Level). Indicates the price level. */
    uint8_t md_price_level;
};

/**
 * @brief Structure for I081 - Order Book Update Message (Differential).
 * @details Provides incremental updates to the order book (bid/ask changes, deletions, new levels).
 */
struct MessageI081 {
    /** @brief X(20) 商品代號 (Product ID). Full product identifier. */
    std::string prod_id;
    /** @brief 9(10) L5 商品行情訊息流水序號 PACK BCD (Product Message Sequence Number). */
    uint32_t    prod_msg_seq;
    /** @brief 9(2) L1 買賣價量變更巢狀迴圈數 PACK BCD (Number of MD Entries). Count of entries in md_entries vector. */
    uint8_t     no_md_entries;

    /** @brief Repeating group of order book update entries based on no_md_entries. */
    std::vector<MdEntryI081> md_entries;

    // CHECK-SUM and TERMINAL-CODE are typically handled after body parsing.
};

/**
 * @brief Parses the body of an I081 - Order Book Update Message.
 * @param body_data Pointer to the raw byte data of the message body.
 * @param body_length The length of the message body, obtained from the common header.
 * @param out_msg Reference to a MessageI081 struct to be populated.
 * @return True if parsing is successful, false otherwise (e.g., insufficient length, conversion error, inconsistent entry count).
 */
bool parse_i081_body(const unsigned char* body_data,
                     uint16_t body_length,
                     MessageI081& out_msg);

} // namespace SpecificMessageParsers
#endif // MESSAGE_I081_H
