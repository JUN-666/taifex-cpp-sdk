// message_identifier.h
#ifndef MESSAGE_IDENTIFIER_H
#define MESSAGE_IDENTIFIER_H

#include <string>
#include "common_header.h" // For CoreUtils::CommonHeader

namespace CoreUtils {

/**
 * @brief Identifies the message ID string based on the transmission code and message kind
 *        from the provided CommonHeader.
 *
 * The mapping is based on "逐筆行情資訊傳輸作業手冊(V1.9.0).pdf", 肆、二、揭示訊息一覽表.
 * Message IDs generally follow an "I" + last three digits of the numeric code (e.g., 1010 -> "I010").
 * Special codes like 1001 (Heartbeat) are mapped to "M1001".
 *
 * @param header The parsed CommonHeader object.
 * @return The identified message ID string (e.g., "I010", "M1001").
 *         Returns an empty string if the combination is not recognized.
 */
std::string identifyMessageId(const CommonHeader& header);

} // namespace CoreUtils

#endif // MESSAGE_IDENTIFIER_H
