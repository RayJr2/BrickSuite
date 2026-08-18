/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "ApiError.h"

#include <utility>

template<typename T>
struct ApiResult
{
    bool success = false;
    T value {};
    ApiError error;

    static ApiResult<T> ok(T resultValue)
    {
        ApiResult<T> result;
        result.success = true;
        result.value = std::move(resultValue);
        return result;
    }

    static ApiResult<T> failed(ApiError resultError)
    {
        ApiResult<T> result;
        result.success = false;
        result.error = std::move(resultError);
        return result;
    }
};
