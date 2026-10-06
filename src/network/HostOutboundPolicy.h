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

#include "BrickSuiteProtocol.h"

class HostOutboundPolicy
{
public:
    static constexpr qint64 HighWaterBytes = 2 * 1024 * 1024;
    static constexpr qint64 HardLimitBytes = 4 * 1024 * 1024;
    static constexpr int SustainedBackpressureMs = 10000;

    enum class Decision { Send, EnterBackpressure, Disconnect };

    static Decision evaluate(qint64 bufferedBytes, qsizetype messageBytes);
    static QByteArray serializeForSend(const BrickSuiteProtocol::Message& message,
                                       bool* replacedOversizedResponse,
                                       bool* rejectedOversizedEvent);
};
