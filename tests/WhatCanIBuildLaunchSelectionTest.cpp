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

#include "../src/models/WhatCanIBuildPartSelection.h"

#include <QCoreApplication>
#include <cstdio>

namespace {
bool require(bool value, const char* message)
{
    if (!value) std::fprintf(stderr, "%s\n", message);
    return value;
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);

    const WhatCanIBuildPartSelection anyColor{QStringLiteral("3001"), std::nullopt, 1};
    if (!require(anyColor.isValid(), "canonical Any Color selection is valid")
        || !require(anyColor.partNumber == QStringLiteral("3001"),
                    "canonical Part number is preserved")
        || !require(!anyColor.rebrickableColorId.has_value(),
                    "Any Color is represented without a provider Color ID")
        || !require(anyColor.quantity == 1, "launch quantity defaults to one"))
        return 1;

    const WhatCanIBuildPartSelection exactColor{QStringLiteral("3001"), 15, 1};
    if (!require(exactColor.isValid(), "exact Rebrickable Color selection is valid")
        || !require(exactColor.rebrickableColorId == 15,
                    "exact provider Color identity is preserved"))
        return 1;

    if (!require(!WhatCanIBuildPartSelection{QString(), std::nullopt, 1}.isValid(),
                 "empty Part identity is rejected")
        || !require(!WhatCanIBuildPartSelection{QStringLiteral("3001"), std::nullopt, 0}.isValid(),
                    "non-positive quantity is rejected")
        || !require(!WhatCanIBuildPartSelection{QStringLiteral("3001"), -1, 1}.isValid(),
                    "invalid provider Color identity is rejected"))
        return 1;

    return 0;
}
