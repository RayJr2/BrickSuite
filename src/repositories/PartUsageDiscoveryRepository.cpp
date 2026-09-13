#include "PartUsageDiscoveryRepository.h"

#include <QHash>
#include <QElapsedTimer>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <algorithm>

namespace {
struct SetAccumulator {
    PartUsageSetResult result;
    QHash<int, QHash<int, qint64>> quantities;
};

QList<PartUsageCriterion> normalized(const QList<PartUsageCriterion>& input)
{
    QList<PartUsageCriterion> result;
    for (const auto& value : input) {
        auto it = std::find_if(result.begin(), result.end(), [&](const auto& existing) {
            return existing.partId == value.partId && existing.colorId == value.colorId;
        });
        if (it == result.end()) result.append(value);
        else it->quantity += value.quantity;
    }
    return result;
}
}

PartUsageSearchResult PartUsageDiscoveryRepository::search(const PartUsageSearch& request) const
{
    QElapsedTimer timer;
    timer.start();
    PartUsageSearchResult output;
    const QList<PartUsageCriterion> criteria = normalized(request.criteria);
    if (criteria.isEmpty() || criteria.size() > 20) {
        output.errorMessage = QStringLiteral("Select between 1 and 20 valid Parts.");
        return output;
    }
    for (const auto& criterion : criteria) {
        if (criterion.partId <= 0 || criterion.quantity <= 0) {
            output.errorMessage = QStringLiteral("A selected Part criterion is invalid.");
            return output;
        }
    }

    QStringList placeholders;
    for (int i = 0; i < criteria.size(); ++i) placeholders.append(QStringLiteral(":part%1").arg(i));
    const QString sql = QStringLiteral(R"(
        WITH effective_parts AS (
            SELECT sir.set_catalog_id set_id,sip.part_id,sip.color_id,sip.quantity
              FROM set_inventory_revision sir
              JOIN set_inventory_part sip ON sip.set_inventory_revision_id=sir.id
             WHERE sir.provider='Rebrickable' AND sir.is_active=1 AND sir.is_preferred=1
               AND sip.is_spare=0
            UNION ALL
            SELECT scp.set_catalog_id,scp.part_id,scp.color_id,scp.quantity_required
              FROM set_catalog_part scp
             WHERE scp.is_spare=0 AND NOT EXISTS (
                SELECT 1 FROM set_inventory_revision sir
                 WHERE sir.set_catalog_id=scp.set_catalog_id AND sir.provider='Rebrickable'
                   AND sir.is_active=1 AND sir.is_preferred=1)
        ), relevant AS (
            SELECT set_id,part_id,color_id,SUM(quantity) quantity
              FROM effective_parts WHERE part_id IN (%1)
             GROUP BY set_id,part_id,color_id
        )
        SELECT sc.id,sc.set_number,sc.name,sc.year,sc.theme_id,
               COALESCE(t.name,''),sc.image_url,sc.num_parts,
               r.part_id,r.color_id,r.quantity
          FROM relevant r JOIN set_catalog sc ON sc.id=r.set_id
          LEFT JOIN theme_catalog t ON t.id=sc.theme_id
         WHERE (:text='' OR sc.set_number LIKE :setNumberPattern
                OR sc.name LIKE :setNamePattern)
         ORDER BY sc.id,r.part_id,r.color_id
    )").arg(placeholders.join(','));

    QSqlQuery query(repositoryDatabase());
    if (!query.prepare(sql)) {
        output.errorMessage = query.lastError().text();
        return output;
    }
    for (int i = 0; i < criteria.size(); ++i) query.bindValue(placeholders.at(i), criteria.at(i).partId);
    const QString text = request.text.trimmed();
    query.bindValue(QStringLiteral(":text"), text);
    const QString pattern = QStringLiteral("%%1%").arg(text);
    query.bindValue(QStringLiteral(":setNumberPattern"), pattern);
    query.bindValue(QStringLiteral(":setNamePattern"), pattern);
    if (!query.exec()) {
        output.errorMessage = query.lastError().text();
        return output;
    }

    QList<SetAccumulator> candidates;
    int currentId = 0;
    while (query.next()) {
        const int id = query.value(0).toInt();
        if (id != currentId) {
            currentId = id;
            SetAccumulator value;
            value.result = {id, query.value(1).toString(), query.value(2).toString(),
                query.value(3).toInt(), query.value(4).toInt(), query.value(5).toString(),
                query.value(6).toString(), query.value(7).toInt(), 0,
                static_cast<int>(criteria.size())};
            candidates.append(value);
        }
        candidates.last().quantities[query.value(8).toInt()][query.value(9).toInt()]
            += query.value(10).toLongLong();
    }

    for (auto& candidate : candidates) {
        QHash<int, QList<PartUsageCriterion>> criteriaByPart;
        for (const auto& criterion : criteria) {
            criteriaByPart[criterion.partId].append(criterion);
        }

        int matchedCriteria = 0;
        for (auto partIt = criteriaByPart.cbegin(); partIt != criteriaByPart.cend(); ++partIt) {
            const auto colors = candidate.quantities.value(partIt.key());
            qint64 totalAvailable = 0;
            for (qint64 quantity : colors) totalAvailable += quantity;

            int exactMatched = 0;
            qint64 exactConsumption = 0;
            int anyDemand = 0;
            for (const auto& criterion : partIt.value()) {
                if (criterion.colorId == 0) {
                    anyDemand = criterion.quantity;
                } else if (colors.value(criterion.colorId) >= criterion.quantity) {
                    ++exactMatched;
                    exactConsumption += criterion.quantity;
                }
            }

            int partMatched = exactMatched;
            if (anyDemand > 0 && totalAvailable >= anyDemand) {
                if (totalAvailable - exactConsumption >= anyDemand)
                    ++partMatched;
                else
                    partMatched = qMax(partMatched, 1);
            }
            matchedCriteria += partMatched;
        }

        candidate.result.matchedCriteria = matchedCriteria;
        if ((request.matchMode == PartUsageMatchMode::All
             && matchedCriteria == criteria.size())
            || (request.matchMode == PartUsageMatchMode::Any && matchedCriteria > 0))
            output.sets.append(candidate.result);
    }

    std::sort(output.sets.begin(), output.sets.end(), [mode=request.matchMode](const auto& a, const auto& b) {
        if (mode == PartUsageMatchMode::Any && a.matchedCriteria != b.matchedCriteria)
            return a.matchedCriteria > b.matchedCriteria;
        return a.setNumber.compare(b.setNumber, Qt::CaseInsensitive) < 0;
    });
    output.qualifyingCount = output.sets.size();
    const int cap = qBound(50, request.maximumResults, 2000);
    output.capReached = output.sets.size() > cap;
    if (output.capReached) output.sets = output.sets.mid(0, cap);
    output.success = true;
#ifndef NDEBUG
    qDebug() << "Part Usage discovery completed. Criteria:" << criteria.size()
             << "candidates:" << candidates.size() << "qualifying:"
             << output.qualifyingCount << "returned:" << output.sets.size()
             << "cap reached:" << output.capReached << "elapsed ms:" << timer.elapsed();
#endif
    return output;
}
