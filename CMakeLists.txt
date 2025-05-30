cmake_minimum_required(VERSION 3.10)
project(CoreUtils CXX) # Changed project name

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# Option to build shared libraries (OFF by default, so STATIC is default)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Core Utilities Library
add_library(core_utils # Type (STATIC/SHARED) determined by BUILD_SHARED_LIBS default or user setting
    pack_bcd.cpp
    checksum.cpp
    string_utils.cpp
    logger.cpp
    # error_codes.h is header-only, included by the .cpp files that need it
)
# Set properties for Windows shared library builds (DLL export/import)
# This would typically involve generating a config header with dllexport/dllimport macros.
# For a simple static library, this is not strictly needed but good for completeness if shared libs are enabled.
# if(WIN32 AND BUILD_SHARED_LIBS)
#   target_compile_definitions(core_utils PRIVATE "-DCORE_UTILS_BUILD_DLL") # Example define
#   # And public headers would use CORE_UTILS_API macro for dllexport/import
# endif()


target_include_directories(core_utils
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> # For consumers finding headers during build
        $<INSTALL_INTERFACE:include> # For consumers finding headers after installation (e.g. /usr/local/include)
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} # For sources within the library itself
)

find_package(Threads REQUIRED)
target_link_libraries(core_utils PRIVATE Threads::Threads) # PUBLIC if consumers also need Threads, PRIVATE if internal use only.
                                                       # Logger uses threads, but consumers of logger macros don't directly.
                                                       # Let's assume PRIVATE for now unless a header exposes thread objects.
                                                       # Re-evaluating: if headers include <thread> or <mutex> via logger.h, then PUBLIC might be safer.
                                                       # logger.h includes <chrono> but not <thread> or <mutex> directly.
                                                       # So, PRIVATE for Threads::Threads linkage to core_utils should be fine.
                                                       # Changing Threads::Threads to PRIVATE linkage for core_utils.
                                                       # target_link_libraries(core_utils PRIVATE Threads::Threads) -- already was private, good.

# --- Installation ---
# Install the library
install(TARGETS core_utils
    ARCHIVE DESTINATION lib  # For static libs
    LIBRARY DESTINATION lib  # For shared libs (Windows .dll, Linux .so)
    RUNTIME DESTINATION bin  # For .dlls needed at runtime on Windows by executables
)

# Install public headers
# Assuming all .h files in the root are public headers for the library
install(FILES
    pack_bcd.h
    checksum.h
    string_utils.h
    logger.h
    error_codes.h
    common_header.h # <-- Add this line
    DESTINATION include/CoreUtils # Install them into an "CoreUtils" subdirectory for namespacing
    COMPONENT Devel # Optional component name
)
# A more robust way for headers if they are in a dedicated 'include' dir:
# install(DIRECTORY include/CoreUtils DESTINATION include)


# --- Test Executables ---
# Common function to add a test executable
function(add_core_utils_test test_name main_cpp_file)
    add_executable(${test_name} ${main_cpp_file})
    target_link_libraries(${test_name} PRIVATE core_utils)
    # No need for target_include_directories for the test main if core_utils
    # correctly exports its include directory via PUBLIC target_include_directories.
endfunction()

add_core_utils_test(pack_bcd_tester main_pack_bcd_test.cpp)
add_core_utils_test(checksum_tester main_checksum_test.cpp)
add_core_utils_test(string_utils_tester main_string_utils_test.cpp)
add_core_utils_test(logger_tester main_logger_test.cpp)
add_core_utils_test(error_codes_tester main_error_codes_test.cpp)

# Enable testing with CTest
enable_testing()

# Example of adding tests to CTest (optional for this step, but good practice)
# add_test(NAME PackBCDTest COMMAND pack_bcd_tester)
# add_test(NAME ChecksumTest COMMAND checksum_tester)
# add_test(NAME StringUtilsTest COMMAND string_utils_tester)
# add_test(NAME LoggerTest COMMAND logger_tester) # Logger output is visual
# add_test(NAME ErrorCodesTest COMMAND error_codes_tester) # Error codes output is visual

# To build and install:
# mkdir build && cd build
# cmake ..
# cmake --build .
# cmake --install . --prefix "your_install_path" (e.g., ../install)
