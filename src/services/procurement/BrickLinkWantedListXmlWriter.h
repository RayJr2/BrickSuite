/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "../../models/procurement/BrickLinkWantedListOptions.h"
#include "../../models/procurement/ProcurementDraft.h"

#include <QString>

class BrickLinkWantedListXmlWriter
{
public:
    struct Result
    {
        bool success = false;
        QString xml;
        QString message;

        int itemRows = 0;
        int totalPieces = 0;
    };

    Result write(const ProcurementDraft& draft,
                 const BrickLinkWantedListOptions& options) const;

private:
    QString remarksForDraft(const ProcurementDraft& draft,
                            const BrickLinkWantedListOptions& options) const;
};
