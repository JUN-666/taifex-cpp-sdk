#ifndef MESSAGE_I001_H
#define MESSAGE_I001_H

#include <cstdint> // For uint16_t (used in parser function signature)
// No other specific includes needed for the struct itself.
// common_header.h might be needed if we were to inspect header.getBodyLength() directly,
// but the length is passed as a parameter to the parser.

namespace SpecificMessageParsers {

/**
 * @brief Structure for I001 - Heartbeat Message.
 * @details The message body is expected to be empty. The primary purpose of parsing
 *          is to confirm this based on the body length reported in the common header.
 *          CHECK-SUM and TERMINAL-CODE, though listed in some specifications as part
 *          of the I001 layout, are typically handled by the message framing layer or
 *          a separate checksum utility, not as part of the effective message "body"
 *          data to be stored in this struct.
 */
struct MessageI001 {
    /** @brief Body is empty. This struct is a placeholder. */
    // No data members.
};

/**
 * @brief Parses the body of an I001 - Heartbeat Message.
 * @details This function primarily validates that the message body is effectively empty.
 *          An empty body can have a length of 0. If the body_length from the
 *          common header includes a checksum and terminal code (typically 3 bytes total),
 *          this is also considered an "effectively empty" data body.
 * @param body_data Pointer to the raw message body data. Can be null if body_length is 0.
 *                  It is not inspected if length is 0 or 3.
 * @param body_length The length of the message body, obtained from the common header.
 * @param out_msg Reference to the MessageI001 struct (which is empty and not modified).
 * @return True if the body_length is 0 or 3, indicating an effectively empty message.
 *         False otherwise, indicating an unexpected body length.
 */
bool parse_i001_body(const unsigned char* body_data,
                     uint16_t body_length,
                     MessageI001& out_msg);

} // namespace SpecificMessageParsers
#endif // MESSAGE_I001_H
