#include "PickABrickExportService.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <limits>

namespace {
QString normalizedNumber(const QString& value)
{
    qsizetype first = 0;
    while (first + 1 < value.size() && value.at(first) == QLatin1Char('0')) ++first;
    return value.mid(first);
}

bool numericLess(const QString& left, const QString& right)
{
    const QString normalizedLeft = normalizedNumber(left);
    const QString normalizedRight = normalizedNumber(right);
    if (normalizedLeft.size() != normalizedRight.size())
        return normalizedLeft.size() < normalizedRight.size();
    const int comparison = QString::compare(normalizedLeft, normalizedRight, Qt::CaseSensitive);
    if (comparison != 0) return comparison < 0;
    return QString::compare(left, right, Qt::CaseSensitive) < 0;
}
}

bool PickABrickExportService::isValidElementId(const QString& elementId)
{
    const QString value = elementId.trimmed();
    if (value.isEmpty()) return false;
    for (const QChar character : value)
        if (!character.isDigit()) return false;
    return normalizedNumber(value) != QStringLiteral("0");
}

QStringList PickABrickExportService::numericCandidateOrder(const QStringList& candidates)
{
    QStringList result;
    QSet<QString> seen;
    for (const QString& candidate : candidates) {
        const QString value = candidate.trimmed();
        if (value.isEmpty() || seen.contains(value)) continue;
        seen.insert(value);
        result.append(value);
    }
    std::sort(result.begin(), result.end(), [](const QString& left, const QString& right) {
        const bool leftNumeric = isValidElementId(left);
        const bool rightNumeric = isValidElementId(right);
        if (leftNumeric != rightNumeric) return leftNumeric;
        if (leftNumeric) return numericLess(left, right);
        return QString::compare(left, right, Qt::CaseInsensitive) < 0;
    });
    return result;
}

QString PickABrickExportService::suggestedElementId(const QStringList& candidates)
{
    const QStringList ordered = numericCandidateOrder(candidates);
    for (auto it = ordered.crbegin(); it != ordered.crend(); ++it)
        if (isValidElementId(*it)) return *it;
    return {};
}

QList<PickABrickExportSourceRow> PickABrickExportService::createSourceRows(
    const QList<MissingPartsExportRow>& rows)
{
    QList<PickABrickExportSourceRow> result;
    result.reserve(rows.size());
    for (const MissingPartsExportRow& row : rows) {
        PickABrickExportSourceRow source;
        source.source = row;
        source.elementCandidates = numericCandidateOrder(row.pickABrickElementCandidates);
        source.selectedElementId = suggestedElementId(source.elementCandidates);
        result.append(source);
    }
    return result;
}

PickABrickExportProjection PickABrickExportService::project(
    const QList<PickABrickExportSourceRow>& rows)
{
    PickABrickExportProjection result;
    QHash<QString, int> targetIndex;
    for (const PickABrickExportSourceRow& row : rows) {
        const qint64 quantity = row.source.missing;
        if (!row.included) {
            ++result.excludedSourceRows;
            if (quantity > 0 && result.excludedPieces <= std::numeric_limits<qint64>::max() - quantity)
                result.excludedPieces += quantity;
            continue;
        }

        ++result.includedSourceRows;
        if (quantity <= 0) {
            result.error = QStringLiteral("Included rows must have a positive missing quantity.");
            continue;
        }
        if (!isValidElementId(row.selectedElementId)
            || !row.elementCandidates.contains(row.selectedElementId)) {
            ++result.unresolvedIncludedRows;
            continue;
        }
        if (result.includedPieces > std::numeric_limits<qint64>::max() - quantity) {
            result.error = QStringLiteral("The included quantity is too large to export safely.");
            continue;
        }
        result.includedPieces += quantity;

        auto found = targetIndex.constFind(row.selectedElementId);
        if (found == targetIndex.cend()) {
            targetIndex.insert(row.selectedElementId, result.rows.size());
            result.rows.append({row.selectedElementId, quantity});
        } else {
            auto& target = result.rows[*found];
            if (target.quantity > std::numeric_limits<qint64>::max() - quantity) {
                result.error = QStringLiteral("An aggregated quantity is too large to export safely.");
                continue;
            }
            target.quantity += quantity;
        }
    }
    return result;
}
