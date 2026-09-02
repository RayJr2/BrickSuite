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
