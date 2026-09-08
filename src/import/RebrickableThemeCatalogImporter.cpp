#include "RebrickableThemeCatalogImporter.h"

#include "RebrickableCsvInputResolver.h"
#include "global/RebrickableImportCancellation.h"

#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVariant>

namespace {
const QString Provider = QStringLiteral("Rebrickable");
struct Row { QString id; QString name; QString parentId; };
struct Existing { int catalogId = 0; int identityId = 0; QString name; int parentId = 0; bool active = false; };

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
}

RebrickableThemeCatalogImporter::Result RebrickableThemeCatalogImporter::importFile(
    const QString& fileName, QSqlDatabase& database,
    const RebrickableImportCancellation* cancellation, bool manageTransaction,
    const QString& source, const RebrickableRowProgress& progress)
{
    Result result; QTemporaryDir temporaryDirectory; QString csvPath;
    if (!RebrickableCsvInputResolver::resolve(fileName, QStringLiteral("themes.csv"),
                                              temporaryDirectory, csvPath, result.message))
        return result;
    QFile file(csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { result.message = QStringLiteral("Unable to open themes.csv."); return result; }
    QTextStream stream(&file); if (stream.atEnd()) { result.message = QStringLiteral("themes.csv is empty."); return result; }
    QString headerLine = stream.readLine(); if (!headerLine.isEmpty() && headerLine.front() == QChar(0xfeff)) headerLine.remove(0, 1);
    bool ok = false; const QStringList header = parseCsv(headerLine, ok);
    if (!ok || header != QStringList{QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("parent_id")}) {
        result.message = QStringLiteral("The selected file is not a supported Rebrickable themes.csv file."); return result;
    }
    QHash<QString, Row> rows;
    while (!stream.atEnd()) {
        const QString line = stream.readLine(); if (line.trimmed().isEmpty()) continue; ++result.rowsRead;
        if ((result.rowsRead & 255) == 0 && cancellation && cancellation->isCancellationRequested()) { result.message = QStringLiteral("Theme import cancelled."); return result; }
        if ((result.rowsRead & 255) == 0 && progress) progress(result.rowsRead);
        const QStringList fields = parseCsv(line, ok);
        Row row;
        if (ok && fields.size() == 3) row = {fields.at(0).trimmed(), fields.at(1).trimmed(), fields.at(2).trimmed()};
        if (!ok || fields.size() != 3 || row.id.isEmpty() || row.name.isEmpty() || rows.contains(row.id)) {
            result.message = QStringLiteral("Invalid or duplicate Theme at data row %1.").arg(result.rowsRead); return result;
        }
        rows.insert(row.id, row);
    }
    if (rows.isEmpty()) { result.message = QStringLiteral("themes.csv contains no catalog rows."); return result; }
    for (const Row& row : rows) if (!row.parentId.isEmpty() && !rows.contains(row.parentId)) {
        result.message = QStringLiteral("Theme %1 refers to missing parent %2.").arg(row.id, row.parentId); return result;
    }
    for (const Row& row : rows) {
        QSet<QString> ancestors; QString current = row.id;
        while (!current.isEmpty()) { if (ancestors.contains(current)) { result.message = QStringLiteral("themes.csv contains a parent cycle."); return result; } ancestors.insert(current); current = rows.value(current).parentId; }
    }
    file.close();
    QHash<QString, Existing> existing; QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT tc.id,tei.id,tei.external_id,tc.name,COALESCE(tc.parent_theme_catalog_id,0),tei.is_active FROM theme_external_identifier tei JOIN theme_catalog tc ON tc.id=tei.theme_catalog_id WHERE tei.provider=:provider"));
    query.bindValue(QStringLiteral(":provider"), Provider);
    if (!query.exec()) { result.message = query.lastError().text(); return result; }
    while (query.next()) existing.insert(query.value(2).toString(), {query.value(0).toInt(), query.value(1).toInt(), query.value(3).toString(), query.value(4).toInt(), query.value(5).toBool()});
    if (manageTransaction && !database.transaction()) { result.message = QStringLiteral("Unable to begin Theme import transaction: %1").arg(database.lastError().text()); return result; }
    auto fail = [&](const QString& message) { if (manageTransaction) database.rollback(); result.message = message; return result; };
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QHash<QString, int> ids; QSet<QString> changed; QSet<QString> reactivatedIds; QSqlQuery insertCatalog(database), insertIdentity(database), updateCatalog(database), updateIdentity(database);
    if (!insertCatalog.prepare(QStringLiteral("INSERT INTO theme_catalog(name,parent_theme_catalog_id,is_active,created_utc,modified_utc) VALUES(:name,NULL,1,:created,:modified)"))
        || !insertIdentity.prepare(QStringLiteral("INSERT INTO theme_external_identifier(theme_catalog_id,provider,external_id,source,is_active,created_utc,modified_utc) VALUES(:catalog,:provider,:external,:source,1,:created,:modified)"))
        || !updateCatalog.prepare(QStringLiteral("UPDATE theme_catalog SET name=:name,is_active=1,modified_utc=:modified WHERE id=:id"))
        || !updateIdentity.prepare(QStringLiteral("UPDATE theme_external_identifier SET source=:source,is_active=1,modified_utc=:modified WHERE id=:id")))
        return fail(QStringLiteral("Unable to prepare Theme import queries."));
    for (const Row& row : rows) {
        if (cancellation && cancellation->isCancellationRequested()) return fail(QStringLiteral("Theme import cancelled."));
        if (existing.contains(row.id)) {
            const Existing item = existing.value(row.id); ids.insert(row.id, item.catalogId);
            updateCatalog.bindValue(":name", row.name); updateCatalog.bindValue(":modified", now); updateCatalog.bindValue(":id", item.catalogId);
            if (!updateCatalog.exec()) return fail(updateCatalog.lastError().text());
            if (item.name != row.name) changed.insert(row.id);
            if (!item.active) { updateIdentity.bindValue(":source", source); updateIdentity.bindValue(":modified", now); updateIdentity.bindValue(":id", item.identityId); if (!updateIdentity.exec()) return fail(updateIdentity.lastError().text()); reactivatedIds.insert(row.id); ++result.reactivated; }
        } else {
            insertCatalog.bindValue(":name", row.name); insertCatalog.bindValue(":created", now); insertCatalog.bindValue(":modified", now); if (!insertCatalog.exec()) return fail(insertCatalog.lastError().text());
            const int id = insertCatalog.lastInsertId().toInt(); ids.insert(row.id, id);
            insertIdentity.bindValue(":catalog", id); insertIdentity.bindValue(":provider", Provider); insertIdentity.bindValue(":external", row.id); insertIdentity.bindValue(":source", source); insertIdentity.bindValue(":created", now); insertIdentity.bindValue(":modified", now); if (!insertIdentity.exec()) return fail(insertIdentity.lastError().text()); ++result.inserted;
        }
    }
    QSqlQuery parent(database); parent.prepare(QStringLiteral("UPDATE theme_catalog SET parent_theme_catalog_id=:parent,modified_utc=:modified WHERE id=:id AND COALESCE(parent_theme_catalog_id,0)<>:compare"));
    for (const Row& row : rows) { const int parentId = row.parentId.isEmpty() ? 0 : ids.value(row.parentId); parent.bindValue(":parent", parentId ? QVariant(parentId) : QVariant()); parent.bindValue(":compare", parentId); parent.bindValue(":modified", now); parent.bindValue(":id", ids.value(row.id)); if (!parent.exec()) return fail(parent.lastError().text()); if (parent.numRowsAffected() > 0 && existing.contains(row.id)) changed.insert(row.id); }
    result.updated = changed.size();
    QSet<QString> changedOrReactivated = changed;
    changedOrReactivated.unite(reactivatedIds);
    result.unchanged = result.rowsRead - result.inserted - changedOrReactivated.size();
    QSqlQuery deactivate(database); deactivate.prepare(QStringLiteral("UPDATE theme_external_identifier SET is_active=0,modified_utc=:modified WHERE id=:id AND is_active=1"));
    for (auto it = existing.cbegin(); it != existing.cend(); ++it) if (!rows.contains(it.key()) && it.value().active) { deactivate.bindValue(":modified", now); deactivate.bindValue(":id", it.value().identityId); if (!deactivate.exec()) return fail(deactivate.lastError().text()); ++result.deactivated; }
    QSqlQuery sync(database); sync.prepare(QStringLiteral("UPDATE theme_catalog SET is_active=CASE WHEN EXISTS(SELECT 1 FROM theme_external_identifier tei WHERE tei.theme_catalog_id=theme_catalog.id AND tei.is_active=1) THEN 1 ELSE 0 END,modified_utc=:modified WHERE is_active<>CASE WHEN EXISTS(SELECT 1 FROM theme_external_identifier tei WHERE tei.theme_catalog_id=theme_catalog.id AND tei.is_active=1) THEN 1 ELSE 0 END")); sync.bindValue(":modified", now); if (!sync.exec()) return fail(sync.lastError().text());
    if (manageTransaction && !database.commit()) return fail(database.lastError().text());
    result.success = true; result.message = QStringLiteral("Theme Catalog import completed successfully."); return result;
}
