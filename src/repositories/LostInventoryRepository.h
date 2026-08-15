#pragma once

#include "../models/LostInventoryItem.h"

#include <QList>
#include <optional>

class LostInventoryRepository
{
public:
    QList<LostInventoryItem> getOutstanding(int workspaceId) const;

    std::optional<LostInventoryItem> getOutstandingForPartColor(int workspaceId,
                                                                int partId,
                                                                int colorId) const;
};
