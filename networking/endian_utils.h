#ifndef ENDIAN_UTILS_H
#define ENDIAN_UTILS_H

#include <cstdint>
#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h> // For htons, htonl, ntohs, ntohl on Windows
    #include <stdlib.h>   // For _byteswap_uint64
#else
    #include <arpa/inet.h> // For htons, htonl, ntohs, ntohl on POSIX systems
#endif

// C++20 introduces std::endian for compile-time endian detection.
// C++23 introduces std::byteswap for portable byte swapping.
// These utilities provide common wrappers for pre-C++20 environments.

namespace NetworkingUtils {

/**
 * @brief Converts a 16-bit unsigned integer from host byte order to network byte order (big-endian).
 * @param val The value in host byte order.
 * @return The value in network byte order.
 */
inline uint16_t host_to_network_short(uint16_t val) { return htons(val); }

/**
 * @brief Converts a 32-bit unsigned integer from host byte order to network byte order (big-endian).
 * @param val The value in host byte order.
 * @return The value in network byte order.
 */
inline uint32_t host_to_network_long(uint32_t val) { return htonl(val); }

/**
 * @brief Converts a 16-bit unsigned integer from network byte order (big-endian) to host byte order.
 * @param val The value in network byte order.
 * @return The value in host byte order.
 */
inline uint16_t network_to_host_short(uint16_t val) { return ntohs(val); }

/**
 * @brief Converts a 32-bit unsigned integer from network byte order (big-endian) to host byte order.
 * @param val The value in network byte order.
 * @return The value in host byte order.
 */
inline uint32_t network_to_host_long(uint32_t val) { return ntohl(val); }


// Platform-dependent 64-bit endian conversion.
// Network byte order is generally big-endian.
#if defined(__GNUC__) || defined(__clang__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        /**
         * @brief Converts a 64-bit unsigned integer from host (little-endian) to network byte order (big-endian).
         * Specific implementation for GCC/Clang on little-endian systems.
         */
        inline uint64_t host_to_network_long_long(uint64_t val) {
            return (((uint64_t)htonl(static_cast<uint32_t>(val & 0xFFFFFFFF))) << 32) | htonl(static_cast<uint32_t>(val >> 32));
        }
        /**
         * @brief Converts a 64-bit unsigned integer from network byte order (big-endian) to host (little-endian).
         * Specific implementation for GCC/Clang on little-endian systems.
         */
        inline uint64_t network_to_host_long_long(uint64_t val) {
            return (((uint64_t)ntohl(static_cast<uint32_t>(val & 0xFFFFFFFF))) << 32) | ntohl(static_cast<uint32_t>(val >> 32));
        }
    #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        /**
         * @brief Converts a 64-bit unsigned integer from host (big-endian) to network byte order (big-endian).
         * No conversion needed on big-endian systems.
         */
        inline uint64_t host_to_network_long_long(uint64_t val) { return val; }
        /**
         * @brief Converts a 64-bit unsigned integer from network byte order (big-endian) to host (big-endian).
         * No conversion needed on big-endian systems.
         */
        inline uint64_t network_to_host_long_long(uint64_t val) { return val; }
    #else
        #error "Byte order not supported or not detected by GCC/Clang specific macros."
    #endif
#elif defined(_MSC_VER)
    // Windows (MSVC) is typically little-endian.
    // _byteswap_uint64 reverses byte order, so it converts little-endian to big-endian and vice-versa.
    /**
     * @brief Converts a 64-bit unsigned integer from host (assumed little-endian) to network byte order (big-endian).
     * Specific implementation for MSVC using _byteswap_uint64.
     */
    inline uint64_t host_to_network_long_long(uint64_t val) {
        return _byteswap_uint64(val);
    }
    /**
     * @brief Converts a 64-bit unsigned integer from network byte order (big-endian) to host (assumed little-endian).
     * Specific implementation for MSVC using _byteswap_uint64.
     */
    inline uint64_t network_to_host_long_long(uint64_t val) {
        return _byteswap_uint64(val);
    }
#else
    // Fallback for other compilers/platforms. This runtime check is less efficient.
    // A build-time configuration check for endianness is generally preferred for these platforms.
    #warning "Using runtime byte order check for 64-bit conversions. Consider build-time configuration for efficiency."
    /**
     * @brief Converts a 64-bit unsigned integer from host to network byte order (big-endian).
     * Generic implementation with runtime endianness check.
     */
    inline uint64_t host_to_network_long_long(uint64_t val) {
        const uint16_t endian_check_val = 0x0100; // Host: 0x0100 (BE) or 0x0001 (LE)
        if (*(const uint8_t*)&endian_check_val == 0x01) { // Big-endian host
            return val;
        } else { // Little-endian host
             return (((uint64_t)htonl(static_cast<uint32_t>(val & 0xFFFFFFFF))) << 32) | htonl(static_cast<uint32_t>(val >> 32));
        }
    }
    /**
     * @brief Converts a 64-bit unsigned integer from network byte order (big-endian) to host byte order.
     * Generic implementation with runtime endianness check.
     */
    inline uint64_t network_to_host_long_long(uint64_t val) {
        const uint16_t endian_check_val = 0x0100;
        if (*(const uint8_t*)&endian_check_val == 0x01) { // Big-endian host
            return val;
        } else { // Little-endian host
            return (((uint64_t)ntohl(static_cast<uint32_t>(val & 0xFFFFFFFF))) << 32) | ntohl(static_cast<uint32_t>(val >> 32));
        }
    }
#endif

} // namespace NetworkingUtils
#endif // ENDIAN_UTILS_H
