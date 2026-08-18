/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include <QString>

enum class ApiErrorType
{
    None,
    Configuration,
    Authentication,
    Network,
    Timeout,
    RateLimit,
    Provider,
    InvalidResponse,
    Cancelled,
    Unknown
};

struct ApiError
{
    ApiErrorType type = ApiErrorType::None;
    QString message;
    QString providerMessage;
    int httpStatusCode = 0;
    int providerErrorCode = 0;
};
