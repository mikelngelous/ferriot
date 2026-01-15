#pragma once

// Ferriot - Result Type
// Type-safe error handling inspired by Rust Result<T, E>

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace lwm2m {

// Error codes aligned with CoAP response codes
enum class ErrorCode {
    Success = 0,

    // Client errors (4.xx)
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    NotAcceptable = 406,
    RequestEntityIncomplete = 408,
    PreconditionFailed = 412,
    RequestEntityTooLarge = 413,
    UnsupportedContentFormat = 415,

    // Server errors (5.xx)
    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnavailable = 503,
    GatewayTimeout = 504,

    // Transport errors (6.xx - custom)
    Timeout = 600,
    ConnectionFailed = 601,
    DtlsHandshakeFailed = 602,
    NetworkUnreachable = 603,

    // Internal errors (7.xx - custom)
    EncodingError = 700,
    DecodingError = 701,
    InvalidState = 702,
    ResourceBusy = 703,
};

// Error with code and optional message
class Error {
public:
    explicit Error(ErrorCode code, std::string message = {}) noexcept
        : code_(code), message_(std::move(message)) {}

    [[nodiscard]] ErrorCode code() const noexcept { return code_; }
    [[nodiscard]] std::string_view message() const noexcept { return message_; }

    [[nodiscard]] bool is_client_error() const noexcept {
        auto c = static_cast<int>(code_);
        return c >= 400 && c < 500;
    }

    [[nodiscard]] bool is_server_error() const noexcept {
        auto c = static_cast<int>(code_);
        return c >= 500 && c < 600;
    }

    [[nodiscard]] bool is_transport_error() const noexcept {
        auto c = static_cast<int>(code_);
        return c >= 600 && c < 700;
    }

private:
    ErrorCode code_;
    std::string message_;
};

// Result<T> - success value or error
template<typename T>
class Result {
public:
    // Implicit construction from success value
    Result(T value) noexcept(std::is_nothrow_move_constructible_v<T>)
        : data_(std::move(value)) {}

    // Implicit construction from error
    Result(Error error) noexcept
        : data_(std::move(error)) {}

    // Construction from ErrorCode (convenience)
    Result(ErrorCode code, std::string message = {}) noexcept
        : data_(Error(code, std::move(message))) {}

    // Check if result is success
    [[nodiscard]] bool is_ok() const noexcept {
        return std::holds_alternative<T>(data_);
    }

    // Check if result is error
    [[nodiscard]] bool is_err() const noexcept {
        return std::holds_alternative<Error>(data_);
    }

    // Get value (throws if error)
    [[nodiscard]] T& value() & {
        if (is_err()) {
            throw std::runtime_error("Result::value() called on error");
        }
        return std::get<T>(data_);
    }

    [[nodiscard]] const T& value() const& {
        if (is_err()) {
            throw std::runtime_error("Result::value() called on error");
        }
        return std::get<T>(data_);
    }

    [[nodiscard]] T&& value() && {
        if (is_err()) {
            throw std::runtime_error("Result::value() called on error");
        }
        return std::get<T>(std::move(data_));
    }

    // Get error (throws if success)
    [[nodiscard]] const Error& error() const& {
        if (is_ok()) {
            throw std::runtime_error("Result::error() called on success");
        }
        return std::get<Error>(data_);
    }

    // Get value or default
    [[nodiscard]] T value_or(T default_value) const& {
        if (is_ok()) {
            return std::get<T>(data_);
        }
        return default_value;
    }

    // Map success value to new type
    template<typename F>
    [[nodiscard]] auto map(F&& f) const& -> Result<std::invoke_result_t<F, const T&>> {
        using U = std::invoke_result_t<F, const T&>;
        if (is_ok()) {
            return Result<U>(std::forward<F>(f)(std::get<T>(data_)));
        }
        return Result<U>(std::get<Error>(data_));
    }

    // Chain operations (flatMap)
    template<typename F>
    [[nodiscard]] auto and_then(F&& f) const& -> std::invoke_result_t<F, const T&> {
        using ResultU = std::invoke_result_t<F, const T&>;
        if (is_ok()) {
            return std::forward<F>(f)(std::get<T>(data_));
        }
        return ResultU(std::get<Error>(data_));
    }

    // Explicit bool conversion
    explicit operator bool() const noexcept {
        return is_ok();
    }

private:
    std::variant<T, Error> data_;
};

// Specialization for void (Result<void>)
template<>
class Result<void> {
public:
    // Success construction
    Result() noexcept : error_(std::nullopt) {}

    // Error construction
    Result(Error error) noexcept : error_(std::move(error)) {}
    Result(ErrorCode code, std::string message = {}) noexcept
        : error_(Error(code, std::move(message))) {}

    [[nodiscard]] bool is_ok() const noexcept { return !error_.has_value(); }
    [[nodiscard]] bool is_err() const noexcept { return error_.has_value(); }

    [[nodiscard]] const Error& error() const& {
        if (!error_.has_value()) {
            throw std::runtime_error("Result::error() called on success");
        }
        return *error_;
    }

    explicit operator bool() const noexcept { return is_ok(); }

private:
    std::optional<Error> error_;
};

// Helper to create success result
template<typename T>
[[nodiscard]] inline Result<T> Ok(T value) {
    return Result<T>(std::move(value));
}

// Helper to create void success
[[nodiscard]] inline Result<void> Ok() {
    return Result<void>();
}

// Helper to create error result
template<typename T = void>
[[nodiscard]] inline Result<T> Err(ErrorCode code, std::string message = {}) {
    return Result<T>(code, std::move(message));
}

template<typename T = void>
[[nodiscard]] inline Result<T> Err(Error error) {
    return Result<T>(std::move(error));
}

} // namespace lwm2m
