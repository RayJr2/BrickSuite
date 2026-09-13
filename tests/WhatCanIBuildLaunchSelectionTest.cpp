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
