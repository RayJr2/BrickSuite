#include "RebrickableMinifigPartsService.h"

#include "MinifigCompositionReplacementService.h"

RebrickableMinifigPartsService::RebrickableMinifigPartsService(QObject* parent)
    : QObject(parent)
    , m_api(new RebrickableService(this))
{
    connect(m_api, &RebrickableService::minifigPartsFinished, this,
            [this](const RebrickableService::MinifigPartsResult& apiResult) {
        if (!m_busy || apiResult.figNumber.compare(m_figNumber, Qt::CaseInsensitive) != 0)
            return;

        Result result;
        result.figNumber = m_figNumber;
        if (!apiResult.success) {
            m_busy = false;
            result.message = apiResult.message;
            emit finished(result);
            return;
        }

        QList<MinifigCompositionReplacementService::InputRow> rows;
        rows.reserve(apiResult.parts.size());
        for (int index = 0; index < apiResult.parts.size(); ++index) {
            const auto& apiPart = apiResult.parts.at(index);
            MinifigCompositionReplacementService::InputRow row;
            row.rebrickablePartNumber = apiPart.partNumber;
            row.rebrickableColorId = apiPart.rebrickableColorId;
            row.quantity = apiPart.quantity;
            row.isSpare = apiPart.isSpare;
            row.context = QString("API row %1").arg(index + 1);
            rows.append(row);
        }

        MinifigCompositionReplacementService replacementService;
        const auto replacement = replacementService.replace(
            m_minifigCatalogId, rows, QStringLiteral("Rebrickable"),
            QStringLiteral("Rebrickable API: Minifig parts"));
        m_busy = false;
        result.success = replacement.success;
        result.compositionRows = replacement.compositionRows;
        result.requiredPieces = replacement.requiredPieces;
        result.sparePieces = replacement.sparePieces;
        result.message = replacement.message;
        emit finished(result);
    });
}

bool RebrickableMinifigPartsService::isBusy() const
{
    return m_busy;
}

void RebrickableMinifigPartsService::retrieveAndReplace(int minifigCatalogId,
                                                        const QString& figNumber,
                                                        const QString& apiKey)
{
    if (m_busy)
        return;
    m_minifigCatalogId = minifigCatalogId;
    m_figNumber = figNumber.trimmed();
    m_busy = true;
    m_api->getMinifigParts(m_figNumber, apiKey);
}
