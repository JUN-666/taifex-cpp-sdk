cmake_minimum_required(VERSION 3.10)
project(MessageProcessingSDK CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
# set(CMAKE_CXX_STANDARD 17) # Consider for std::optional etc.

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Core Utilities Library (core_utils)
add_library(core_utils
    pack_bcd.cpp checksum.cpp string_utils.cpp logger.cpp
    common_header.cpp message_identifier.cpp
)
target_include_directories(core_utils PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
find_package(Threads REQUIRED) # Threads is already found once here
target_link_libraries(core_utils PRIVATE Threads::Threads)

# Specific Message Parsers Library (specific_message_parsers)
add_library(specific_message_parsers STATIC
    messages/message_parser_utils.cpp messages/message_i010.cpp messages/message_i081.cpp
    messages/message_i083.cpp messages/message_i001.cpp messages/message_i002.cpp
)
target_link_libraries(specific_message_parsers PUBLIC core_utils)
target_include_directories(specific_message_parsers PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Order Book Library (order_book_lib)
add_library(order_book_lib STATIC
    order_book/order_book.cpp
)
target_link_libraries(order_book_lib PUBLIC specific_message_parsers)
target_include_directories(order_book_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# NEW: Taifex Networking Library (taifex_networking_lib)
add_library(taifex_networking_lib STATIC
    networking/retransmission_protocol.cpp
    networking/multicast_receiver.cpp
    networking/retransmission_client.cpp
    networking/network_manager.cpp
)
# Networking library uses core_utils (e.g., for logging) and threads
target_link_libraries(taifex_networking_lib PUBLIC core_utils) # PUBLIC if its headers include core_utils headers
target_link_libraries(taifex_networking_lib PRIVATE Threads::Threads) # Threads are an implementation detail

target_include_directories(taifex_networking_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> # To find its own headers (e.g. networking/endian_utils.h)
    $<INSTALL_INTERFACE:include>                  # For installed headers
)


# Taifex SDK Library (taifex_sdk_lib) - Now defined after its dependencies
add_library(taifex_sdk_lib STATIC
    sdk/taifex_sdk.cpp
)
# TaifexSdk uses OrderBook, message structs, core utils, and now networking.
# Linking PUBLIC to order_book_lib and taifex_networking_lib makes their headers available.
target_link_libraries(taifex_sdk_lib PUBLIC order_book_lib taifex_networking_lib)

target_include_directories(taifex_sdk_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)


# --- Installation ---
install(TARGETS core_utils ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(FILES pack_bcd.h checksum.h string_utils.h logger.h error_codes.h common_header.h message_identifier.h DESTINATION include/CoreUtils)

install(TARGETS specific_message_parsers ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY messages/ DESTINATION include/SpecificMessageParsers FILES_MATCHING PATTERN "*.h")

install(TARGETS order_book_lib ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(FILES order_book/order_book.h DESTINATION include/OrderBookManagement)

# Install taifex_sdk_lib and its header (from previous step, order might change slightly)
install(TARGETS taifex_sdk_lib
    ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin
)
install(FILES sdk/taifex_sdk.h DESTINATION include/Taifex)

# NEW: Install taifex_networking_lib and its public headers
install(TARGETS taifex_networking_lib
    ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin
)
# Install relevant public headers for the networking module
install(FILES
    networking/network_manager.h
    networking/retransmission_protocol.h # Contains structs that might be part of NetworkManagerConfig if detailed
    networking/endian_utils.h            # Utility header
    # multicast_receiver.h and retransmission_client.h are considered internal implementation details of NetworkManager for now
    DESTINATION include/TaifexNetworking
)


# --- Test Executables ---
# (Functions add_core_utils_test, add_message_parser_test, add_order_book_test, add_taifex_sdk_test remain)
function(add_core_utils_test test_name main_cpp_file)
    add_executable(${test_name} ${main_cpp_file})
    target_link_libraries(${test_name} PRIVATE core_utils)
endfunction()
function(add_message_parser_test test_name main_cpp_file)
    add_executable(${test_name} ${main_cpp_file})
    target_link_libraries(${test_name} PRIVATE specific_message_parsers)
endfunction()
function(add_order_book_test test_name main_cpp_file)
    add_executable(${test_name} ${main_cpp_file})
    target_link_libraries(${test_name} PRIVATE order_book_lib)
endfunction()
function(add_taifex_sdk_test test_name main_cpp_file)
    add_executable(${test_name} ${main_cpp_file})
    target_link_libraries(${test_name} PRIVATE taifex_sdk_lib)
endfunction()
# No new test executable for taifex_networking_lib itself in this step, unit tests for it are next.

# Existing tests ...
add_core_utils_test(pack_bcd_tester main_pack_bcd_test.cpp)
add_core_utils_test(checksum_tester main_checksum_test.cpp)
add_core_utils_test(string_utils_tester main_string_utils_test.cpp)
add_core_utils_test(logger_tester main_logger_test.cpp)
add_core_utils_test(error_codes_tester main_error_codes_test.cpp)
add_core_utils_test(common_header_parser_tester main_common_header_parser_test.cpp)
add_core_utils_test(message_identifier_tester main_message_identifier_test.cpp)

add_message_parser_test(test_message_i010 tests/test_message_i010.cpp)
add_message_parser_test(test_message_i081 tests/test_message_i081.cpp)
add_message_parser_test(test_message_i083 tests/test_message_i083.cpp)
add_message_parser_test(test_message_i001 tests/test_message_i001.cpp)
add_message_parser_test(test_message_i002 tests/test_message_i002.cpp)

add_order_book_test(test_order_book tests/test_order_book.cpp)
add_taifex_sdk_test(test_taifex_sdk tests/test_taifex_sdk.cpp)


# Enable testing with CTest
enable_testing()

# CTest entries ...
add_test(NAME TestMessageI010 COMMAND test_message_i010)
add_test(NAME TestMessageI081 COMMAND test_message_i081)
add_test(NAME TestMessageI083 COMMAND test_message_i083)
add_test(NAME TestMessageI001 COMMAND test_message_i001)
add_test(NAME TestMessageI002 COMMAND test_message_i002)
add_test(NAME TestOrderBook COMMAND test_order_book)
add_test(NAME TestTaifexSdk COMMAND test_taifex_sdk)

# ... (rest of CMakeLists.txt, e.g. existing CTest entries if any) ...
