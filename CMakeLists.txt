cmake_minimum_required(VERSION 3.10)
project(MessageProcessingSDK CXX) # Updated project name to be more encompassing

set(CMAKE_CXX_STANDARD 11) # Was 11, let's ensure it's at least C++11 for our code.
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Core Utilities Library (core_utils) - No changes to its definition
add_library(core_utils
    pack_bcd.cpp
    checksum.cpp
    string_utils.cpp
    logger.cpp
    common_header.cpp
    message_identifier.cpp
)
target_include_directories(core_utils
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
find_package(Threads REQUIRED)
target_link_libraries(core_utils PRIVATE Threads::Threads)


# NEW: Specific Message Parsers Library (specific_message_parsers)
add_library(specific_message_parsers STATIC # Explicitly STATIC, or could use BUILD_SHARED_LIBS
    messages/message_parser_utils.cpp
    messages/message_i010.cpp
    messages/message_i081.cpp
    messages/message_i083.cpp
    messages/message_i001.cpp
    messages/message_i002.cpp
)
target_link_libraries(specific_message_parsers PUBLIC core_utils) # PUBLIC so users of specific_message_parsers also link core_utils

target_include_directories(specific_message_parsers
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> # For its own headers and for consumers
        $<INSTALL_INTERFACE:include> # For installed headers
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} # For its own sources to find headers in root like pack_bcd.h via core_utils
)


# --- Installation ---
# Install core_utils library
install(TARGETS core_utils
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
# Install core_utils public headers
install(FILES
    pack_bcd.h
    checksum.h
    string_utils.h
    logger.h
    error_codes.h
    common_header.h
    message_identifier.h # Added this line as it was missing from previous install list
    DESTINATION include/CoreUtils
)

# NEW: Install specific_message_parsers library
install(TARGETS specific_message_parsers
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
# NEW: Install specific_message_parsers public headers
install(DIRECTORY messages/ DESTINATION include/SpecificMessageParsers FILES_MATCHING PATTERN "*.h")


# --- Test Executables ---
# Common function to add a core_utils test executable
function(add_core_utils_test test_name main_cpp_file)
    add_executable(${test_name} ${main_cpp_file})
    target_link_libraries(${test_name} PRIVATE core_utils)
endfunction()

# NEW: Common function to add a message parser test executable
function(add_message_parser_test test_name main_cpp_file)
    add_executable(${test_name} ${main_cpp_file})
    target_link_libraries(${test_name} PRIVATE specific_message_parsers) # Links against specific_message_parsers (which links core_utils)
endfunction()

# Existing core_utils tests
add_core_utils_test(pack_bcd_tester main_pack_bcd_test.cpp)
add_core_utils_test(checksum_tester main_checksum_test.cpp)
add_core_utils_test(string_utils_tester main_string_utils_test.cpp)
add_core_utils_test(logger_tester main_logger_test.cpp)
add_core_utils_test(error_codes_tester main_error_codes_test.cpp)
add_core_utils_test(common_header_parser_tester main_common_header_parser_test.cpp)
add_core_utils_test(message_identifier_tester main_message_identifier_test.cpp)

# NEW: Add tests for specific message parsers
add_message_parser_test(test_message_i010 tests/test_message_i010.cpp)
add_message_parser_test(test_message_i081 tests/test_message_i081.cpp)
add_message_parser_test(test_message_i083 tests/test_message_i083.cpp)
add_message_parser_test(test_message_i001 tests/test_message_i001.cpp)
add_message_parser_test(test_message_i002 tests/test_message_i002.cpp)


# Enable testing with CTest
enable_testing()

# NEW: Add CTest entries for the new tests (optional, but good practice)
add_test(NAME TestMessageI010 COMMAND test_message_i010)
add_test(NAME TestMessageI081 COMMAND test_message_i081)
add_test(NAME TestMessageI083 COMMAND test_message_i083)
add_test(NAME TestMessageI001 COMMAND test_message_i001)
add_test(NAME TestMessageI002 COMMAND test_message_i002)

# Existing CTest entries (example, can be uncommented/added if desired)
# add_test(NAME PackBCDTest COMMAND pack_bcd_tester)
# ... other existing tests ...

# To build and install:
# mkdir build && cd build
# cmake ..
# cmake --build .
# cmake --install . --prefix "your_install_path" (e.g., ../install_dir relative to build)
