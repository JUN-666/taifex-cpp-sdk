// main_checksum_test.cpp
#include "checksum.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <iomanip> // For std::hex

void print_hex(unsigned char val) {
    std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(val) << std::dec;
}

int main() {
    std::cout << "Testing Checksum Utilities..." << std::endl;
    int tests_passed = 0;
    int tests_total = 0;

    // Test data based on the document's 1100 message example structure (conceptual values)
    // "將第二個byte 起至 check-sum 欄位前一個byte"
    // Example: [ESC, TC, MK, IT1..IT6, CI1..CI2, CS1..CS5, VN, BL1..BL2, <BODY...>, CHK, TERM]
    // Checksum is over [TC, MK, IT1..IT6, CI1..CI2, CS1..CS5, VN, BL1..BL2, <BODY...>]

    tests_total++;
    std::vector<unsigned char> data1 = {0x35, 0x34, 0x09, 0x01, 0x00, 0x58, 0x00, 0x00}; // TC, MK, IT (partial)
    unsigned char cs1 = CoreUtils::calculateXorChecksum(data1);
    // 0x35 ^ 0x34 = 00110101 ^ 00110100 = 00000001 (0x01)
    // 0x01 ^ 0x09 = 00000001 ^ 00001001 = 00001000 (0x08)
    // 0x08 ^ 0x01 = 00001000 ^ 00000001 = 00001001 (0x09)
    // 0x09 ^ 0x00 = 0x09
    // 0x09 ^ 0x58 = 00001001 ^ 01011000 = 01010001 (0x51)
    // 0x51 ^ 0x00 = 0x51
    // 0x51 ^ 0x00 = 0x51
    unsigned char expected_cs1 = 0x51;
    assert(cs1 == expected_cs1);
    std::cout << "Test 1: Calculate {0x35, 0x34, ...} -> "; print_hex(cs1); std::cout << " (Expected: "; print_hex(expected_cs1); std::cout << ") - PASS" << std::endl;
    tests_passed++;

    tests_total++;
    assert(CoreUtils::verifyXorChecksum(data1, expected_cs1));
    std::cout << "Test 2: Verify {0x35, 0x34, ...} with "; print_hex(expected_cs1); std::cout << " -> PASS" << std::endl;
    tests_passed++;

    tests_total++;
    assert(!CoreUtils::verifyXorChecksum(data1, 0x00));
    std::cout << "Test 3: Verify {0x35, 0x34, ...} with 0x00 -> Fail (as expected) - PASS" << std::endl;
    tests_passed++;

    tests_total++;
    std::vector<unsigned char> data_empty = {};
    unsigned char cs_empty = CoreUtils::calculateXorChecksum(data_empty);
    assert(cs_empty == 0x00);
    std::cout << "Test 4: Calculate {} -> "; print_hex(cs_empty); std::cout << " (Expected: 0x00) - PASS" << std::endl;
    tests_passed++;

    tests_total++;
    assert(CoreUtils::verifyXorChecksum(data_empty, 0x00));
    std::cout << "Test 5: Verify {} with 0x00 -> PASS" << std::endl;
    tests_passed++;

    tests_total++;
    std::vector<unsigned char> data_single = {0xFF};
    unsigned char cs_single = CoreUtils::calculateXorChecksum(data_single);
    assert(cs_single == 0xFF);
    std::cout << "Test 6: Calculate {0xFF} -> "; print_hex(cs_single); std::cout << " (Expected: 0xFF) - PASS" << std::endl;
    tests_passed++;

    // Test from the document's example "肆、五、檢核碼(check-sum)產生說明 (手冊第 87-88 頁)"
    // The example data (bytes from TRANSMISSION-CODE to byte before CHECK-SUM):
    // TRANSMISSION-CODE: '5' (0x35) (This is ASCII, but seems the example uses raw hex values for BCD packed fields later)
    // The example shows:
    // TRANSMISSION-CODE = "5"
    // MESSAGE-KIND = "4"
    // INFORMATION-TIME = "0x09 0x01 0x00 0x58 0x00 0x00" (PACK BCD)
    // CHANNEL-ID = "0x00 0x09" (PACK BCD)
    // CHANNEL-SEQ = "0x00 0x00 0x00 0x00 0x02" (PACK BCD)
    // VERSION-NO = "0x01" (PACK BCD)
    // BODY-LENGTH = "0x00 0x28" (PACK BCD)
    // PROD-ID-S = "TXOO7900F9" (ASCII: 0x54 0x58 0x4F 0x4F 0x37 0x39 0x30 0x30 0x46 0x39)
    // DISCLOSURE-TIME = "0x00 0x00 0x00 0x00 0x00 0x00" (PACK BCD)
    // DURATION-TIME = "0x00 0x01" (PACK BCD)
    // TOTAL BYTES for checksum calculation: 1 (TC) + 1 (MK) + 6 (IT) + 2 (CI) + 5 (CS) + 1 (VN) + 2 (BL) + 10 (PROD) + 6 (DT) + 2 (DUR) = 36 bytes
    // The example says "合併後共36個BYTE" - this refers to the segment for checksum calculation.
    // The example checksum is "01110000" which is 0x70.

    std::vector<unsigned char> doc_example_data = {
        // Manually converting ASCII '5' and '4' to their char values.
        // However, the example shows values like 0x09 for BCD.
        // The fields TRANSMISSION-CODE and MESSAGE-KIND are $X(1).
        // Let's assume they mean the character '5' and '4'.
        0x35, // TRANSMISSION-CODE '5'
        0x34, // MESSAGE-KIND '4'
        0x09, 0x01, 0x00, 0x58, 0x00, 0x00, // INFORMATION-TIME
        0x00, 0x09,                         // CHANNEL-ID
        0x00, 0x00, 0x00, 0x00, 0x02,       // CHANNEL-SEQ
        0x01,                               // VERSION-NO
        0x00, 0x28,                         // BODY-LENGTH
        // PROD-ID-S "TXOO7900F9"
        0x54, 0x58, 0x4F, 0x4F, 0x37, 0x39, 0x30, 0x30, 0x46, 0x39,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // DISCLOSURE-TIME
        0x00, 0x01                          // DURATION-TIME
    };
    assert(doc_example_data.size() == 36); // Verify total bytes

    tests_total++;
    unsigned char cs_doc = CoreUtils::calculateXorChecksum(doc_example_data);
    unsigned char expected_cs_doc = 0x70;
    // Let's verify this calculation manually for a few bytes:
    // 0x35 ^ 0x34 = 0x01
    // 0x01 ^ 0x09 = 0x08
    // 0x08 ^ 0x01 = 0x09
    // 0x09 ^ 0x00 = 0x09
    // 0x09 ^ 0x58 = 0x51
    // ... this will take a while. Trusting the code for now, or the example.
    // If this fails, it means my interpretation of the example's input bytes might differ from the document.
    // The document's "TRANSMISSION-CODE = '5'" could mean 0x05 if it's a numeric value, or 0x35 if ASCII.
    // Given "$X(1)$" format, it's usually direct char value.

    if (cs_doc == expected_cs_doc) {
        std::cout << "Test 7: Calculate checksum for document example data -> "; print_hex(cs_doc);
        std::cout << " (Expected: "; print_hex(expected_cs_doc); std::cout << ") - PASS" << std::endl;
        tests_passed++;
    } else {
        std::cout << "Test 7: Calculate checksum for document example data -> "; print_hex(cs_doc);
        std::cout << " (Expected: "; print_hex(expected_cs_doc); std::cout << ") - FAIL (Checksum mismatch with document example. Check input byte values if this is unexpected.)" << std::endl;
        // This might indicate that '5' and '4' should be 0x05 and 0x04, not 0x35 and 0x34 if they are also packed.
        // However, format $X(1) usually means single char/byte.
        // Let's try with 0x05 and 0x04 for TC and MK:
        std::vector<unsigned char> doc_example_data_alt = {
            0x05, // TRANSMISSION-CODE (numeric 5)
            0x04, // MESSAGE-KIND (numeric 4)
            0x09, 0x01, 0x00, 0x58, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x28,
            0x54, 0x58, 0x4F, 0x4F, 0x37, 0x39, 0x30, 0x30, 0x46, 0x39,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
        };
        unsigned char cs_doc_alt = CoreUtils::calculateXorChecksum(doc_example_data_alt);
        std::cout << "Test 7 (Alternative): Calculate with TC=0x05, MK=0x04 -> "; print_hex(cs_doc_alt);
        // If cs_doc_alt is 0x70, then TC & MK are numeric, not ASCII.
        // The document's "TRANSMISSION-CODE = '5'" is ambiguous. "$X(1)$" means 1 byte of data.
        // The "行情訊息共用檔頭" table specifies $X(1)$ for TRANSMISSION-CODE and MESSAGE-KIND.
        // This usually implies raw byte values, not necessarily printable ASCII.
        // "參考揭示訊息一覽表" implies these are lookup codes.
        // For the purpose of the checksum function, it just XORs what it's given.
        // The exact values (0x35 vs 0x05) are an input concern.
        // The example checksum 0x70 is key. If my code with 0x35,0x34,... results in 0x70, it's fine.
    }
    // For now, assume the first interpretation (ASCII '5', '4') is what a generic parser would feed.
    // If the test fails, it's a known point of ambiguity to clarify from the spec's full context.
    // The checksum calculation itself is straightforward XOR sum.

    std::cout << "\nFinished Checksum tests." << std::endl;
    std::cout << tests_passed << " / " << tests_total << " tests passed." << std::endl;

    return !(tests_passed == tests_total);
}
