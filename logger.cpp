// logger.cpp
#include "logger.h"
#include <iostream>
#include <mutex> // For basic thread safety around log level setting and cout/cerr
#include <cstring> // For strrchr

namespace CoreUtils {

static LogLevel current_min_log_level = LogLevel::INFO;
static std::mutex log_mutex; // Mutex for thread-safe log level access and output
NullStream g_null_stream;    // Definition of the global null stream instance

void setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> guard(log_mutex);
    current_min_log_level = level;
}

LogLevel getCurrentLogLevel() { // No lock needed if only used internally by LOG_STREAM check
    // Reading the current level might not need a lock if updates are rare and protected.
    // However, for strict safety, especially if setLogLevel can be called frequently
    // from different threads, a read lock or the same lock could be used.
    // For this basic logger, assuming reads are frequent and writes are rare.
    return current_min_log_level;
}

// Implementation of LogMessageStreamComposer
LogMessageStreamComposer::LogMessageStreamComposer(LogLevel level, const char* file, int line)
    : _level(level), _file(file), _line(line), _is_active(false) {
    // Check level immediately.
    // No lock needed here to read current_min_log_level if getCurrentLogLevel() is safe.
    // Or, could lock here too if setLogLevel is frequent from other threads.
    // For simplicity, direct access for now, assuming getCurrentLogLevel() provides the needed value.
    if (_level >= getCurrentLogLevel()) {
        _is_active = true;
    }
}

std::ostream& LogMessageStreamComposer::getStream() {
    if (_is_active) {
        return _stream;
    }
    return g_null_stream;
}

LogMessageStreamComposer::~LogMessageStreamComposer() {
    if (_is_active && !_stream.str().empty()) { // Only log if active and message is not empty
        logMessage(_level, _stream.str(), _file, _line);
    }
}


void logMessage(LogLevel level, const std::string& message, const char* file, int line) {
    // The _is_active check in LogMessageStreamComposer's destructor should prevent
    // this function from being called if the level was too low initially.
    // However, current_min_log_level could change between LogMessageStreamComposer construction and destruction.
    // So, a final check under lock is good.
    std::lock_guard<std::mutex> guard(log_mutex);

    if (level < current_min_log_level) { // Final check with lock
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);

    // Get milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostream& output_stream = (level == LogLevel::ERROR || level == LogLevel::WARNING) ? std::cerr : std::cout;

    output_stream << "[" << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
    output_stream << '.' << std::setfill('0') << std::setw(3) << ms.count() << std::setfill(' ') << "] ";

    switch (level) {
        case LogLevel::DEBUG:   output_stream << "[DEBUG]   "; break;
        case LogLevel::INFO:    output_stream << "[INFO]    "; break;
        case LogLevel::WARNING: output_stream << "[WARNING] "; break;
        case LogLevel::ERROR:   output_stream << "[ERROR]   "; break;
        default: break;
    }

    const char* base_file_name = file;
    const char* last_slash = strrchr(file, '/');
    if (last_slash) {
        base_file_name = last_slash + 1;
    } else {
        const char* last_backslash = strrchr(file, '\\');
        if (last_backslash) {
            base_file_name = last_backslash + 1;
        }
    }
    output_stream << "[" << base_file_name << ":" << line << "] ";
    output_stream << message << std::endl;
}

} // namespace CoreUtils
