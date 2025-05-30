// main_pack_bcd_test.cpp
#include "pack_bcd.h"
#include <iostream>
#include <vector>
#include <iomanip>
#include <cassert>

void print_bytes(const std::vector<unsigned char>& bytes) {
    std::cout << "{ ";
    for (size_t i = 0; i < bytes.size(); ++i) {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
        if (i < bytes.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << " }" << std::dec << std::setfill(' ');
}

bool compare_vectors(const std::vector<unsigned char>& v1, const std::vector<unsigned char>& v2) {
    return v1 == v2;
}

int main() {
    std::cout << "Testing PACK BCD Utilities..." << std::endl;
    int tests_passed = 0;
    int tests_total = 0;

    // Test asciiToPackBcd
    std::cout << "\n--- Testing asciiToPackBcd ---" << std::endl;
    try {
        tests_total++;
        std::string s1 = "12345";
        std::vector<unsigned char> bcd1 = CoreUtils::asciiToPackBcd(s1);
        std::vector<unsigned char> exp1 = {0x01, 0x23, 0x45};
        assert(compare_vectors(bcd1, exp1));
        std::cout << s1 << " -> "; print_bytes(bcd1); std::cout << " (Expected: "; print_bytes(exp1); std::cout << ") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::string s2 = "0123";
        std::vector<unsigned char> bcd2 = CoreUtils::asciiToPackBcd(s2);
        std::vector<unsigned char> exp2 = {0x01, 0x23};
        assert(compare_vectors(bcd2, exp2));
        std::cout << s2 << " -> "; print_bytes(bcd2); std::cout << " (Expected: "; print_bytes(exp2); std::cout << ") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::string s3 = "9";
        std::vector<unsigned char> bcd3 = CoreUtils::asciiToPackBcd(s3);
        std::vector<unsigned char> exp3 = {0x09};
        assert(compare_vectors(bcd3, exp3));
        std::cout << s3 << " -> "; print_bytes(bcd3); std::cout << " (Expected: "; print_bytes(exp3); std::cout << ") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::string s4 = "000123450";
        std::vector<unsigned char> bcd4 = CoreUtils::asciiToPackBcd(s4);
        std::vector<unsigned char> exp4 = {0x00, 0x00, 0x12, 0x34, 0x50};
        assert(compare_vectors(bcd4, exp4));
        std::cout << s4 << " -> "; print_bytes(bcd4); std::cout << " (Expected: "; print_bytes(exp4); std::cout << ") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::string s5 = "0";
        std::vector<unsigned char> bcd5 = CoreUtils::asciiToPackBcd(s5);
        std::vector<unsigned char> exp5 = {0x00};
        assert(compare_vectors(bcd5, exp5));
        std::cout << s5 << " -> "; print_bytes(bcd5); std::cout << " (Expected: "; print_bytes(exp5); std::cout << ") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::string s6 = "";
        std::vector<unsigned char> bcd6 = CoreUtils::asciiToPackBcd(s6);
        std::vector<unsigned char> exp6 = {};
        assert(compare_vectors(bcd6, exp6));
        std::cout << "\"\"" << " -> "; print_bytes(bcd6); std::cout << " (Expected: "; print_bytes(exp6); std::cout << ") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        bool thrown = false;
        try {
            std::string s_err = "12a3";
            CoreUtils::asciiToPackBcd(s_err);
        } catch (const std::runtime_error&) {
            thrown = true;
        }
        assert(thrown);
        std::cout << "Non-digit input '12a3' -> Exception thrown - PASS" << std::endl;
        tests_passed++;

    } catch (const std::exception& e) {
        std::cerr << "Exception in asciiToPackBcd tests: " << e.what() << std::endl;
    }

    // Test packBcdToAscii (simple overload first)
    std::cout << "\n--- Testing packBcdToAscii (simple overload) ---" << std::endl;
    try {
        tests_total++;
        std::vector<unsigned char> bcd_t1 = {0x01, 0x23, 0x45};
        std::string res_t1 = CoreUtils::packBcdToAscii(bcd_t1);
        assert(res_t1 == "012345");
        std::cout << "Input: "; print_bytes(bcd_t1); std::cout << "Output: \"" << res_t1 << "\" (Expected: \"012345\") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::vector<unsigned char> bcd_t2 = {0x01, 0x23};
        std::string res_t2 = CoreUtils::packBcdToAscii(bcd_t2);
        assert(res_t2 == "0123");
        std::cout << "Input: "; print_bytes(bcd_t2); std::cout << "Output: \"" << res_t2 << "\" (Expected: \"0123\") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::vector<unsigned char> bcd_t3 = {0x09};
        std::string res_t3 = CoreUtils::packBcdToAscii(bcd_t3);
        assert(res_t3 == "09");
        std::cout << "Input: "; print_bytes(bcd_t3); std::cout << "Output: \"" << res_t3 << "\" (Expected: \"09\") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::vector<unsigned char> bcd_t4 = {0x00, 0x00, 0x12, 0x34, 0x50};
        std::string res_t4 = CoreUtils::packBcdToAscii(bcd_t4);
        assert(res_t4 == "0000123450");
        std::cout << "Input: "; print_bytes(bcd_t4); std::cout << "Output: \"" << res_t4 << "\" (Expected: \"0000123450\") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::vector<unsigned char> bcd_t5 = {};
        std::string res_t5 = CoreUtils::packBcdToAscii(bcd_t5);
        assert(res_t5 == "");
        std::cout << "Input: "; print_bytes(bcd_t5); std::cout << "Output: \"" << res_t5 << "\" (Expected: \"\") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        bool thrown_decode = false;
        try {
            std::vector<unsigned char> bcd_err = {0x1A};
            CoreUtils::packBcdToAscii(bcd_err);
        } catch (const std::runtime_error&) {
            thrown_decode = true;
        }
        assert(thrown_decode);
        std::cout << "Invalid BCD {0x1A} -> Exception thrown - PASS" << std::endl;
        tests_passed++;

    } catch (const std::exception& e) {
        std::cerr << "Exception in packBcdToAscii (simple) tests: " << e.what() << std::endl;
    }

    // Test packBcdToAscii (with num_digits)
    std::cout << "\n--- Testing packBcdToAscii (with num_digits) ---" << std::endl;
    try {
        tests_total++;
        std::vector<unsigned char> bcd_n1 = {0x01, 0x23, 0x45};
        std::string res_n1a = CoreUtils::packBcdToAscii(bcd_n1, 5);
        assert(res_n1a == "12345");
        std::cout << "Input: "; print_bytes(bcd_n1); std::cout << ", num_digits: 5. Output: \"" << res_n1a << "\" (Expected: \"12345\") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::string res_n1b = CoreUtils::packBcdToAscii(bcd_n1, 6);
        assert(res_n1b == "012345");
        std::cout << "Input: "; print_bytes(bcd_n1); std::cout << ", num_digits: 6. Output: \"" << res_n1b << "\" (Expected: \"012345\") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::string res_n1c = CoreUtils::packBcdToAscii(bcd_n1, 3);
        assert(res_n1c == "345");
        std::cout << "Input: "; print_bytes(bcd_n1); std::cout << ", num_digits: 3. Output: \"" << res_n1c << "\" (Expected: \"345\") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::vector<unsigned char> bcd_doc = {0x00, 0x00, 0x12, 0x34, 0x50};
        std::string res_n2a = CoreUtils::packBcdToAscii(bcd_doc, 9);
        assert(res_n2a == "000123450");
        std::cout << "Input: "; print_bytes(bcd_doc); std::cout << ", num_digits: 9. Output: \"" << res_n2a << "\" (Expected: \"000123450\") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::string res_n2b = CoreUtils::packBcdToAscii(bcd_doc, 10);
        assert(res_n2b == "0000123450");
        std::cout << "Input: "; print_bytes(bcd_doc); std::cout << ", num_digits: 10. Output: \"" << res_n2b << "\" (Expected: \"0000123450\") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::string res_n2c = CoreUtils::packBcdToAscii(bcd_doc, 5);
        assert(res_n2c == "23450");
        std::cout << "Input: "; print_bytes(bcd_doc); std::cout << ", num_digits: 5. Output: \"" << res_n2c << "\" (Expected: \"23450\") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::vector<unsigned char> bcd_s = {0x01};
        std::string res_n3a = CoreUtils::packBcdToAscii(bcd_s, 1);
        assert(res_n3a == "1");
        std::cout << "Input: "; print_bytes(bcd_s); std::cout << ", num_digits: 1. Output: \"" << res_n3a << "\" (Expected: \"1\") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::string res_n3b = CoreUtils::packBcdToAscii(bcd_s, 2);
        assert(res_n3b == "01");
        std::cout << "Input: "; print_bytes(bcd_s); std::cout << ", num_digits: 2. Output: \"" << res_n3b << "\" (Expected: \"01\") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::string res_n3c = CoreUtils::packBcdToAscii(bcd_s, 3);
        assert(res_n3c == "001");
        std::cout << "Input: "; print_bytes(bcd_s); std::cout << ", num_digits: 3. Output: \"" << res_n3c << "\" (Expected: \"001\") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::string res_n4 = CoreUtils::packBcdToAscii(bcd_s, 0);
        assert(res_n4 == "01");
        std::cout << "Input: "; print_bytes(bcd_s); std::cout << ", num_digits: 0. Output: \"" << res_n4 << "\" (Expected: \"01\") - PASS" << std::endl;
        tests_passed++;

        tests_total++;
        std::vector<unsigned char> bcd_empty_decode = {};
        std::string res_n5 = CoreUtils::packBcdToAscii(bcd_empty_decode, 0);
        assert(res_n5 == "");
        std::cout << "Input: "; print_bytes(bcd_empty_decode); std::cout << ", num_digits: 0. Output: \"" << res_n5 << "\" (Expected: \"\") - PASS" << std::endl;
        tests_passed++;

    } catch (const std::exception& e) {
        std::cerr << "Exception in packBcdToAscii (num_digits) tests: " << e.what() << std::endl;
    }

    std::cout << "\nFinished PACK BCD tests." << std::endl;
    std::cout << tests_passed << " / " << tests_total << " tests passed." << std::endl;

    return !(tests_passed == tests_total);
}
