#include "SetCompositionReplacementService.h"

#include "../../database/DatabaseManager.h"
#include "../../models/SetCatalogPart.h"
#include "../../repositories/SetCatalogPartRepository.h"

#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>
#include <limits>
#include <tuple>

namespace {
QString logicalKey(int partId, int colorId, bool spare)
{ return QString("%1|%2|%3").arg(partId).arg(colorId).arg(spare ? 1 : 0); }
}

SetCompositionReplacementService::Result SetCompositionReplacementService::replace(
    int setCatalogId, const QList<InputRow>& rows,
    const QString& provider, const QString& source) const
{
    Result result;
    if (setCatalogId <= 0 || rows.isEmpty()) {
        result.message = QStringLiteral("The Set composition contains no rows.");
        return result;
    }
    if (provider.trimmed().isEmpty() || source.trimmed().isEmpty()) {
        result.message = QStringLiteral("Set composition provenance is missing. No changes were saved.");
        return result;
    }
    QSqlDatabase database = DatabaseManager::instance().database();
    QHash<QString, int> partIds;
    QSqlQuery partQuery(database);
    if (!partQuery.exec("SELECT id,rebrickable_part_id FROM part WHERE rebrickable_part_id IS NOT NULL")) {
        result.message = QString("Unable to read the Part Catalog: %1").arg(partQuery.lastError().text());
        return result;
    }
    while (partQuery.next()) {
        const QString external = partQuery.value(1).toString().trimmed();
        const QString key = external.toCaseFolded();
        const int id = partQuery.value(0).toInt();
        if (partIds.contains(key) && partIds.value(key) != id) {
            result.message = QString("The Part Catalog contains an ambiguous Rebrickable Part identity: %1").arg(external);
            return result;
        }
        partIds.insert(key, id);
    }
    QHash<int, int> colorIds;
    QSqlQuery colorQuery(database);
    if (!colorQuery.exec("SELECT id,rebrickable_id FROM color WHERE rebrickable_id IS NOT NULL")) {
        result.message = QString("Unable to read the Color Catalog: %1").arg(colorQuery.lastError().text());
        return result;
    }
    while (colorQuery.next()) {
        const int external = colorQuery.value(1).toInt();
        const int id = colorQuery.value(0).toInt();
        if (colorIds.contains(external) && colorIds.value(external) != id) {
            result.message = QString("The Color Catalog contains an ambiguous Rebrickable Color identity: %1").arg(external);
            return result;
        }
        colorIds.insert(external, id);
    }

    QHash<QString, SetCatalogPart> pending;
    QStringList unresolved;
    for (const InputRow& row : rows) {
        const QString number = row.rebrickablePartNumber.trimmed();
        if (number.isEmpty() || row.rebrickableColorId < 0 || row.quantity <= 0) {
            result.message = QString("Invalid Set composition row%1. No changes were saved.")
                                 .arg(row.context.isEmpty() ? QString() : " " + row.context);
            return result;
        }
        const int partId = partIds.value(number.toCaseFolded(), 0);
        const int colorId = colorIds.value(row.rebrickableColorId, 0);
        if (partId <= 0 || colorId <= 0) {
            unresolved << QString("%1Part %2, Color %3 (%4%5)")
                .arg(row.context.isEmpty() ? QString() : row.context + ": ", number)
                .arg(row.rebrickableColorId)
                .arg(partId <= 0 ? "unresolved Part" : "")
                .arg(partId <= 0 && colorId <= 0 ? ", unresolved Color" : colorId <= 0 ? "unresolved Color" : "");
            continue;
        }
        const QString key = logicalKey(partId, colorId, row.isSpare);
        if (pending.contains(key)) {
            const qint64 combined = qint64(pending[key].quantityRequired) + row.quantity;
            if (combined > std::numeric_limits<int>::max()) {
                result.message = QStringLiteral("Combined Set composition quantity is too large. No changes were saved.");
                return result;
            }
            pending[key].quantityRequired = int(combined);
        } else {
            SetCatalogPart part;
            part.setCatalogId = setCatalogId; part.partId = partId; part.colorId = colorId;
            part.quantityRequired = row.quantity; part.isSpare = row.isSpare;
            part.provider = provider; part.source = source;
            pending.insert(key, part);
        }
    }
    if (!unresolved.isEmpty()) {
        result.message = QString("Some Part or Color identities could not be resolved:\n%1\nNo changes were saved.")
                             .arg(unresolved.join('\n'));
        return result;
    }
    QList<SetCatalogPart> composition = pending.values();
    std::sort(composition.begin(), composition.end(), [](const auto& a, const auto& b) {
        return std::tie(a.isSpare,a.partId,a.colorId) < std::tie(b.isSpare,b.partId,b.colorId);
    });
    qint64 required = 0, spare = 0;
    for (const auto& part : composition) {
        qint64& total = part.isSpare ? spare : required;
        total += part.quantityRequired;
        if (total > std::numeric_limits<int>::max()) {
            result.message = QStringLiteral("The total Set composition quantity is too large. No changes were saved.");
            return result;
        }
    }
    if (!database.transaction()) { result.message = QStringLiteral("Unable to begin Set composition transaction."); return result; }
    SetCatalogPartRepository repository;
    QString error;
    if (!repository.replaceForSet(setCatalogId, composition, error)) {
        database.rollback(); result.message = QString("Unable to replace the Set composition: %1. No changes were saved.").arg(error); return result;
    }
    if (!database.commit()) {
        database.rollback(); result.message = QString("Unable to commit the Set composition: %1").arg(database.lastError().text()); return result;
    }
    result.success = true; result.compositionRows = composition.size();
    result.requiredPieces = int(required); result.sparePieces = int(spare);
    result.message = QStringLiteral("Set composition replaced successfully.");
    return result;
}
