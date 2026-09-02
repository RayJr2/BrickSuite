#include "MinifigCompositionReplacementService.h"

#include "../../database/DatabaseManager.h"
#include "../../models/MinifigCatalogPart.h"
#include "../../repositories/MinifigCatalogPartRepository.h"

#include <QHash>
#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>
#include <limits>
#include <tuple>

namespace {
QString logicalKey(int partId, int colorId, bool isSpare)
{
    return QString("%1|%2|%3").arg(partId).arg(colorId).arg(isSpare ? 1 : 0);
}
}

MinifigCompositionReplacementService::Result
MinifigCompositionReplacementService::replace(int minifigCatalogId,
                                               const QList<InputRow>& rows,
                                               const QString& provider,
                                               const QString& source) const
{
    Result result;
    if (minifigCatalogId <= 0 || rows.isEmpty()) {
        result.message = QStringLiteral("The Minifig composition contains no rows.");
        return result;
    }

    QSqlDatabase database = DatabaseManager::instance().database();
    QHash<QString, int> partIds;
    QSqlQuery partsQuery(database);
    if (!partsQuery.exec("SELECT id, rebrickable_part_id FROM part "
                         "WHERE rebrickable_part_id IS NOT NULL")) {
        result.message = QString("Unable to read the Part Catalog: %1")
                             .arg(partsQuery.lastError().text());
        return result;
    }
    while (partsQuery.next()) {
        const QString providerId = partsQuery.value(1).toString().trimmed();
        const QString normalized = providerId.toCaseFolded();
        const int partId = partsQuery.value(0).toInt();
        if (partIds.contains(normalized) && partIds.value(normalized) != partId) {
            result.message = QString("The Part Catalog contains an ambiguous Rebrickable "
                                     "Part identity: %1")
                                 .arg(providerId);
            return result;
        }
        partIds.insert(normalized, partId);
    }

    QHash<int, int> colorIds;
    QSqlQuery colorsQuery(database);
    if (!colorsQuery.exec("SELECT id, rebrickable_id FROM color "
                          "WHERE rebrickable_id IS NOT NULL")) {
        result.message = QString("Unable to read the Color Catalog: %1")
                             .arg(colorsQuery.lastError().text());
        return result;
    }
    while (colorsQuery.next())
        colorIds.insert(colorsQuery.value(1).toInt(), colorsQuery.value(0).toInt());

    QHash<QString, MinifigCatalogPart> pending;
    QStringList unresolved;
    for (const InputRow& row : rows) {
        const QString partNumber = row.rebrickablePartNumber.trimmed();
        if (partNumber.isEmpty() || row.rebrickableColorId < 0 || row.quantity <= 0) {
            result.message = QString("Invalid Minifig composition row%1. No changes were saved.")
                                 .arg(row.context.isEmpty() ? QString() : " " + row.context);
            return result;
        }
        const int partId = partIds.value(partNumber.toCaseFolded(), 0);
        const int colorId = colorIds.value(row.rebrickableColorId, 0);
        if (partId <= 0 || colorId <= 0) {
            QStringList issues;
            if (partId <= 0) issues.append(QStringLiteral("unresolved Part"));
            if (colorId <= 0) issues.append(QStringLiteral("unresolved Color"));
            unresolved.append(QString("%1Part %2, Color %3 (%4)")
                                  .arg(row.context.isEmpty() ? QString() : row.context + ": ")
                                  .arg(partNumber)
                                  .arg(row.rebrickableColorId)
                                  .arg(issues.join(", ")));
            continue;
        }

        const QString key = logicalKey(partId, colorId, row.isSpare);
        if (pending.contains(key)) {
            const qint64 combined = static_cast<qint64>(pending[key].quantityRequired)
                                    + row.quantity;
            if (combined > std::numeric_limits<int>::max()) {
                result.message = QString("Combined quantity is too large%1. No changes were saved.")
                                     .arg(row.context.isEmpty() ? QString() : " at " + row.context);
                return result;
            }
            pending[key].quantityRequired = static_cast<int>(combined);
        } else {
            MinifigCatalogPart part;
            part.minifigCatalogId = minifigCatalogId;
            part.partId = partId;
            part.colorId = colorId;
            part.quantityRequired = row.quantity;
            part.isSpare = row.isSpare;
            part.provider = provider;
            part.source = source;
            pending.insert(key, part);
        }
    }
    if (!unresolved.isEmpty()) {
        result.message = QString("Some Part or Color identities could not be resolved:\n%1\n"
                                 "No changes were saved.")
                             .arg(unresolved.join('\n'));
        return result;
    }

    QList<MinifigCatalogPart> composition = pending.values();
    std::sort(composition.begin(), composition.end(), [](const auto& left, const auto& right) {
        return std::tie(left.isSpare, left.partId, left.colorId)
               < std::tie(right.isSpare, right.partId, right.colorId);
    });

    qint64 required = 0;
    qint64 spare = 0;
    for (const MinifigCatalogPart& part : composition) {
        qint64& total = part.isSpare ? spare : required;
        total += part.quantityRequired;
        if (total > std::numeric_limits<int>::max()) {
            result.message = QStringLiteral("The total Minifig composition quantity is too large. "
                                            "No changes were saved.");
            return result;
        }
    }

    if (!database.transaction()) {
        result.message = QStringLiteral("Unable to begin Minifig composition transaction.");
        return result;
    }
    MinifigCatalogPartRepository repository;
    QString repositoryError;
    if (!repository.replaceForMinifig(minifigCatalogId, composition, repositoryError)) {
        database.rollback();
        result.message = QString("Unable to replace the Minifig composition: %1. "
                                 "No changes were saved.").arg(repositoryError);
        return result;
    }
    if (!database.commit()) {
        database.rollback();
        result.message = QString("Unable to commit the Minifig composition: %1")
                             .arg(database.lastError().text());
        return result;
    }

    result.success = true;
    result.compositionRows = composition.size();
    result.requiredPieces = static_cast<int>(required);
    result.sparePieces = static_cast<int>(spare);
    result.message = QStringLiteral("Minifig composition replaced successfully.");
    return result;
}
