// main_message_identifier_test.cpp
#include "message_identifier.h"
#include "common_header.h" // For CoreUtils::CommonHeader
#include "logger.h"        // For logging
#include <cassert>
#include <string> // Required for std::string

void test_identification(unsigned char tc, unsigned char mk, const std::string& expected_id) {
    CoreUtils::CommonHeader header;
    header.transmission_code = tc;
    header.message_kind = mk;
    std::string id = CoreUtils::identifyMessageId(header);
    LOG_DEBUG << "TC: '" << tc << "', MK: '" << mk << "' -> ID: '" << id << "' (Expected: '" << expected_id << "')";
    assert(id == expected_id);
}

int main() {
    CoreUtils::setLogLevel(CoreUtils::LogLevel::DEBUG);
    LOG_INFO << "Testing Message ID Identification...";

    // Multicast Group
    test_identification('0', '1', "M1001"); // Heartbeat
    test_identification('0', '2', "M1002"); // Sequence Reset

    // Futures TC '1' (Example from table)
    test_identification('1', '1', "I010"); // 1010 商品基本資料訊息
    test_identification('1', '2', "I030"); // 1030 商品委託量累計訊息
    test_identification('1', '3', "I011"); // 1011 契約基本資料
    test_identification('1', '4', "I050"); // 1050 公告訊息
    test_identification('1', '5', "I060"); // 1060 現貨標的資訊揭示
    test_identification('1', '6', "I120"); // 1120 股票期貨与現貨標的對照表
    test_identification('1', '7', "I130"); // 1130 契約調整檔
    test_identification('1', '8', "I064"); // 1064 現貨標的試撮与狀態資訊
    test_identification('1', 'A', "I012"); // 1012 商品漲跌幅資訊

    // Futures TC '2' (Example from table)
    test_identification('2', '1', "I070"); // 1070 收盤行情資料訊息
    test_identification('2', '2', "I071"); // 1071 收盤行情訊息含結算價
    test_identification('2', '3', "I072"); // 1072 行情訊息含結算價及未平倉合約數
    test_identification('2', '4', "I100"); // 1100 詢價揭示訊息
    test_identification('2', 'A', "I081"); // 1081 委託簿揭示訊息
    test_identification('2', 'B', "I083"); // 1083 委託簿快照訊息
    test_identification('2', 'C', "I084"); // 1084 快照更新訊息
    test_identification('2', 'D', "I024"); // 1024 成交價量揭示訊息
    test_identification('2', 'E', "I025"); // 1025 盤中最高低價揭示訊息

    // Futures TC '3' (Example from table)
    test_identification('3', '1', "I070"); // 1070 收盤行情資料訊息
    test_identification('3', '3', "I140"); // 1140 系統訊息
    test_identification('3', '4', "I073"); // 1073 複式商品收盤行情資料訊息

    // Options TC '4' (Example from table)
    test_identification('4', '1', "I010"); // 1010 商品基本資料訊息
    test_identification('4', '6', "I120"); // 1120 (added based on full table assumption)
    test_identification('4', 'A', "I012"); // 1012 商品漲跌幅資訊

    // Options TC '5' (Example from table)
    test_identification('5', '1', "I070"); // 1070 收盤行情資料訊息
    test_identification('5', '3', "I072"); // 1072 行情訊息含結算價及未平倉合約數
    test_identification('5', 'D', "I024"); // 1024 成交價量揭示訊息
    test_identification('5', 'E', "I025"); // 1025 盤中最高低價揭示訊息

    // Unknown combination
    test_identification('X', 'Y', "");     // Expected empty string
    test_identification('1', 'Z', "");     // Expected empty string (valid TC, invalid MK)

    LOG_INFO << "Message ID Identification tests finished successfully."; // Assuming asserts pass
    return 0;
}
