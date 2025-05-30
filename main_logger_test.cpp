// main_logger_test.cpp
#include "logger.h"
#include <thread> // For testing basic thread safety (just concurrent calls)
#include <vector>
#include <iostream> // Required for std::cout here

void log_spam_function() {
    LOG_DEBUG << "This is a debug message from thread " << std::this_thread::get_id();
    LOG_INFO << "Info message with a number: " << 123 << " from thread " << std::this_thread::get_id();
    LOG_WARNING << "Warning: something might be wrong. Value: " << 3.14 << " from thread " << std::this_thread::get_id();
    LOG_ERROR << "Error! Critical failure. Code: " << 0xDEADBEEF << " from thread " << std::this_thread::get_id();
}

int main() {
    std::cout << "Testing Logger Utilities..." << std::endl;
    std::cout << "Default log level is INFO. DEBUG messages should not appear." << std::endl;

    LOG_DEBUG << "This is a DEBUG message. (Should not be visible by default)";
    LOG_INFO << "This is an INFO message.";
    LOG_WARNING << "This is a WARNING message.";
    LOG_ERROR << "This is an ERROR message. An int: " << 42 << ", a float: " << 3.14f;

    std::cout << "\nSetting log level to DEBUG. All messages should appear." << std::endl;
    CoreUtils::setLogLevel(CoreUtils::LogLevel::DEBUG);
    LOG_DEBUG << "This is a DEBUG message. (Now visible)";
    LOG_INFO << "This is another INFO message.";

    std::cout << "\nSetting log level to WARNING. Only WARNING and ERROR should appear." << std::endl;
    CoreUtils::setLogLevel(CoreUtils::LogLevel::WARNING);
    LOG_DEBUG << "This is a DEBUG message. (Should not be visible)";
    LOG_INFO << "This is an INFO message. (Should not be visible)";
    LOG_WARNING << "This is another WARNING message.";
    LOG_ERROR << "This is another ERROR message.";

    std::cout << "\nSetting log level to NONE. No messages should appear." << std::endl;
    CoreUtils::setLogLevel(CoreUtils::LogLevel::NONE);
    LOG_DEBUG << "This is a DEBUG message. (Should not be visible)";
    LOG_INFO << "This is an INFO message. (Should not be visible)";
    LOG_WARNING << "This is a WARNING message. (Should not be visible)";
    LOG_ERROR << "This is an ERROR message. (Should not be visible)";

    std::cout << "\nResetting log level to INFO for further tests." << std::endl;
    CoreUtils::setLogLevel(CoreUtils::LogLevel::INFO);

    std::cout << "\nTesting concurrent logging (basic test, output may interleave but should not crash):" << std::endl;
    std::vector<std::thread> threads;
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back(log_spam_function);
    }
    for (auto& t : threads) {
        t.join();
    }

    std::cout << "\nFinished Logger tests. Please visually inspect output." << std::endl;
    // Automated testing of logger output is complex and beyond scope here.
    // We rely on visual inspection for this basic test.
    return 0;
}
