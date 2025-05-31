#ifndef MESSAGE_I002_H
#define MESSAGE_I002_H

#include <cstdint> // For uint16_t (used in parser function signature)
// No other specific includes needed for the struct itself.

namespace SpecificMessageParsers {

/**
 * @brief Structure for I002 - Sequence Reset Message.
 * @details The message body is expected to be empty. The parser function's main
 *          role is to verify this based on the body length from the common header.
 *          Similar to I001, any CHECK-SUM and TERMINAL-CODE are generally handled
 *          outside the scope of what's considered the storable "body" data for this message type.
 */
struct MessageI002 {
    /** @brief Body is empty. This struct serves as a placeholder. */
    // No data members.
};

/**
 * @brief Parses the body of an I002 - Sequence Reset Message.
 * @details This function primarily validates that the message body is effectively empty.
 *          A body_length of 0 is considered valid. If body_length is 3 (potentially
 *          for a checksum and terminal code), it's also treated as effectively empty
 *          from a data content perspective.
 * @param body_data Pointer to the raw message body data. Can be null if body_length is 0.
 *                  The content is not inspected by this parser if length is 0 or 3.
 * @param body_length The length of the message body, as indicated by the common header.
 * @param out_msg Reference to the MessageI002 struct (which is empty and not modified).
 * @return True if body_length is 0 or 3, indicating an effectively empty message.
 *         False for any other length, signifying an unexpected body structure.
 */
bool parse_i002_body(const unsigned char* body_data,
                     uint16_t body_length,
                     MessageI002& out_msg);

} // namespace SpecificMessageParsers
#endif // MESSAGE_I002_H
