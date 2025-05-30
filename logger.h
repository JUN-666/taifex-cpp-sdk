// logger.h
#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <sstream> // For log message construction
#include <iostream> // For std::cout, std::cerr (will be in .cpp)
#include <chrono>   // For timestamps
#include <iomanip>  // For std::put_time

namespace CoreUtils {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    NONE = 4 // To disable all logging
};

// Sets the minimum log level that will be processed.
void setLogLevel(LogLevel level);

// Core logging function (typically not called directly by user code)
void logMessage(LogLevel level, const std::string& message, const char* file, int line);

// Helper macro to allow variadic arguments for log messages (C++11 style)
// This creates a temporary ostringstream and then passes its content.

// Forward declaration for NullStream
class NullStream;
extern NullStream g_null_stream; // Will be defined in logger.cpp

// Forward declaration
class LogMessageStreamComposer;

// User-facing macros should directly use LogMessageStreamComposer
// The getStream() method will handle returning the correct stream.
#define LOG_DEBUG   CoreUtils::LogMessageStreamComposer(CoreUtils::LogLevel::DEBUG, __FILE__, __LINE__).getStream()
#define LOG_INFO    CoreUtils::LogMessageStreamComposer(CoreUtils::LogLevel::INFO, __FILE__, __LINE__).getStream()
#define LOG_WARNING CoreUtils::LogMessageStreamComposer(CoreUtils::LogLevel::WARNING, __FILE__, __LINE__).getStream()
#define LOG_ERROR   CoreUtils::LogMessageStreamComposer(CoreUtils::LogLevel::ERROR, __FILE__, __LINE__).getStream()


// Internal helper class for stream-based logging
class LogMessageStreamComposer {
public:
    LogMessageStreamComposer(LogLevel level, const char* file, int line);
    ~LogMessageStreamComposer();
    std::ostream& getStream(); // Now conditionally returns g_null_stream
private:
    std::ostringstream _stream; // Used only if _is_active
    LogLevel _level;
    const char* _file;
    int _line;
    bool _is_active; // Flag to indicate if this message should be logged
};

// Declaration for getCurrentLogLevel, to be defined in logger.cpp
LogLevel getCurrentLogLevel();

// Definition of NullStream
class NullStream : public std::ostream {
public:
    NullStream() : std::ostream(nullptr) {}

    template <class T>
    NullStream& operator<<(const T& /*val*/) {
        return *this;
    }
    NullStream& operator<<(std::ostream& (*/*manip*/)(std::ostream&)) {
        return *this;
    }
};

} // namespace CoreUtils

#endif // LOGGER_H
