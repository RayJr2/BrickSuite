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

#include "../../api/brickset/BricksetService.h"
#include "../../api/rebrickable/RebrickableService.h"

#include <QObject>
#include <QString>

class RebrickableApiClient;

class SetDetailsProviderService : public QObject
{
    Q_OBJECT

public:
    enum class Source
    {
        None,
        Brickset,
        Rebrickable
    };

    struct Result
    {
        bool hasEnrichment = false;
        bool bricksetAttempted = false;
        bool usedFallback = false;

        Source source = Source::None;

        QString setNumber;
        QString message;
        QString fallbackReason;

        BricksetService::SetDetails brickset;
        RebrickableService::SetDetails rebrickable;
    };

    explicit SetDetailsProviderService(QObject* parent = nullptr);

    void requestDetails(const QString& setNumber);

signals:
    void detailsReady(const SetDetailsProviderService::Result& result);

private:
    void requestPreferredProvider();
    void requestRebrickable(bool usedFallback,
                            const QString& fallbackReason = QString());

    QString m_setNumber;
    QString m_fallbackReason;
    bool m_bricksetAttempted = false;
    bool m_usedFallback = false;

    BricksetService* m_bricksetService = nullptr;
    RebrickableApiClient* m_rebrickableApiClient = nullptr;
};

Q_DECLARE_METATYPE(SetDetailsProviderService::Result)
