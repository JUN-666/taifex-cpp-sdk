#ifndef ENDIAN_UTILS_H
#define ENDIAN_UTILS_H

#include <cstdint>
#include <arpa/inet.h> // For htons, htonl, ntohs, ntohl

// It's common for POSIX systems to define these in <arpa/inet.h> or <netinet/in.h>
// For MSVC, <winsock2.h> then <ws2tcpip.h>
// C++20 has std::endian, C++23 has std::byteswap

namespace NetworkingUtils {

inline uint16_t host_to_network_short(uint16_t val) { return htons(val); }
inline uint32_t host_to_network_long(uint32_t val) { return htonl(val); }
// uint64_t host_to_network_long_long(uint64_t val); // Requires custom or non-standard

inline uint16_t network_to_host_short(uint16_t val) { return ntohs(val); }
inline uint32_t network_to_host_long(uint32_t val) { return ntohl(val); }
// uint64_t network_to_host_long_long(uint64_t val); // Requires custom or non-standard

// Basic implementation for 64-bit if htonll/ntohll are not available
// __BYTE_ORDER is a GCC extension, __LITTLE_ENDIAN and __BIG_ENDIAN are also common.
// For more portability, a configure-time check or a library like Boost.Endian might be used.
#if defined(__GNUC__) || defined(__clang__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        inline uint64_t host_to_network_long_long(uint64_t val) {
            return (((uint64_t)htonl(static_cast<uint32_t>(val & 0xFFFFFFFF))) << 32) | htonl(static_cast<uint32_t>(val >> 32));
        }
        inline uint64_t network_to_host_long_long(uint64_t val) {
            return (((uint64_t)ntohl(static_cast<uint32_t>(val & 0xFFFFFFFF))) << 32) | ntohl(static_cast<uint32_t>(val >> 32));
        }
    #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        inline uint64_t host_to_network_long_long(uint64_t val) { return val; }
        inline uint64_t network_to_host_long_long(uint64_t val) { return val; }
    #else
        #error "Byte order not supported or not detected"
    #endif
#elif defined(_MSC_VER)
    // Windows is typically little-endian
    // No standard htonll/ntohll in Winsock, _byteswap_uint64 can be used for general swap.
    // For host-to-network, if host is little-endian, swap is needed.
    #include <stdlib.h>
    inline uint64_t host_to_network_long_long(uint64_t val) {
        // Assuming little-endian host for MSVC common case
        return _byteswap_uint64(val);
    }
    inline uint64_t network_to_host_long_long(uint64_t val) {
        // Assuming little-endian host for MSVC common case
        return _byteswap_uint64(val);
    }
#else
    // Fallback or error for other compilers/platforms if byte order macros not defined
    #warning "Byte order detection for 64-bit conversions might be incomplete for this platform."
    // Attempt a generic little-endian implementation as a guess or require specific flags
    // This is a common default, but not universally safe.
    inline uint64_t host_to_network_long_long(uint64_t val) {
        uint64_t temp = 1;
        if (*(char*)&temp == 1) { // Check if little endian at runtime (inefficient for repeated calls)
             return (((uint64_t)htonl(static_cast<uint32_t>(val & 0xFFFFFFFF))) << 32) | htonl(static_cast<uint32_t>(val >> 32));
        }
        return val; // Assume big endian or same as network
    }
    inline uint64_t network_to_host_long_long(uint64_t val) {
        uint64_t temp = 1;
         if (*(char*)&temp == 1) { // Check if little endian
            return (((uint64_t)ntohl(static_cast<uint32_t>(val & 0xFFFFFFFF))) << 32) | ntohl(static_cast<uint32_t>(val >> 32));
        }
        return val;
    }
#endif

} // namespace NetworkingUtils
#endif // ENDIAN_UTILS_H
