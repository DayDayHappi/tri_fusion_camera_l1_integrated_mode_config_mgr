#pragma once

#include <string>
#include <utility>
#include <variant>
#include "foundation/error/ErrorCode.h"
#include "foundation/error/ErrorMessage.h"

namespace tri::foundation {

class Status {
public:
    Status() = default;
    Status(ErrorCode code, std::string message = {})
        : code_(code), message_(std::move(message)) {}

    static Status success() { return Status(ErrorCode::Ok); }
    static Status error(ErrorCode code, std::string message = {}) {
        return Status(code, std::move(message));
    }

    bool ok() const noexcept { return code_ == ErrorCode::Ok; }
    explicit operator bool() const noexcept { return ok(); }
    ErrorCode code() const noexcept { return code_; }
    const std::string& message() const noexcept { return message_; }
    std::string describe() const {
        if (!message_.empty()) return message_;
        return errorMessage(code_);
    }

private:
    ErrorCode code_{ErrorCode::Ok};
    std::string message_{};
};

template <typename T>
class Result {
public:
    Result(const T& value) : data_(value) {}
    Result(T&& value) : data_(std::move(value)) {}
    Result(Status status) : data_(std::move(status)) {}

    static Result<T> ok(T value) { return Result<T>(std::move(value)); }
    static Result<T> error(ErrorCode code, std::string message = {}) {
        return Result<T>(Status::error(code, std::move(message)));
    }

    bool ok() const noexcept { return std::holds_alternative<T>(data_); }
    explicit operator bool() const noexcept { return ok(); }

    const T& value() const { return std::get<T>(data_); }
    T& value() { return std::get<T>(data_); }
    T take() { return std::move(std::get<T>(data_)); }

    Status status() const {
        if (ok()) return Status::success();
        return std::get<Status>(data_);
    }

private:
    std::variant<T, Status> data_;
};

template <>
class Result<void> {
public:
    Result() = default;
    Result(Status status) : status_(std::move(status)) {}

    static Result<void> success() { return Result<void>(); }
    static Result<void> error(ErrorCode code, std::string message = {}) {
        return Result<void>(Status::error(code, std::move(message)));
    }

    bool ok() const noexcept { return status_.ok(); }
    explicit operator bool() const noexcept { return ok(); }
    Status status() const { return status_; }

private:
    Status status_{Status::success()};
};

} // namespace tri::foundation
