#include "RebrickableGlobalImportService.h"

#include "RebrickableDatasetRegistry.h"
#include "RebrickableImportPlanController.h"
#include "../RebrickableCsvInputResolver.h"
#include "../RebrickableMinifigCatalogImporter.h"
#include "../RebrickableInventoryCompositionImporter.h"
#include "../RebrickableElementImporter.h"
#include "../RebrickablePartCatalogImporter.h"
#include "../RebrickablePartRelationshipImporter.h"
#include "../RebrickableReferenceImporter.h"
#include "../RebrickableSetCatalogImporter.h"
#include "../RebrickableThemeCatalogImporter.h"
#include "../../services/minifigs/RebrickableMinifigThemeDerivationService.h"

#include <QElapsedTimer>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>
#include <QDebug>

namespace {
QStringList parseCsv(const QString& line, bool& ok)
{
    QStringList fields; QString field; bool quoted = false; ok = true;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == QChar('"')) {
            if (quoted && i + 1 < line.size() && line.at(i + 1) == QChar('"')) { field += ch; ++i; }
            else quoted = !quoted;
        } else if (ch == QChar(',') && !quoted) { fields.append(field); field.clear(); }
        else field += ch;
    }
    ok = !quoted; fields.append(field); return fields;
}

bool loadIdentitySet(QSqlDatabase& database, const QString& sql, QSet<QString>& values,
                     QString& error)
{
    QSqlQuery query(database);
    if (!query.exec(sql)) { error = query.lastError().text(); return false; }
    while (query.next()) values.insert(query.value(0).toString());
    return true;
}

bool loadPartIdentityMap(QSqlDatabase& database, QHash<QString, int>& values,
                         QString& error)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT part_number, id FROM part"))) {
        error = query.lastError().text();
        return false;
    }
    while (query.next())
        values.insert(query.value(0).toString(), query.value(1).toInt());
    return true;
}

bool validateSnapshot(const RebrickableImportPlanEntry& entry, QSqlDatabase& database,
                      const RebrickableImportCancellation& cancellation,
                      RebrickableImportCounters& counters, QString& error,
                      const RebrickableGlobalImportService::Progress& progress,
                      int datasetIndex, int datasetTotal)
{
    const QFileInfo source(entry.sourcePath);
    if (!source.exists() || source.size() != entry.sourceSize
        || source.lastModified().toMSecsSinceEpoch() != entry.sourceModifiedMilliseconds) {
        error = QStringLiteral("The source changed after preflight. Rescan before importing.");
        return false;
    }
    const auto* descriptor = RebrickableDatasetRegistry::descriptor(entry.dataset);
    QTemporaryDir temporary; QString path;
    if (!descriptor || !RebrickableCsvInputResolver::resolve(entry.sourcePath,
            descriptor->csvFileName, temporary, path, error)) return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { error = file.errorString(); return false; }
    QTextStream stream(&file); QString headerLine = stream.readLine();
    if (!headerLine.isEmpty() && headerLine.front() == QChar(0xfeff)) headerLine.remove(0, 1);
    bool ok = false; const QStringList headers = parseCsv(headerLine, ok);
    if (!ok) { error = QStringLiteral("The CSV header is malformed."); return false; }
    for (const QString& required : descriptor->requiredHeaders) if (!headers.contains(required)) {
        error = QStringLiteral("Missing required header: %1.").arg(required); return false;
    }
    if (descriptor->exactHeaders && headers != descriptor->requiredHeaders) {
        error = QStringLiteral("The CSV header does not match the required exact layout."); return false;
    }
    QSet<QString> categoryIds;
    QHash<QString, int> partIdsByNumber;
    if (entry.dataset == RebrickableDatasetId::Parts
        && !loadIdentitySet(database, QStringLiteral("SELECT rebrickable_id FROM part_category WHERE rebrickable_id IS NOT NULL"), categoryIds, error)) return false;
    if (entry.dataset == RebrickableDatasetId::PartRelationships
        && !loadPartIdentityMap(database, partIdsByNumber, error)) return false;
    const int partNum = headers.indexOf(QStringLiteral("part_num"));
    const int name = headers.indexOf(QStringLiteral("name"));
    const int category = headers.indexOf(QStringLiteral("part_cat_id"));
    const int rel = headers.indexOf(QStringLiteral("rel_type"));
    const int child = headers.indexOf(QStringLiteral("child_part_num"));
    const int parent = headers.indexOf(QStringLiteral("parent_part_num"));
    const int setNum = headers.indexOf(QStringLiteral("set_num"));
    const int year = headers.indexOf(QStringLiteral("year"));
    const int theme = headers.indexOf(QStringLiteral("theme_id"));
    const int count = headers.indexOf(entry.dataset == RebrickableDatasetId::Minifigs
                                          ? QStringLiteral("num_parts") : QStringLiteral("num_parts"));
    const int figNum = headers.indexOf(QStringLiteral("fig_num"));
    const int elementId = headers.indexOf(QStringLiteral("element_id"));
    QSet<QString> primaryIds;
    while (!stream.atEnd()) {
        const QString line = stream.readLine(); if (line.trimmed().isEmpty()) continue; ++counters.rowsRead;
        if ((counters.rowsRead & 255) == 0) {
            if (cancellation.isCancellationRequested()) { error = QStringLiteral("Import cancelled during validation."); return false; }
            if (progress) progress({entry.dataset, datasetIndex, datasetTotal, counters.rowsRead, -1,
                                    QStringLiteral("Validating"), QString(), 0, counters});
        }
        const QStringList fields = parseCsv(line, ok);
        if (!ok || fields.size() != headers.size()) { error = QStringLiteral("Malformed CSV data row %1.").arg(counters.rowsRead); return false; }
        auto requiredText = [&](int index) { return index < 0 || !fields.at(index).trimmed().isEmpty(); };
        if (!requiredText(name)) { error = QStringLiteral("Missing required name at row %1.").arg(counters.rowsRead); return false; }
        QString primary;
        if (entry.dataset == RebrickableDatasetId::Parts && partNum >= 0)
            primary = fields.at(partNum).trimmed();
        else if (entry.dataset == RebrickableDatasetId::Sets && setNum >= 0)
            primary = fields.at(setNum).trimmed();
        else if (entry.dataset == RebrickableDatasetId::Minifigs && figNum >= 0)
            primary = fields.at(figNum).trimmed();
        else if (entry.dataset == RebrickableDatasetId::Elements && elementId >= 0)
            primary = fields.at(elementId).trimmed();
        if (!primary.isEmpty() && primaryIds.contains(primary.toCaseFolded())) { error = QStringLiteral("Duplicate identity %1 at row %2.").arg(primary).arg(counters.rowsRead); return false; }
        if (!primary.isEmpty()) primaryIds.insert(primary.toCaseFolded());
        if (entry.dataset == RebrickableDatasetId::Parts) {
            bool categoryOk = false; const QString categoryId = fields.at(category).trimmed(); categoryId.toInt(&categoryOk);
            if (primary.isEmpty() || !categoryOk || !categoryIds.contains(categoryId)) { ++counters.unresolved; error = QStringLiteral("Part %1 references unresolved Part Category %2 at row %3.").arg(primary, categoryId).arg(counters.rowsRead); return false; }
        } else if (entry.dataset == RebrickableDatasetId::PartRelationships) {
            const QString r = fields.at(rel).trimmed().toUpper(); const QString c = fields.at(child).trimmed(); const QString p = fields.at(parent).trimmed();
            static const QSet<QString> supportedTypes = {
                QStringLiteral("A"), QStringLiteral("M"), QStringLiteral("P"),
                QStringLiteral("T"), QStringLiteral("B"), QStringLiteral("R")};
            if (!supportedTypes.contains(r) || c.isEmpty() || p.isEmpty()) {
                error = QStringLiteral("Invalid Part Relationship at row %1.")
                            .arg(counters.rowsRead);
                return false;
            }
            const auto childIt = partIdsByNumber.constFind(c);
            const auto parentIt = partIdsByNumber.constFind(p);
            if (parentIt == partIdsByNumber.constEnd()) {
                ++counters.unresolved;
                error = QStringLiteral(
                    "Part Relationship has an unresolved parent Part %1 at row %2.")
                            .arg(p)
                            .arg(counters.rowsRead);
                return false;
            }
            if (childIt == partIdsByNumber.constEnd()) {
                ++counters.unresolved;
                error = QStringLiteral(
                    "Part Relationship has an unresolved child Part %1 at row %2.")
                            .arg(c)
                            .arg(counters.rowsRead);
                return false;
            }
            if (childIt.value() == parentIt.value())
                ++counters.selfReferencesIgnored;
        } else if (entry.dataset == RebrickableDatasetId::Sets) {
            bool y = false, t = false, n = false; const int yv = fields.at(year).trimmed().toInt(&y); const int tv = fields.at(theme).trimmed().toInt(&t); const int nv = fields.at(count).trimmed().toInt(&n);
            if (primary.isEmpty() || !requiredText(name) || !y || !t || !n || yv < 0 || tv < 0 || nv < 0) { error = QStringLiteral("Invalid Set data at row %1.").arg(counters.rowsRead); return false; }
        }
    }
    if (counters.rowsRead == 0) { error = QStringLiteral("The CSV contains no data rows."); return false; }
    return true;
}

RebrickableImportPlanEntry* findEntry(RebrickableImportPlan& plan, RebrickableDatasetId id)
{ for (auto& entry : plan.entries) if (entry.dataset == id) return &entry; return nullptr; }
}

RebrickableImportPlan RebrickableGlobalImportService::run(
    RebrickableImportPlan plan, QSqlDatabase& database,
    const RebrickableImportCancellation& cancellation, const Progress& progress) const
{
    QVector<RebrickableDatasetId> order;
    for (const auto& descriptor : RebrickableDatasetRegistry::datasets()) if (descriptor.importerImplemented) order.append(descriptor.id);
    qInfo() << "Global Rebrickable import started. Implemented datasets:" << order.size();
    const int datasetTotal = static_cast<int>(order.size());
    RebrickableInventoryCompositionImporter compositionImporter;
    for (int index = 0; index < datasetTotal; ++index) {
        auto* entry = findEntry(plan, order.at(index)); if (!entry) continue;
        if (cancellation.isCancellationRequested()) { if (entry->status == RebrickableImportStatus::Ready) RebrickableImportPlanController::failDataset(plan, entry->dataset, QStringLiteral("Import cancelled."), true); continue; }
        if (entry->status != RebrickableImportStatus::Ready) continue;
        if (!RebrickableImportPlanController::beginDataset(plan, entry->dataset)) continue;
        QElapsedTimer timer; timer.start(); RebrickableImportCounters counters; QString error;
        if (progress) progress({entry->dataset, index + 1, datasetTotal, 0, -1, QStringLiteral("Validating"), entry->displayName, 0, counters});
        if (!validateSnapshot(*entry, database, cancellation, counters, error, progress, index + 1, datasetTotal)) {
            RebrickableImportPlanController::failDataset(plan, entry->dataset, error, cancellation.isCancellationRequested());
            qWarning() << "Global Rebrickable dataset validation failed:" << entry->displayName << error; continue;
        }
        if (progress) progress({entry->dataset, index + 1, datasetTotal, counters.rowsRead, counters.rowsRead, QStringLiteral("Importing"), entry->displayName, timer.elapsed(), counters});
        bool success = false; bool noChanges = false; QString message; RebrickableImportCounters imported;
        const RebrickableRowProgress rowProgress = [&](qint64 row) {
            if (progress) progress({entry->dataset, index + 1, datasetTotal, row,
                                    counters.rowsRead, QStringLiteral("Importing"),
                                    entry->displayName, timer.elapsed(), imported});
        };
        switch (entry->dataset) {
        case RebrickableDatasetId::Themes: { auto r = RebrickableThemeCatalogImporter().importFile(entry->sourcePath, database, &cancellation, true, QStringLiteral("Rebrickable themes.csv"), rowProgress); success=r.success; message=r.message; imported={r.rowsRead,r.inserted,r.updated,r.unchanged,0,r.deactivated,0,r.reactivated}; noChanges=r.inserted+r.updated+r.reactivated+r.deactivated==0; break; }
        case RebrickableDatasetId::Colors:
        case RebrickableDatasetId::PartCategories: {
            QTemporaryDir temporary; QString path; const auto* d=RebrickableDatasetRegistry::descriptor(entry->dataset);
            if (!RebrickableCsvInputResolver::resolve(entry->sourcePath,d->csvFileName,temporary,path,message)) break;
            RebrickableReferenceImporter::ImportResult r; RebrickableReferenceImporter importer(database);
            success = entry->dataset == RebrickableDatasetId::Colors
                ? importer.importColors(path, r, true, &cancellation, rowProgress)
                : importer.importPartCategories(path, r, true, &cancellation, rowProgress);
            imported.rowsRead=r.recordsProcessed; imported.skipped=r.recordsFailed;
            noChanges = false; // The legacy reference importer does not classify upserts reliably.
            if (!success) message=QStringLiteral("Reference-data import failed; see the application log."); break; }
        case RebrickableDatasetId::Parts: { auto r=RebrickablePartCatalogImporter().importFile(entry->sourcePath,database,&cancellation,rowProgress); success=r.success;message=r.message;imported={r.rowsRead,r.inserted,r.updated,r.unchanged,r.skipped,0,0,0};noChanges=r.inserted+r.updated==0;break; }
        case RebrickableDatasetId::PartRelationships: { auto r=RebrickablePartRelationshipImporter().importFile(entry->sourcePath,database,&cancellation,rowProgress,true);success=r.success;message=r.message;imported={r.rowsRead,r.inserted,r.updated,r.unchanged,r.skippedInvalid,r.deactivated,r.skippedMissingParent+r.skippedMissingChild,0,r.selfReferencesIgnored};noChanges=r.inserted+r.updated+r.deactivated==0;break; }
        case RebrickableDatasetId::Sets: { auto r=RebrickableSetCatalogImporter().importFile(entry->sourcePath,database,&cancellation,rowProgress);success=r.success;message=r.message;imported={r.rowsRead,r.inserted,r.updated,r.unchanged,r.skipped,0,0,0};noChanges=r.inserted+r.updated==0;break; }
        case RebrickableDatasetId::Minifigs: { auto r=RebrickableMinifigCatalogImporter().importFile(entry->sourcePath,database,&cancellation,rowProgress);success=r.success;message=r.message;imported={r.rowsRead,r.inserted,r.updated,r.unchanged,0,r.deactivated,0,0};noChanges=r.inserted+r.updated+r.deactivated==0;break; }
        case RebrickableDatasetId::Elements: { auto r=RebrickableElementImporter().importFile(entry->sourcePath,database,&cancellation,rowProgress);success=r.success;message=r.message;imported.rowsRead=r.rowsRead;imported.inserted=r.inserted;imported.updated=r.updated;imported.unchanged=r.unchanged;imported.reactivated=r.reactivated;imported.deactivated=r.deactivated;noChanges=r.inserted+r.updated+r.reactivated+r.deactivated==0;break; }
        case RebrickableDatasetId::Inventories: { auto r=compositionImporter.importInventories(entry->sourcePath,database,&cancellation,rowProgress);success=r.success;message=r.message;imported.rowsRead=r.rowsRead;imported.inserted=r.inserted;imported.updated=r.updated;imported.unchanged=r.unchanged;imported.deactivated=r.deactivated;imported.setInventoryRows=r.setInventoryRows;imported.recognizedMinifigInventories=r.recognizedMinifigInventories;noChanges=r.inserted+r.updated+r.deactivated==0;break; }
        case RebrickableDatasetId::InventoryParts: { auto r=compositionImporter.importParts(entry->sourcePath,database,&cancellation,rowProgress);success=r.success;message=r.message;imported.rowsRead=r.rowsRead;imported.inserted=r.inserted;imported.updated=r.updated;imported.unchanged=r.unchanged;imported.replaced=r.replaced;imported.setInventoryRows=r.setInventoryRows;imported.minifigPartRows=r.minifigPartRows;noChanges=r.inserted+r.updated+r.replaced==0;break; }
        case RebrickableDatasetId::InventoryMinifigs: { auto r=compositionImporter.importMinifigs(entry->sourcePath,database,&cancellation,rowProgress);success=r.success;message=r.message;imported.rowsRead=r.rowsRead;imported.inserted=r.inserted;imported.updated=r.updated;imported.unchanged=r.unchanged;imported.replaced=r.replaced;imported.setInventoryRows=r.setInventoryRows;noChanges=r.inserted+r.updated+r.replaced==0;break; }
        case RebrickableDatasetId::InventorySets: { auto r=compositionImporter.importSets(entry->sourcePath,database,&cancellation,rowProgress);success=r.success;message=r.message;imported.rowsRead=r.rowsRead;imported.inserted=r.inserted;imported.updated=r.updated;imported.unchanged=r.unchanged;imported.replaced=r.replaced;imported.setInventoryRows=r.setInventoryRows;noChanges=r.inserted+r.updated+r.replaced==0;break; }
        default: break;
        }
        if (!success) RebrickableImportPlanController::failDataset(plan, entry->dataset, message, cancellation.isCancellationRequested());
        else RebrickableImportPlanController::completeDataset(plan, entry->dataset, imported, timer.elapsed(), noChanges);
        qInfo() << "Global Rebrickable dataset finished:" << entry->displayName << rebrickableImportStatusText(entry->status) << "elapsed ms:" << timer.elapsed();
    }
    const auto completed = [&](RebrickableDatasetId id) {
        const auto* entry = findEntry(plan, id);
        return entry && (entry->status == RebrickableImportStatus::Imported
                         || entry->status == RebrickableImportStatus::NoChanges);
    };
    if (completed(RebrickableDatasetId::Inventories)
        && completed(RebrickableDatasetId::InventoryParts)
        && completed(RebrickableDatasetId::InventoryMinifigs)
        && completed(RebrickableDatasetId::InventorySets)) {
        auto preferred = compositionImporter.selectPreferredRevisions(database);
        auto* entry = findEntry(plan, RebrickableDatasetId::InventorySets);
        if (!preferred.success) {
            RebrickableImportPlanController::failDataset(
                plan, RebrickableDatasetId::InventorySets, preferred.message, false);
        } else if (entry && preferred.preferredChanged) {
            entry->counters.preferredChanged = preferred.preferredChanged;
            entry->message.chop(1);
            entry->message += QStringLiteral(", %1 preferred revision flag state changes.")
                                  .arg(preferred.preferredChanged);
        }
        if (preferred.success && completed(RebrickableDatasetId::Themes)
            && completed(RebrickableDatasetId::Sets)
            && completed(RebrickableDatasetId::Minifigs)) {
            const auto themes=RebrickableMinifigThemeDerivationService().rebuild(database);
            if (!themes.success) {
                RebrickableImportPlanController::failDataset(
                    plan,RebrickableDatasetId::InventorySets,themes.message,false);
            } else if (entry) {
                entry->message.chop(1);
                entry->message+=QStringLiteral(", %1 Minifig Theme associations derived.")
                                    .arg(themes.associations);
            }
        }
    }
    qInfo() << "Global Rebrickable import finished."; return plan;
}
