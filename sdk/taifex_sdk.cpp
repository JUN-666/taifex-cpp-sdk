#include "sdk/taifex_sdk.h"

// Required includes for types used in TaifexSdk state and method implementations
#include "common_header.h"         // Removed core_utils/ prefix
#include "message_identifier.h"    // Removed core_utils/ prefix
#include "logger.h"                // Removed core_utils/ prefix
#include "checksum.h"              // Removed core_utils/ prefix
#include "string_utils.h"          // Added string_utils.h

// Specific Message Parser function headers
#include "messages/message_i010.h"
#include "messages/message_i081.h"
#include "messages/message_i083.h"
#include "messages/message_i001.h" // For parse_i001_body, etc.
#include "messages/message_i002.h"
// Other specific parser headers if needed

#include "order_book/order_book.h" // For OrderBook type

#include <iostream> // For temporary product_id extraction, remove later


namespace Taifex {

TaifexSdk::TaifexSdk() : initialized_(false) {
    // Maps (product_info_cache_, order_books_, channel_sequences_) are default constructed.
    // If a logger instance was to be owned by TaifexSdk, it would be initialized here or in initialize().
    // For now, assuming logger is globally accessible or configured elsewhere if needed by CoreUtils.
    LOG_INFO << "TaifexSdk instance created.";
}

TaifexSdk::~TaifexSdk() {
    LOG_INFO << "TaifexSdk instance destroyed.";
    // std::map members will automatically clean up their contents.
}

bool TaifexSdk::initialize(/* Potential config parameters */) {
    // Example: Configure logger if it has such an API and if TaifexSdk manages its settings.
    // CoreUtils::Logger::SetLevel(CoreUtils::LogLevel::DEBUG); // Example

    LOG_INFO << "TaifexSdk initializing...";
    // Perform any other one-time setup for the SDK.

    initialized_ = true;
    LOG_INFO << "TaifexSdk initialized successfully.";
    return true;
}

// --- Stubs for other methods to be implemented later ---

void TaifexSdk::process_message(const unsigned char* raw_message, size_t length) {
    if (!initialized_) {
        LOG_WARNING << "TaifexSdk::process_message called before initialization.";
        return;
    }
    if (!raw_message || length == 0) {
        LOG_WARNING << "TaifexSdk::process_message called with null or empty message.";
        return;
    }

    // 1. Checksum Validation (Dept A)
    // Assuming header is 19 bytes, body is body_length, then 1 byte checksum, 2 bytes terminal_code
    // Total expected length = CommonHeader::HEADER_SIZE + body_length_from_header + 1 (checksum) + 2 (term_code)
    // Checksum is calculated from byte 1 (ESC is byte 0) up to the byte before checksum.
    if (length < CoreUtils::CommonHeader::HEADER_SIZE + 1 + 2) { // Min length for header, checksum, term_code
        LOG_ERROR << "Message too short for even basic validation.";
        return;
    }

    // Temporarily parse header to get body_length for full length check and checksum position
    CoreUtils::CommonHeader temp_header;
    if (!CoreUtils::CommonHeader::parse(raw_message, length, temp_header)) {
         LOG_ERROR << "Initial header parse for validation failed (message too short for header).";
        return;
    }
    uint16_t body_len_from_header = temp_header.getBodyLength();
    size_t expected_total_length = CoreUtils::CommonHeader::HEADER_SIZE + body_len_from_header + 1 + 2;

    if (length != expected_total_length) {
        LOG_ERROR << "Message length mismatch. Expected: " +
                               std::to_string(expected_total_length) + ", Got: " + std::to_string(length);
        return;
    }

    // Checksum byte is at offset: CommonHeader::HEADER_SIZE + body_len_from_header
    unsigned char received_checksum = raw_message[CoreUtils::CommonHeader::HEADER_SIZE + body_len_from_header];
    // Data to checksum: from raw_message[1] up to raw_message[CommonHeader::HEADER_SIZE + body_len_from_header - 1]
    // Length of data to checksum = (CommonHeader::HEADER_SIZE + body_len_from_header - 1) - 1 + 1
    //                             = CommonHeader::HEADER_SIZE + body_len_from_header - 1
    // Start pointer for checksum is raw_message + 1 (skip ESC)
    // Length of data to checksum = CommonHeader::HEADER_SIZE (excluding ESC) + body_len_from_header - 1 (actual checksum byte itself)
    size_t checksum_data_length = (CoreUtils::CommonHeader::HEADER_SIZE -1) + body_len_from_header;
    std::span<const unsigned char> checksum_data_uchars(raw_message + 1, checksum_data_length);
    unsigned char calculated_checksum = CoreUtils::calculateXorChecksum(std::as_bytes(checksum_data_uchars));

    if (calculated_checksum != received_checksum) {
        LOG_ERROR << "Checksum validation failed. Calculated: " +
                               std::to_string(calculated_checksum) + ", Received: " + std::to_string(received_checksum);
        return;
    }
    LOG_DEBUG << "Checksum validation passed.";

    // 2. Parse Common Header (Dept B)
    CoreUtils::CommonHeader header; // Already have temp_header, can use it or re-parse for clarity. Let's use temp_header.
    header = temp_header; // Or re-parse: CoreUtils::CommonHeader::parse(raw_message, length, header);

    LOG_DEBUG << "CommonHeader parsed. BodyLength: " + std::to_string(header.getBodyLength()) +
                           ", ChannelID: " + std::to_string(header.getChannelId()) +
                           ", ChannelSeq: " + std::to_string(header.getChannelSeq());


    // 3. Sequence Number Validation (Basic - per Channel)
    if (!is_sequence_valid(header)) {
        // is_sequence_valid should log the details of the error.
        // Depending on strategy, might return or just flag. For now, assume it logs and we continue.
        // If strict, could return here.
    }

    // 4. Identify Message Type (Dept B)
    std::string msg_type_str = CoreUtils::identifyMessageId(header); // Changed to use existing fn
    // CoreUtils::Logger::Log(CoreUtils::LogLevel::INFO, "Identified MessageType: " + CoreUtils::messageTypeToString(msg_type)); // Commented out due to missing fn and type
    LOG_INFO << "Identified Message ID: " + msg_type_str;


    // 5. Dispatch to Body Parser/Handler (Dept E)
    const unsigned char* body_ptr = raw_message + CoreUtils::CommonHeader::HEADER_SIZE;
    uint16_t body_length = header.getBodyLength();

    // For messages like I081, I083, I010, product ID is in the body.
    // We need to extract it before calling get_or_create_order_book in some cases.
    // This is a bit tricky as product_id is needed by the handle_XXX methods.
    // For now, dispatch_message_body will call the specific parser, then the handler.
    // The handler will then extract product_id if needed.

    // A temporary way to get product_id for relevant messages before full parsing in handlers.
    // This is simplified. Ideally, handlers extract this.
    // std::string product_id_for_handler; // Commented out as msg_type logic is disabled
    // if (msg_type == CoreUtils::MessageType::I010_PRODUCT_BASIC_DATA) { // Commented out
        // PROD-ID-S is X(10) at offset 0 of I010 body
        // if (body_length >= 10) {
        //     product_id_for_handler.assign(reinterpret_cast<const char*>(body_ptr), 10);
        // }
    // } else if (msg_type == CoreUtils::MessageType::I081_ORDER_BOOK_UPDATE || msg_type == CoreUtils::MessageType::I083_ORDER_BOOK_SNAPSHOT) { // Commented out
        // PROD-ID is X(20) at offset 0 of I081/I083 body
        // if (body_length >= 20) {
        //     product_id_for_handler.assign(reinterpret_cast<const char*>(body_ptr), 20);
        // }
    // }
    // For I001, I002, product_id is not in the body.

    // Commenting out dispatch logic due to msg_type_str being a string and MessageType enum missing
    // if (msg_type_str == "I010") { // Example, actual string values from identifyMessageId not known here
    //     // PROD-ID-S is X(10) at offset 0 of I010 body
    //     if (body_length >= 10) {
    //         product_id_for_handler.assign(reinterpret_cast<const char*>(body_ptr), 10);
    //     }
    // } else if (msg_type_str == "I081" || msg_type_str == "I083") { // Example
    //     // PROD-ID is X(20) at offset 0 of I081/I083 body
    //     if (body_length >= 20) {
    //         product_id_for_handler.assign(reinterpret_cast<const char*>(body_ptr), 20);
    //     }
    // }
    // dispatch_message_body(body_ptr, body_length, msg_type_str, header, product_id_for_handler); // msg_type_str is std::string
    LOG_WARNING << "Message dispatch logic currently commented out due to interface mismatches (MessageType enum).";
}


// ... (get_order_book, get_product_info remain as previously implemented) ...


// Specific Message Parser function headers
#include "messages/message_i010.h" // Includes SpecificMessageParsers::parse_i010_body
#include "messages/message_i081.h" // Includes SpecificMessageParsers::parse_i081_body
#include "messages/message_i083.h" // Includes SpecificMessageParsers::parse_i083_body
#include "messages/message_i001.h" // Includes SpecificMessageParsers::parse_i001_body
#include "messages/message_i002.h" // Includes SpecificMessageParsers::parse_i002_body

#include "order_book/order_book.h"
#include <string_view> // For C++17, if used. For C++11, use const std::string& or substrings.
                       // Sticking to std::string for keys for C++11 compatibility.


// Note: process_message's temporary product_id extraction will be superseded by logic in handlers.
// The dispatch_message_body's temp_product_id parameter will also be removed or ignored.

// --- Private Helper Method Implementations for State Management ---

// Helper to extract the base product ID (PROD-ID-S, typically 10 chars or less if part of a complex ID)
// This is a simplified rule; a robust solution might need more context or better defined rules from spec.
static std::string get_base_prod_id_for_i010_lookup(const std::string& prod_id_from_message) {
    // If prod_id_from_message is already 10 chars (like PROD-ID-S), use it.
    // If it's longer (e.g., 20 chars for complex products like "TXFF3/I3"),
    // try to extract the first part.
    // A common pattern for spreads is "LEG1/LEG2". We'd want "LEG1".
    // Or if it's a 20-char ID that is somehow directly mapped to an I010.
    // For now, if > 10 chars, assume it might be a complex product ID where the
    // I010 info is tied to the first leg before a '/' or just the first 10 chars.
    // This is a heuristic and might need to be very specific based on actual product ID formats.
    size_t slash_pos = prod_id_from_message.find('/');
    if (slash_pos != std::string::npos) {
        return prod_id_from_message.substr(0, slash_pos);
    }

    if (prod_id_from_message.length() > 10) {
        // Attempt to trim to a potential PROD-ID-S, e.g. if it's a 20 char ID like "TXO202309C18000" (no slash)
        // vs "TXF202309" (PROD-ID-S). The I010 will be for "TXF" part or similar.
        // This is highly dependent on actual product ID structure rules not fully detailed here.
        // A simple approach: if it's a known complex product pattern, extract.
        // If not, and we need an I010, we might be stuck unless I010 is also sent for the 20-char ID.
        // For now, let's assume if it's a 20 char ID, we use the first 10 chars as a candidate for I010 lookup.
        // This is a simplification.
        // A more robust system might involve a ProductMaster that maps complex IDs to their I010-providing base IDs.
        std::string potential_base = prod_id_from_message.substr(0, 10);
        // Trim trailing spaces if any, as PROD-ID-S is X(10) but actual ID might be shorter.
        // CoreUtils::trim(potential_base); // Assuming CoreUtils::trim exists // Commented out
        return potential_base;
    }
    return prod_id_from_message; // Assume it's already a PROD-ID-S or compatible
}

OrderBookManagement::OrderBook* Taifex::TaifexSdk::get_or_create_order_book(const std::string& product_id_from_message_body) { // Added Taifex::
    auto it_ob = order_books_.find(product_id_from_message_body);
    if (it_ob != order_books_.end()) {
        return &it_ob->second;
    }

    // Order book not found, try to create it.
    // Need to find its I010 data for decimal_locator.
    std::string base_prod_id_for_i010 = get_base_prod_id_for_i010_lookup(product_id_from_message_body); // Now calls the static function defined above

    auto it_info = product_info_cache_.find(base_prod_id_for_i010);
    if (it_info != product_info_cache_.end()) {
        const auto& product_info = it_info->second;
        LOG_INFO << "Creating new OrderBook for PROD-ID: " + product_id_from_message_body +
                               " using I010 from PROD-ID-S: " + base_prod_id_for_i010 +
                               " with DecimalLocator: " + std::to_string(product_info.decimal_locator);

        // Use emplace to construct in place if possible, or insert.
        // Using product_id_from_message_body (which can be 10 or 20 char) as key for order_books_ map.
        auto result = order_books_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(product_id_from_message_body),
            std::forward_as_tuple(product_id_from_message_body, product_info.decimal_locator)
        );
        return &result.first->second; // Pointer to the newly created OrderBook
    } else {
        LOG_WARNING << "No I010 product info found for PROD-ID-S: " +
                               base_prod_id_for_i010 + " (derived from: " + product_id_from_message_body +
                               "). Cannot create OrderBook.";
        return nullptr;
    }
}

// Moved handle_i010, handle_i081, etc. after get_or_create_order_book and get_base_prod_id_for_i010_lookup
// to ensure functions are defined before use or declared appropriately.
// The actual order of these handler functions (handle_i010, handle_i081, etc.) among themselves doesn't matter
// as they are methods of TaifexSdk and called from dispatch_message_body (which is currently commented out).

void Taifex::TaifexSdk::handle_i010(const unsigned char* body_ptr, uint16_t body_len, const CoreUtils::CommonHeader& header) { // Added Taifex::
    SpecificMessageParsers::MessageI010 i010_msg;
    if (SpecificMessageParsers::parse_i010_body(body_ptr, body_len, i010_msg)) {
        std::string prod_id_s = i010_msg.prod_id_s;
        // CoreUtils::trim(prod_id_s); // Ensure key is clean // Commented out

        LOG_INFO << "Parsed I010 for PROD-ID-S: " + prod_id_s +
                               ", DecLoc: " + std::to_string(i010_msg.decimal_locator);
        product_info_cache_[prod_id_s] = i010_msg;

        // If an order book for this product (or a complex one deriving from it) exists
        // but was created before I010 arrived (e.g. if it used a default decimal_locator),
        // it might need to be updated or re-initialized.
        // For now, get_or_create_order_book handles creation with the locator when first needed.
        // If I010 arrives *after* an I081/I083 for a product that used a default/wrong locator,
        // that's a harder problem (re-scaling existing book or erroring).
        // Current design: OrderBook is created *with* the locator. If I010 not present, OB not made.
    } else {
        LOG_ERROR << "Failed to parse I010 body.";
    }
}


void Taifex::TaifexSdk::handle_i081(const unsigned char* body_ptr, uint16_t body_len, const CoreUtils::CommonHeader& header, const std::string& /*temp_product_id_from_dispatch*/) { // Added Taifex::
    SpecificMessageParsers::MessageI081 i081_msg;
    if (SpecificMessageParsers::parse_i081_body(body_ptr, body_len, i081_msg)) {
        std::string current_prod_id = i081_msg.prod_id;
        // CoreUtils::trim(current_prod_id); // Commented out

        LOG_INFO << "Parsed I081 for PROD-ID: " + current_prod_id +
                               ", MsgSeq: " + std::to_string(i081_msg.prod_msg_seq) +
                               ", Entries: " + std::to_string(i081_msg.no_md_entries);

        OrderBookManagement::OrderBook* ob = get_or_create_order_book(current_prod_id);
        if (ob) {
            ob->apply_update(i081_msg);
        } else {
            LOG_ERROR << "Failed to get/create OrderBook for PROD-ID: " + current_prod_id + " for I081. Message unprocessed.";
            // TODO: Strategy for messages for products without I010? Queue them? Discard?
        }
    } else {
        LOG_ERROR << "Failed to parse I081 body.";
    }
}

void Taifex::TaifexSdk::handle_i083(const unsigned char* body_ptr, uint16_t body_len, const CoreUtils::CommonHeader& header, const std::string& /*temp_product_id_from_dispatch*/) { // Added Taifex::
    SpecificMessageParsers::MessageI083 i083_msg;
    if (SpecificMessageParsers::parse_i083_body(body_ptr, body_len, i083_msg)) {
        std::string current_prod_id = i083_msg.prod_id;
        // CoreUtils::trim(current_prod_id); // Commented out

        LOG_INFO << "Parsed I083 for PROD-ID: " + current_prod_id +
                               ", MsgSeq: " + std::to_string(i083_msg.prod_msg_seq) +
                               ", Entries: " + std::to_string(i083_msg.no_md_entries);

        OrderBookManagement::OrderBook* ob = get_or_create_order_book(current_prod_id);
        if (ob) {
            ob->apply_snapshot(i083_msg);
        } else {
            LOG_ERROR << "Failed to get/create OrderBook for PROD-ID: " + current_prod_id + " for I083. Message unprocessed.";
        }
    } else {
        LOG_ERROR << "Failed to parse I083 body.";
    }
}

void Taifex::TaifexSdk::handle_i001(const CoreUtils::CommonHeader& header) { // Added Taifex::
    LOG_INFO << "Processing Heartbeat I001. Channel: " + std::to_string(header.getChannelId()) + ", Seq: " + std::to_string(header.getChannelSeq());
    // No specific state change other than sequence number already handled by is_sequence_valid
}

void Taifex::TaifexSdk::handle_i002(const CoreUtils::CommonHeader& header) { // Added Taifex::
    // I002: "若該CHANNEL屬即時行情群組則須清空各商品委託簿,並重置該傳輸群組之群組序號,同時重置各商品行情訊息流水序號"
    LOG_INFO << "Processing Sequence Reset I002 for Channel: " + std::to_string(header.getChannelId());

    // For now, reset ALL order books. A more granular approach might be needed if OrderBook
    // objects were associated with specific channels, or if the I002 implies a global reset.
    // The spec says "清空各商品委託簿" (clear all products' order books) for that channel.
    // If we don't map books to channels here, we reset all.
    for (auto& pair_ob : order_books_) { // C++11: auto& pair_ob : order_books_
        LOG_DEBUG << "Resetting OrderBook for PROD-ID: " + pair_ob.first + " due to I002.";
        pair_ob.second.reset();
    }

    // Reset channel sequence number for this specific channel
    uint32_t channel_id = header.getChannelId();
    channel_sequences_[channel_id] = 0; // Reset to 0, next expected will be 1 (or as per actual start seq)
                                        // Or, remove the entry: channel_sequences_.erase(channel_id);
                                        // Setting to 0 seems safer if first seq is > 0.
    LOG_INFO << "Channel sequence for Channel " + std::to_string(channel_id) + " reset.";

    // "同時重置各商品行情訊息流水序號" - this is handled by OrderBook::reset() which sets its last_prod_msg_seq_ to 0.
}

bool Taifex::TaifexSdk::is_sequence_valid(const CoreUtils::CommonHeader& header) { // Added Taifex::
    uint32_t channel_id = header.getChannelId();
    uint64_t current_channel_seq = header.getChannelSeq();

    auto it = channel_sequences_.find(channel_id);
    if (it == channel_sequences_.end()) {
        // First message for this channel
        LOG_INFO << "First message for Channel " + std::to_string(channel_id) +
                               ", received Seq: " + std::to_string(current_channel_seq) + ". Storing.";
        channel_sequences_[channel_id] = current_channel_seq;
        return true;
    }

    uint64_t last_known_seq = it->second;
    if (current_channel_seq == last_known_seq + 1) {
        // Expected sequence
        it->second = current_channel_seq;
        return true;
    } else if (current_channel_seq <= last_known_seq) {
        LOG_WARNING << "Out-of-order/replay Channel Seq for Channel " + std::to_string(channel_id) +
                               ". Expected > " + std::to_string(last_known_seq) + ", Got: " + std::to_string(current_channel_seq);
        return false; // Or a special status
    } else { // current_channel_seq > last_known_seq + 1
        LOG_WARNING << "Gap detected in Channel Seq for Channel " + std::to_string(channel_id) +
                               ". Expected: " + std::to_string(last_known_seq + 1) + ", Got: " + std::to_string(current_channel_seq);
        it->second = current_channel_seq; // Update to current to resync past the gap
        return false; // Or a special status indicating gap
    }
}

} // namespace Taifex
