// pack_bcd.h
#ifndef PACK_BCD_H
#define PACK_BCD_H

#include <vector>
#include <string>
#include <stdexcept> // For std::runtime_error

namespace CoreUtils {

std::vector<unsigned char> asciiToPackBcd(const std::string& ascii_numeric_str);
std::string packBcdToAscii(const std::vector<unsigned char>& bcd_data, size_t num_digits);
std::string packBcdToAscii(const std::vector<unsigned char>& bcd_data);

} // namespace CoreUtils

#endif // PACK_BCD_H
