/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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
