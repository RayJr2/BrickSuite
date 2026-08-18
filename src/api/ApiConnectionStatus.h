/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

enum class ApiConnectionStatus
{
    NotConfigured,
    Unknown,
    Testing,
    Connected,
    AuthenticationFailed,
    NetworkError,
    ProviderError
};
