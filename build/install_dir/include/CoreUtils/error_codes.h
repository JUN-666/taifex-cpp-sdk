// error_codes.h
#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#include <stdexcept>
#include <string>

namespace CoreUtils {

/**
 * @brief Base class for custom exceptions in the CoreUtils library.
 */
class CoreUtilsException : public std::runtime_error {
public:
    explicit CoreUtilsException(const std::string& message)
        : std::runtime_error(message) {}

    explicit CoreUtilsException(const char* message)
        : std::runtime_error(message) {}
};

/**
 * @brief Exception for errors related to invalid arguments passed to functions.
 */
class InvalidArgumentError : public CoreUtilsException {
public:
    explicit InvalidArgumentError(const std::string& message)
        : CoreUtilsException(message) {}

    explicit InvalidArgumentError(const char* message)
        : CoreUtilsException(message) {}
};

/**
 * @brief Exception for errors encountered during parsing operations.
 *
 * This could be used by PACK BCD decoders, message parsers, etc.
 */
class ParsingError : public CoreUtilsException {
public:
    explicit ParsingError(const std::string& message)
        : CoreUtilsException(message) {}

    explicit ParsingError(const char* message)
        : CoreUtilsException(message) {}
};

/**
 * @brief Exception for configuration-related errors.
 */
class ConfigurationError : public CoreUtilsException {
public:
    explicit ConfigurationError(const std::string& message)
        : CoreUtilsException(message) {}

    explicit ConfigurationError(const char* message)
        : CoreUtilsException(message) {}
};

/**
 * @brief Exception for I/O related errors.
 */
class IOError : public CoreUtilsException {
public:
    explicit IOError(const std::string& message)
        : CoreUtilsException(message) {}

    explicit IOError(const char* message)
        : CoreUtilsException(message) {}
};


// Example of how error codes could be integrated, though not strictly required by "simple exception classes"
// enum class ErrorCode {
//     SUCCESS = 0,
//     UNKNOWN_ERROR = 1,
//     INVALID_ARGUMENT = 100,
//     PARSE_ERROR_GENERAL = 200,
//     PARSE_ERROR_BCD = 201,
//     // ... more specific codes
// };

// If using error codes with exceptions, the exception class could hold an ErrorCode member.
// For example:
// class ParsingError : public CoreUtilsException {
// public:
//     ParsingError(ErrorCode code, const std::string& message)
//         : CoreUtilsException(message), _errorCode(code) {}
//     ErrorCode getErrorCode() const { return _errorCode; }
// private:
//     ErrorCode _errorCode;
// };
// For this task, simple exception classes are sufficient as per the plan.

} // namespace CoreUtils

#endif // ERROR_CODES_H
