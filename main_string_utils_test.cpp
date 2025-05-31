// main_string_utils_test.cpp
#include "string_utils.h"
#include <iostream>
#include <vector>
#include <cassert>

int main() {
    std::cout << "Testing String/Byte Conversion Utilities..." << std::endl;
    int tests_passed = 0;
    int tests_total = 0;

    // Test bytesToString
    std::cout << "\n--- Testing bytesToString ---" << std::endl;
    tests_total++;
    unsigned char data1[] = {'h', 'e', '\0', 'l', 'l', 'o'};
    std::string str1 = CoreUtils::bytesToString(data1, sizeof(data1));
    assert(str1.length() == sizeof(data1));
    assert(str1[0] == 'h');
    assert(str1[2] == '\0');
    assert(str1[5] == 'o');
    std::cout << "Test 1 (bytesToString with null): Length " << str1.length() << " (Expected: " << sizeof(data1) << ") - PASS" << std::endl;
    tests_passed++;

    tests_total++;
    std::string str2 = CoreUtils::bytesToString(nullptr, 5);
    assert(str2.empty());
    std::cout << "Test 2 (bytesToString with nullptr): Empty string - PASS" << std::endl;
    tests_passed++;

    tests_total++;
    unsigned char data3[] = {'a', 'b', 'c'};
    std::string str3 = CoreUtils::bytesToString(data3, 0);
    assert(str3.empty());
    std::cout << "Test 3 (bytesToString with zero length): Empty string - PASS" << std::endl;
    tests_passed++;

    tests_total++;
    unsigned char data4[] = {0x41, 0x42, 0x00, 0x43}; // A, B, NULL, C
    std::string str4 = CoreUtils::bytesToString(data4, sizeof(data4));
    assert(str4.size() == 4);
    assert(str4 == std::string("AB\0C", 4));
    std::cout << "Test 4 (bytesToString check content): Correct content with embedded null - PASS" << std::endl;
    tests_passed++;


    // Test bytesToHexString
    std::cout << "\n--- Testing bytesToHexString ---" << std::endl;
    tests_total++;
    unsigned char hex_data1[] = {0xDE, 0xAD, 0xBE, 0xEF};
    std::string hex_str1 = CoreUtils::bytesToHexString(hex_data1, sizeof(hex_data1));
    assert(hex_str1 == "DEADBEEF");
    std::cout << "Test 5 (bytesToHexString {DE,AD,BE,EF}): " << hex_str1 << " (Expected: DEADBEEF) - PASS" << std::endl;
    tests_passed++;

    tests_total++;
    unsigned char hex_data2[] = {0x00, 0x01, 0x02, 0xFF};
    std::string hex_str2 = CoreUtils::bytesToHexString(hex_data2, sizeof(hex_data2));
    assert(hex_str2 == "000102FF");
    std::cout << "Test 6 (bytesToHexString {00,01,02,FF}): " << hex_str2 << " (Expected: 000102FF) - PASS" << std::endl;
    tests_passed++;

    tests_total++;
    std::vector<unsigned char> vec_hex_data1 = {0xCA, 0xFE, 0xBA, 0xBE};
    std::string hex_str_vec1 = CoreUtils::bytesToHexString(vec_hex_data1);
    assert(hex_str_vec1 == "CAFEBABE");
    std::cout << "Test 7 (bytesToHexString vector {CA,FE,BA,BE}): " << hex_str_vec1 << " (Expected: CAFEBABE) - PASS" << std::endl;
    tests_passed++;

    tests_total++;
    std::string hex_str_null = CoreUtils::bytesToHexString(nullptr, 5);
    assert(hex_str_null.empty());
    std::cout << "Test 8 (bytesToHexString nullptr): Empty string - PASS" << std::endl;
    tests_passed++;

    tests_total++;
    std::string hex_str_zero_len = CoreUtils::bytesToHexString(hex_data1, 0);
    assert(hex_str_zero_len.empty());
    std::cout << "Test 9 (bytesToHexString zero length): Empty string - PASS" << std::endl;
    tests_passed++;

    tests_total++;
    std::vector<unsigned char> vec_empty = {};
    std::string hex_str_vec_empty = CoreUtils::bytesToHexString(vec_empty);
    assert(hex_str_vec_empty.empty());
    std::cout << "Test 10 (bytesToHexString empty vector): Empty string - PASS" << std::endl;
    tests_passed++;


    std::cout << "\nFinished String/Byte Conversion tests." << std::endl;
    std::cout << tests_passed << " / " << tests_total << " tests passed." << std::endl;

    return !(tests_passed == tests_total);
}
