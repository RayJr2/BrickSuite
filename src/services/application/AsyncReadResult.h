#pragma once

#include <QString>
#include <atomic>
#include <functional>
#include <optional>

enum class AsyncReadError
{
    None,
    NotFound,
    InvalidRequest,
    Unavailable,
    Unsupported,
    Timeout,
    ServerBusy,
    InternalFailure
};

using ReadRequestToken = quint64;

inline ReadRequestToken nextReadRequestToken()
{
    static std::atomic<ReadRequestToken> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
}

template <typename T>
struct AsyncReadResult
{
    ReadRequestToken token = 0;
    AsyncReadError error = AsyncReadError::None;
    QString message;
    std::optional<T> value;

    bool succeeded() const { return error == AsyncReadError::None && value.has_value(); }
    static AsyncReadResult success(ReadRequestToken token, T value)
    { return {token, AsyncReadError::None, {}, std::move(value)}; }
    static AsyncReadResult failure(ReadRequestToken token, AsyncReadError error,
                                   const QString& message)
    { return {token, error, message, std::nullopt}; }
};

template <typename T>
using AsyncReadCompletion = std::function<void(AsyncReadResult<T>)>;
