#include "message_i001.h"
// No other specific includes needed unless we add logging or complex error types.

#include <cstdint> // For uint16_t
#include <iostream> // For temporary diagnostic messages

namespace SpecificMessageParsers {

/**
 * @brief Parses the body of an I001 - Heartbeat Message.
 *
 * The specification states that the message body for I001 is empty,
 * except for CHECK-SUM and TERMINAL-CODE.
 * This function confirms this based on the provided body_length.
 * The fields CHECK-SUM (1 byte) and TERMINAL-CODE (2 bytes) are listed
 * in the message format table in the reference document.
 *
 * If body_length is 0, the body is considered empty.
 * If body_length is 3, it's assumed these bytes are CHECK-SUM and TERMINAL-CODE,
 * and the effective data body is still considered empty.
 * Any other body_length is treated as unexpected.
 *
 * @param body_data Pointer to the raw message body data. Can be null if body_length is 0.
 * @param body_length The length of the message body, obtained from the common header.
 * @param out_msg Reference to the MessageI001 struct (which is empty).
 * @return true if the body is considered empty (length 0 or 3), false otherwise.
 */
bool parse_i001_body(const unsigned char* body_data,
                     uint16_t body_length,
                     MessageI001& out_msg) {
    // out_msg is an empty struct, so nothing to populate.
    // The main purpose is to validate the body_length.

    // According to the spec: "確認此訊息本體為空 (除了共用檔頭外，只有 CHECK-SUM 和 TERMINAL-CODE)"
    // (Confirm this message body is empty (besides common header, only CHECK-SUM and TERMINAL-CODE)).
    // CHECK-SUM (1 byte) and TERMINAL-CODE (2 bytes). Total length 3 bytes.

    if (body_length == 0) {
        // Body is explicitly empty.
        return true;
    } else if (body_length == 3) {
        // This implies body_length from common header includes
        // the CHECK-SUM (1 byte) and TERMINAL-CODE (2 bytes).
        // The data content part of the message is still "empty".
        // if (body_data == nullptr && body_length > 0) {
        //     std::cerr << "Warning: body_data is null but body_length is " << body_length << " for I001." << std::endl;
        //     return false;
        // }
        return true;
    } else {
        // Any other length is unexpected for an "empty" data message.
        // std::cerr << "Error: I001 message body is not effectively empty as expected. Body length: "
        //           << body_length << std::endl;
        return false;
    }
}

} // namespace SpecificMessageParsers
