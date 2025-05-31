// message_identifier.cpp
#include "message_identifier.h"
#include <map>
#include <utility> // For std::pair
#include <string>  // For std::to_string, std::string::substr
#include <iomanip> // For std::setw, std::setfill (alternative formatting)
#include <sstream> // For std::ostringstream (alternative formatting)


namespace CoreUtils {

// Helper function to format the numeric part of the message ID (last three digits)
// e.g., 1010 -> "010", 1100 -> "100", 70 -> "070" (if it was just 70)
// The rule is "I" + last three digits of the four-digit code.
// So if code is 1xxx, we take xxx.
std::string formatLastThreeDigits(int code_val) {
    if (code_val < 0) return "XXX"; // Should not happen with valid codes

    std::string num_str = std::to_string(code_val % 1000); // Get last three digits, e.g., 1010 % 1000 = 10
                                                       // 1100 % 1000 = 100
                                                       // 1072 % 1000 = 72
                                                       // 1024 % 1000 = 24
                                                       // 1001 % 1000 = 1
    // Pad with leading zeros if necessary to make it 3 digits long
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(3) << num_str;
    return oss.str();
}


std::string identifyMessageId(const CommonHeader& header) {
    // Key: pair<TransmissionCode, MessageKind>
    // Value: Internal numeric code (e.g., 1010, 1001)
    // Using char literals for TC and MK as they are $X(1)
    static const std::map<std::pair<unsigned char, unsigned char>, int> message_map = {
        // --- Multicast Group Common ---
        {{'0', '1'}, 1001}, // Heartbeat
        {{'0', '2'}, 1002}, // Sequence Reset

        // --- 期貨 (Futures) ---
        // TC '1'
        {{'1', '1'}, 1010}, // 商品基本資料訊息 (I010)
        {{'1', '2'}, 1030}, // 商品委託量累計訊息 (I030)
        {{'1', '3'}, 1011}, // 契約基本資料 (I011)
        {{'1', '4'}, 1050}, // 公告訊息 (I050)
        {{'1', '5'}, 1060}, // 現貨標的資訊揭示 (I060)
        {{'1', '6'}, 1120}, // 股票期貨与現貨標的對照表 (I120)
        {{'1', '7'}, 1130}, // 契約調整檔 (I130)
        {{'1', '8'}, 1064}, // 現貨標的試撮与狀態資訊 (I064)
        {{'1', 'A'}, 1012}, // 商品漲跌幅資訊 (I012)
        // TC '2'
        {{'2', '1'}, 1070}, // 收盤行情資料訊息 (I070)
        {{'2', '2'}, 1071}, // 收盤行情訊息含結算價 (I071)
        {{'2', '3'}, 1072}, // 行情訊息含結算價及未平倉合約數 (I072)
        {{'2', '4'}, 1100}, // 詢價揭示訊息 (I100)
        {{'2', 'A'}, 1081}, // 委託簿揭示訊息 (I081)
        {{'2', 'B'}, 1083}, // 委託簿快照訊息 (I083)
        {{'2', 'C'}, 1084}, // 快照更新訊息 (I084)
        {{'2', 'D'}, 1024}, // 成交價量揭示訊息 (I024)
        {{'2', 'E'}, 1025}, // 盤中最高低價揭示訊息 (I025)
        // TC '3'
        // Note: Document reference "1070 TC3/MK1" and "1070 TC2/MK1" for same description.
        {{'3', '1'}, 1070}, // 收盤行情資料訊息 (I070)
        // Note: Document reference "1140 1072" for TC3/MK3. "1140" seems to be the primary code.
        {{'3', '3'}, 1140}, // 系統訊息 (I140)
        {{'3', '4'}, 1073}, // 複式商品收盤行情資料訊息 (I073)

        // --- 選擇權 (Options) ---
        // TC '4'
        {{'4', '1'}, 1010}, // 商品基本資料訊息 (I010)
        {{'4', '2'}, 1030}, // 商品委託量累計訊息 (I030)
        {{'4', '3'}, 1011}, // 契約基本資料 (I011)
        {{'4', '4'}, 1050}, // 公告訊息 (I050)
        {{'4', '5'}, 1060}, // 現貨標的資訊揭示 (I060)
        // TC4/MK6 is 1120 in the full table (股票選擇權与現貨標的對照檔)
        {{'4', '6'}, 1120}, // Based on full table (I120)
        {{'4', '7'}, 1130}, // 契約調整檔 (I130)
        {{'4', '8'}, 1064}, // 現貨標的試撮与狀態資訊 (I064)
        {{'4', 'A'}, 1012}, // 商品漲跌幅資訊 (I012)
        // TC '5'
        {{'5', '1'}, 1070}, // 收盤行情資料訊息 (I070)
        {{'5', '2'}, 1071}, // 收盤行情訊息含結算價 (I071)
        // Note: Document reference "1072 1140" for TC5/MK3. "1072" seems primary for options context.
        {{'5', '3'}, 1072}, // 行情訊息含結算價及未平倉合約數 (I072)
        {{'5', '4'}, 1100}, // 詢價揭示訊息 (I100)
        // TC5/MK5 (1060) is for options, but already under TC4/MK5. The table implies TC5/MK5 is also 1060.
        // For this example, assume TC4/MK5 is the one for 1060. If TC5/MK5 is needed, it would be another entry.
        // TC5/MK6 (1120) is for options, but already under TC4/MK6.
        {{'5', 'A'}, 1081}, // 委託簿揭示訊息 (I081)
        {{'5', 'B'}, 1083}, // 委託簿快照訊息 (I083)
        {{'5', 'C'}, 1084}, // 快照更新訊息 (I084)
        {{'5', 'D'}, 1024}, // 成交價量揭示訊息 (I024)
        {{'5', 'E'}, 1025}  // 盤中最高低價揭示訊息 (I025)
    };

    auto it = message_map.find({header.transmission_code, header.message_kind});
    if (it != message_map.end()) {
        int code_val = it->second;
        if (code_val == 1001) return "M1001"; // Heartbeat
        if (code_val == 1002) return "M1002"; // Sequence Reset

        return "I" + formatLastThreeDigits(code_val);
    }

    return ""; // Not found
}

} // namespace CoreUtils
