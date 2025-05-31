#include "message_i002.h"
// No other specific includes needed unless we add logging or complex error types.
// common_header.h might be needed if we were to inspect header.getBodyLength() directly,
// but the length is passed as a parameter.

#include <cstdint> // For uint16_t
#include <iostream> // For temporary diagnostic messages

namespace SpecificMessageParsers {

/**
 * @brief Parses the body of an I002 - Sequence Reset Message.
 *
 * The specification states that the message body for I002 is empty.
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
 * @param out_msg Reference to the MessageI002 struct (which is empty).
 * @return true if the body is considered empty (length 0 or 3), false otherwise.
 */
bool parse_i002_body(const unsigned char* body_data,
                     uint16_t body_length,
                     MessageI002& out_msg) {
    // out_msg is an empty struct, so nothing to populate.
    // The main purpose is to validate the body_length.

    // According to the spec: "確認此訊息本體為空" (Confirm this message body is empty).
    // The table also lists CHECK-SUM (1) and TERMINAL-CODE (2).
    // Total length of these is 3 bytes.

    if (body_length == 0) {
        // Body is explicitly empty. This is the ideal case.
        return true;
    } else if (body_length == 3) {
        // This could mean the body_length from common header includes
        // the CHECK-SUM (1 byte) and TERMINAL-CODE (2 bytes).
        // From a data content perspective, the message is still "empty".
        // No actual data fields to parse from these 3 bytes here.
        // Checksum validation and terminal code handling would be separate.
        // For now, we accept this as "effectively empty".
        // if (body_data == nullptr && body_length > 0) {
        //     std::cerr << "Warning: body_data is null but body_length is " << body_length << " for I002." << std::endl;
        //     return false; // Data pointer should be valid if length > 0
        // }
        return true;
    } else {
        // Any other length is unexpected for an "empty" message.
        // std::cerr << "Error: I002 message body is not empty as expected. Body length: "
        //           << body_length << std::endl;
        return false;
    }
}

} // namespace SpecificMessageParsers
