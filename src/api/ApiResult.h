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
