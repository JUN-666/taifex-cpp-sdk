// main_error_codes_test.cpp
#include "error_codes.h" // Should be in the include path
#include "logger.h"      // Use our logger for test output
#include <iostream>
#include <string>

void function_that_throws_invalid_arg(int x) {
    if (x < 0) {
        throw CoreUtils::InvalidArgumentError("Input value cannot be negative.");
    }
    LOG_INFO << "function_that_throws_invalid_arg received valid argument: " << x;
}

void function_that_throws_parsing_error(const std::string& data) {
    if (data.find("invalid_token") != std::string::npos) {
        throw CoreUtils::ParsingError("Found invalid token during parsing.");
    }
    LOG_INFO << "function_that_throws_parsing_error processed data: " << data;
}

int main() {
    CoreUtils::setLogLevel(CoreUtils::LogLevel::DEBUG); // Enable all logs for test
    LOG_INFO << "Testing Error Handling Structures...";

    // Test InvalidArgumentError
    try {
        LOG_DEBUG << "Calling function_that_throws_invalid_arg with 10 (should not throw)";
        function_that_throws_invalid_arg(10);
        LOG_DEBUG << "Calling function_that_throws_invalid_arg with -1 (should throw)";
        function_that_throws_invalid_arg(-1); // This should throw
    } catch (const CoreUtils::InvalidArgumentError& e) {
        LOG_ERROR << "Caught InvalidArgumentError: " << e.what();
    } catch (const CoreUtils::CoreUtilsException& e) {
        LOG_ERROR << "Caught CoreUtilsException (unexpected type for invalid arg): " << e.what();
    } catch (const std::exception& e) {
        LOG_ERROR << "Caught std::exception (unexpected type for invalid arg): " << e.what();
    }

    // Test ParsingError
    try {
        LOG_DEBUG << "Calling function_that_throws_parsing_error with 'valid_data' (should not throw)";
        function_that_throws_parsing_error("valid_data");
        LOG_DEBUG << "Calling function_that_throws_parsing_error with 'data with invalid_token' (should throw)";
        function_that_throws_parsing_error("data with invalid_token"); // This should throw
    } catch (const CoreUtils::ParsingError& e) {
        LOG_ERROR << "Caught ParsingError: " << e.what();
    } catch (const CoreUtils::CoreUtilsException& e) {
        LOG_ERROR << "Caught CoreUtilsException (unexpected type for parsing error): " << e.what();
    } catch (const std::exception& e) {
        LOG_ERROR << "Caught std::exception (unexpected type for parsing error): " << e.what();
    }

    // Test catching base class
    try {
        LOG_DEBUG << "Throwing ParsingError, catching as CoreUtilsException.";
        throw CoreUtils::ParsingError("Specific parsing issue for base class test.");
    } catch (const CoreUtils::CoreUtilsException& e) {
        LOG_INFO << "Successfully caught ParsingError as CoreUtilsException: " << e.what();
    } catch (const std::exception& e) {
        LOG_ERROR << "Caught std::exception (unexpected, should have been CoreUtilsException): " << e.what();
    }


    LOG_INFO << "Finished Error Handling tests. Review log output.";
    // This test is mostly about demonstrating the types and catch mechanisms.
    // Automated pass/fail is harder without more structure, relies on log review.
    return 0;
}
