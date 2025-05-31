#ifndef MESSAGE_I010_H
#define MESSAGE_I010_H

#include <string>
#include <cstdint> // For int64_t, uint8_t

// Forward declare CommonHeader if it's only needed for a parameter type by pointer/reference
// Or include "common_header.h" if full definition is needed or preferred.
// For now, assume body_length and body_data are sufficient, CommonHeader not directly needed by parser signature.
// struct CoreUtils::CommonHeader; // Forward declaration

namespace SpecificMessageParsers {

/**
 * @brief Structure for I010 - Product Basic Data Message.
 * @details Stores the parsed fields from an I010 message. All BCD numeric fields
 *          are stored as integers without decimal point adjustment. The actual value
 *          needs to be calculated by the consumer using DECIMAL-LOCATOR and
 *          STRIKE-PRICE-DECIMAL-LOCATOR provided within this structure.
 */
struct MessageI010 {
    /** @brief X(10) 商品代號 (Product ID Suffix/Short Code). Typically part of a full product identifier. */
    std::string prod_id_s;
    /** @brief 9(9) L5 參考價 PACK BCD (Reference Price). Stored as a scaled integer. Use decimal_locator to find the decimal point position. */
    int64_t     reference_price;
    /** @brief X(1) 契約種類 (Product Kind). E.g., I:Index, S:Stock, F:Futures, O:Options. */
    char        prod_kind;
    /** @brief 9(1) L1 價格欄位小數位數 PACK BCD (Decimal Locator for prices). Indicates the number of decimal places for price fields like reference_price. */
    uint8_t     decimal_locator;
    /** @brief 9(1) L1 選擇權商品代號之履約價格小數位數 PACK BCD (Strike Price Decimal Locator). Indicates decimal places for option strike prices. */
    uint8_t     strike_price_decimal_locator;
    /** @brief 9(8) L4 上市日期(YYYYMMDD) PACK BCD (Begin Date/Listing Date). Format YYYYMMDD. */
    std::string begin_date;
    /** @brief 9(8) L4 下市日期(YYYYMMDD) PACK BCD (End Date/Delisting Date). Format YYYYMMDD. */
    std::string end_date;
    /** @brief 9(2) L1 流程群組 PACK BCD (Flow Group). Identifies a group of products for processing flow. */
    uint8_t     flow_group;
    /** @brief 9(8) L4 最後結算日 (YYYYMMDD) PACK BCD (Delivery Date/Final Settlement Date). Format YYYYMMDD. */
    std::string delivery_date;
    /** @brief X(1) 適用動態價格穩定 (Dynamic Banding). 'Y' (Yes) or 'N' (No). */
    char        dynamic_banding;

    // CHECK-SUM and TERMINAL-CODE are typically handled after body parsing or by a wrapper.
};

/**
 * @brief Parses the body of an I010 - Product Basic Data Message.
 * @param body_data Pointer to the raw byte data of the message body.
 * @param body_length The length of the message body, obtained from the common header.
 * @param out_msg Reference to a MessageI010 struct to be populated with parsed data.
 * @return True if parsing is successful (including correct length and valid field formats), false otherwise.
 */
bool parse_i010_body(const unsigned char* body_data,
                     uint16_t body_length,
                     MessageI010& out_msg);

} // namespace SpecificMessageParsers
#endif // MESSAGE_I010_H
