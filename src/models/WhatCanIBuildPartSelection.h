#pragma once

#include <QMetaType>
#include <QString>
#include <optional>

struct WhatCanIBuildPartSelection
{
    QString partNumber;
    std::optional<int> rebrickableColorId;
    int quantity = 1;

    bool isValid() const
    {
        return !partNumber.trimmed().isEmpty() && quantity >= 1
            && (!rebrickableColorId || *rebrickableColorId >= 0);
    }
};

Q_DECLARE_METATYPE(WhatCanIBuildPartSelection)
