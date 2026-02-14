#pragma once

#include <string>
#include <variant>
#include <type_traits>
#include <cassert>

namespace stv::core {

/// Rust-style Result<T, E> for explicit error handling.
/// Replaces bool returns and exceptions with a type-safe discriminated union.
template<typename T, typename E>
class Result {
public:
    /// Construct a success result.
    static Result Ok(T value) {
        Result r;
        r.storage_ = std::move(value);
        return r;
    }

    /// Construct an error result.
    static Result Err(E error) {
        Result r;
        r.storage_ = std::move(error);
        return r;
    }

    [[nodiscard]] bool is_ok() const noexcept {
        return std::holds_alternative<T>(storage_);
    }

    [[nodiscard]] bool is_err() const noexcept {
        return std::holds_alternative<E>(storage_);
    }

    /// Access the success value. UB if is_err().
    [[nodiscard]] const T& value() const& {
        assert(is_ok() && "Result::value() called on Err");
        return std::get<T>(storage_);
    }

    [[nodiscard]] T&& value() && {
        assert(is_ok() && "Result::value() called on Err");
        return std::get<T>(std::move(storage_));
    }

    /// Access the error value. UB if is_ok().
    [[nodiscard]] const E& error() const& {
        assert(is_err() && "Result::error() called on Ok");
        return std::get<E>(storage_);
    }

    [[nodiscard]] E&& error() && {
        assert(is_err() && "Result::error() called on Ok");
        return std::get<E>(std::move(storage_));
    }

private:
    Result() = default;
    std::variant<T, E> storage_;
};

/// Specialization for Result<void, E> — success carries no value.
template<typename E>
class Result<void, E> {
public:
    static Result Ok() {
        Result r;
        r.has_error_ = false;
        return r;
    }

    static Result Err(E error) {
        Result r;
        r.has_error_ = true;
        r.error_ = std::move(error);
        return r;
    }

    [[nodiscard]] bool is_ok() const noexcept { return !has_error_; }
    [[nodiscard]] bool is_err() const noexcept { return has_error_; }

    [[nodiscard]] const E& error() const& {
        assert(is_err() && "Result<void,E>::error() called on Ok");
        return error_;
    }

private:
    Result() = default;
    bool has_error_ = false;
    E error_{};
};

} // namespace stv::core
