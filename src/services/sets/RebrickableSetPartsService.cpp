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

#include "RebrickableSetPartsService.h"

#include "SetCompositionReplacementService.h"

RebrickableSetPartsService::RebrickableSetPartsService(QObject* parent)
    : QObject(parent), m_api(new RebrickableService(this))
{
    connect(m_api, &RebrickableService::setCatalogPartsFinished, this,
            [this](const RebrickableService::SetPartsResult& apiResult) {
        if (!m_busy || apiResult.setNumber.compare(m_setNumber, Qt::CaseInsensitive) != 0) return;
        Result result; result.setNumber = m_setNumber;
        if (!apiResult.success) { m_busy = false; result.message = apiResult.message; emit finished(result); return; }
        QList<SetCompositionReplacementService::InputRow> rows;
        rows.reserve(apiResult.parts.size());
        for (int index = 0; index < apiResult.parts.size(); ++index) {
            const auto& part = apiResult.parts.at(index);
            rows << SetCompositionReplacementService::InputRow{
                part.partNumber, part.rebrickableColorId, part.quantity, part.isSpare,
                QString("API row %1").arg(index + 1)};
        }
        const auto replaced = SetCompositionReplacementService().replace(
            m_setCatalogId, rows, QStringLiteral("Rebrickable"),
            QStringLiteral("Rebrickable API: Set parts (including Minifig parts)"));
        m_busy = false; result.success = replaced.success;
        result.compositionRows = replaced.compositionRows;
        result.requiredPieces = replaced.requiredPieces; result.sparePieces = replaced.sparePieces;
        result.message = replaced.message; emit finished(result);
    });
}

bool RebrickableSetPartsService::isBusy() const { return m_busy; }

void RebrickableSetPartsService::retrieveAndReplace(int setCatalogId,
                                                     const QString& setNumber,
                                                     const QString& apiKey)
{
    if (m_busy) return;
    m_setCatalogId = setCatalogId; m_setNumber = setNumber.trimmed(); m_busy = true;
    m_api->getSetCatalogParts(m_setNumber, apiKey);
}
