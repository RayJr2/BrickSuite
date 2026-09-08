#include "RebrickableMinifigThemeImporter.h"
#include "RebrickableThemeCatalogImporter.h"

#include "RebrickableCsvInputResolver.h"
#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVariant>
#include <memory>

namespace {
const QString Provider = QStringLiteral("Rebrickable");
const QString Source = QStringLiteral("Rebrickable Minifig Theme relationship files");
struct ThemeRow { QString id; QString name; QString parentId; };
struct ExistingTheme { int catalogId = 0; int identityId = 0; QString name; int parentId = 0; bool active = false; };

QStringList parseCsv(const QString& line, bool& ok)
{
    QStringList fields; QString field; bool quoted = false; ok = true;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == '"') {
            if (quoted && i + 1 < line.size() && line.at(i + 1) == '"') { field += '"'; ++i; }
            else quoted = !quoted;
        } else if (ch == ',' && !quoted) { fields.append(field); field.clear(); }
        else field += ch;
    }
    if (quoted) ok = false;
    fields.append(field); return fields;
}

bool resolveDataset(const QDir& directory, const QString& csvName,
                    QTemporaryDir& temporaryDirectory, QString& path, QString& error)
{
    QStringList matches;
    const QString csvLower = csvName.toLower();
    const QString zipLower = csvLower + ".zip";
    for (const QString& entry : directory.entryList(QDir::Files)) {
        const QString lower = entry.toLower();
        if (lower == csvLower || lower == zipLower) matches.append(directory.filePath(entry));
    }
    if (matches.isEmpty()) { error = QString("Required dataset %1 or %1.zip was not found.").arg(csvName); return false; }
    if (matches.size() != 1) { error = QString("Multiple sources were found for %1; keep exactly one CSV or ZIP.").arg(csvName); return false; }
    return RebrickableCsvInputResolver::resolve(matches.constFirst(), csvName,
                                                 temporaryDirectory, path, error);
}

bool openCsv(const QString& path, const QStringList& expected, QFile& file,
             QTextStream*& stream, QString& error)
{
    file.setFileName(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { error = "Unable to open " + QFileInfo(path).fileName(); return false; }
    stream = new QTextStream(&file);
    if (stream->atEnd()) { error = QFileInfo(path).fileName() + " is empty."; return false; }
    QString header = stream->readLine(); if (!header.isEmpty() && header.front() == QChar(0xfeff)) header.remove(0,1);
    bool ok = false;
    if (parseCsv(header, ok) != expected || !ok) { error = "Unsupported header in " + QFileInfo(path).fileName(); return false; }
    return true;
}
}

RebrickableMinifigThemeImporter::Result
RebrickableMinifigThemeImporter::importDirectory(const QString& directoryPath)
{
    Result result; QDir directory(directoryPath);
    if (!directory.exists()) { result.message = "The selected directory does not exist."; return result; }
    QTemporaryDir themeTemp, setTemp, inventoryTemp, relationTemp;
    QString themePath, setPath, inventoryPath, relationPath;
    if (!resolveDataset(directory,"themes.csv",themeTemp,themePath,result.message)
        || !resolveDataset(directory,"sets.csv",setTemp,setPath,result.message)
        || !resolveDataset(directory,"inventories.csv",inventoryTemp,inventoryPath,result.message)
        || !resolveDataset(directory,"inventory_minifigs.csv",relationTemp,relationPath,result.message)) return result;

    QHash<QString, ThemeRow> themes; QHash<QString, QString> sets, inventories;
    QList<QPair<QString,QString>> relationships; QSet<QString> sourcePairs;
    auto read = [&](const QString& path, const QStringList& headers, auto consume) {
        QFile file; QTextStream* raw = nullptr;
        if (!openCsv(path,headers,file,raw,result.message)) { delete raw; return false; }
        std::unique_ptr<QTextStream> stream(raw); int row = 0;
        while (!stream->atEnd()) { QString line=stream->readLine(); if(line.trimmed().isEmpty()) continue; ++row;
            bool ok=false; QStringList f=parseCsv(line,ok);
            if(!ok || f.size()!=headers.size() || !consume(f,row)) { if(result.message.isEmpty()) result.message=QString("Invalid row %1 in %2.").arg(row).arg(QFileInfo(path).fileName()); return false; }
        }
        return true;
    };
    if (!read(themePath,{"id","name","parent_id"},[&](const QStringList& f,int){
        ThemeRow t{f[0].trimmed(),f[1].trimmed(),f[2].trimmed()};
        if(t.id.isEmpty()||t.name.isEmpty()||themes.contains(t.id)) return false; themes.insert(t.id,t); ++result.themesRead; return true; })) return result;
    for (const ThemeRow& t : themes) if(!t.parentId.isEmpty()&&!themes.contains(t.parentId)){result.message="themes.csv contains a missing parent Theme.";return result;}
    for (const ThemeRow& theme : themes) {
        QSet<QString> ancestors; QString current = theme.id;
        while (!current.isEmpty()) {
            if (ancestors.contains(current)) { result.message = "themes.csv contains a parent cycle."; return result; }
            ancestors.insert(current); current = themes.value(current).parentId;
        }
    }
    if (!read(setPath,{"set_num","name","year","theme_id","num_parts","img_url"},[&](const QStringList& f,int){
        QString id=f[0].trimmed(), theme=f[3].trimmed(); if(id.isEmpty()||theme.isEmpty()||sets.contains(id)||!themes.contains(theme)) return false; sets.insert(id,theme); return true; })) return result;
    if (!read(inventoryPath,{"id","version","set_num"},[&](const QStringList& f,int){
        QString id=f[0].trimmed(); if(id.isEmpty()||inventories.contains(id)) return false; inventories.insert(id,f[2].trimmed()); return true; })) return result;
    if (!read(relationPath,{"inventory_id","fig_num","quantity"},[&](const QStringList& f,int){
        QString inventory=f[0].trimmed(), fig=f[1].trimmed(), pair=inventory+QChar(0x1f)+fig; bool ok=false; int q=f[2].trimmed().toInt(&ok);
        if(inventory.isEmpty()||fig.isEmpty()||!ok||q<=0||sourcePairs.contains(pair)||!inventories.contains(inventory)) return false;
        if(!sets.contains(inventories.value(inventory))){result.message="An inventory_minifigs row refers to an inventory whose Set is missing.";return false;}
        sourcePairs.insert(pair); relationships.append({fig,sets.value(inventories.value(inventory))}); ++result.relationshipRowsRead; return true; })) return result;

    QSqlDatabase db=DatabaseManager::instance().database();
    QSqlQuery catalogCheck(db);
    catalogCheck.prepare("SELECT 1 FROM minifig_external_identifier mei "
                         "JOIN minifig_catalog mc ON mc.id=mei.minifig_catalog_id "
                         "WHERE mei.provider=:p AND mei.is_active=1 AND mc.is_active=1 LIMIT 1");
    catalogCheck.bindValue(":p", Provider);
    if (!catalogCheck.exec()) {
        result.message = QString("Unable to verify the Rebrickable Minifigs Catalog: %1")
                             .arg(catalogCheck.lastError().text());
        return result;
    }
    if (!catalogCheck.next()) {
        result.message = "Import the Rebrickable Minifigs Catalog before importing Minifig Themes.";
        return result;
    }
    catalogCheck.finish();
    if(!db.transaction()){result.message="Unable to begin Theme import transaction.";return result;}
    auto fail=[&](const QString& m){db.rollback();result.message=m;return result;};
    const auto themeResult = RebrickableThemeCatalogImporter().importFile(
        themePath, db, nullptr, false, Source);
    if (!themeResult.success) return fail(themeResult.message);
    result.themesInserted=themeResult.inserted; result.themesUpdated=themeResult.updated;
    result.themesReactivated=themeResult.reactivated; result.themesDeactivated=themeResult.deactivated;
    QHash<QString,int> internal; QSqlQuery q(db);
    q.prepare("SELECT tei.external_id,tei.theme_catalog_id FROM theme_external_identifier tei WHERE tei.provider=:p AND tei.is_active=1");q.bindValue(":p",Provider);
    if(!q.exec())return fail(q.lastError().text());while(q.next())internal.insert(q.value(0).toString(),q.value(1).toInt());
    QHash<QString,int> minifigs; q.prepare("SELECT mei.external_id,mei.minifig_catalog_id FROM minifig_external_identifier mei JOIN minifig_catalog mc ON mc.id=mei.minifig_catalog_id WHERE mei.provider=:p AND mei.is_active=1 AND mc.is_active=1");q.bindValue(":p",Provider);if(!q.exec())return fail(q.lastError().text());while(q.next())minifigs.insert(q.value(0).toString().toCaseFolded(),q.value(1).toInt());
    QSet<QString> unresolved, associations; int resolvableRows = 0; for(const auto&r:relationships){int mid=minifigs.value(r.first.toCaseFolded());if(!mid){unresolved.insert(r.first.toCaseFolded());continue;}++resolvableRows;associations.insert(QString::number(mid)+QChar(0x1f)+QString::number(internal.value(r.second)));}
    result.unresolvedMinifigs=unresolved.size();result.associations=associations.size();result.duplicateRelationshipsCollapsed=resolvableRows-result.associations;
    q.prepare("DELETE FROM minifig_theme WHERE provider=:p");q.bindValue(":p",Provider);if(!q.exec())return fail(q.lastError().text());
    QSqlQuery link(db);link.prepare("INSERT INTO minifig_theme(minifig_catalog_id,theme_catalog_id,provider) VALUES(:m,:t,:p)");
    for(const QString& key:associations){auto parts=key.split(QChar(0x1f));link.bindValue(":m",parts[0].toInt());link.bindValue(":t",parts[1].toInt());link.bindValue(":p",Provider);if(!link.exec())return fail(link.lastError().text());}
    if(!db.commit())return fail(db.lastError().text());result.success=true;result.message="Minifig Theme import completed successfully.";return result;
}
