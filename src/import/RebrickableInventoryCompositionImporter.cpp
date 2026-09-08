#include "RebrickableInventoryCompositionImporter.h"

#include "RebrickableCsvInputResolver.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>
#include <QDebug>

namespace {
constexpr auto Provider = "Rebrickable";

QStringList parseCsv(const QString& line, bool& ok)
{
    QStringList fields; QString field; bool quoted = false; ok = true;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == '"') {
            if (quoted && i + 1 < line.size() && line.at(i + 1) == '"') { field += ch; ++i; }
            else quoted = !quoted;
        } else if (ch == ',' && !quoted) { fields.append(field); field.clear(); }
        else field += ch;
    }
    ok = !quoted; fields.append(field); return fields;
}

bool parseBoolean(const QString& text, bool& value)
{
    const QString normalized = text.trimmed().toLower();
    if (normalized == "t" || normalized == "true" || normalized == "1" || normalized == "yes") { value = true; return true; }
    if (normalized == "f" || normalized == "false" || normalized == "0" || normalized == "no") { value = false; return true; }
    return false;
}

bool cancelled(qint64 row, const RebrickableImportCancellation* token,
               const RebrickableRowProgress& progress)
{
    if ((row & 1023) != 0) return false;
    if (progress) progress(row);
    return token && token->isCancellationRequested();
}

bool loadMap(QSqlDatabase& database, const QString& sql, QHash<QString, int>& map,
             QString& error)
{
    QSqlQuery query(database);
    if (!query.exec(sql)) { error = query.lastError().text(); return false; }
    while (query.next()) map.insert(query.value(0).toString(), query.value(1).toInt());
    return true;
}

struct Input {
    QTemporaryDir temporary;
    QFile file;
    QTextStream stream;
    QStringList headers;
    QString error;
    Input(const QString& source, const QString& expected)
    {
        QString path;
        if (!RebrickableCsvInputResolver::resolve(source, expected, temporary, path, error)) return;
        file.setFileName(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { error = file.errorString(); return; }
        stream.setDevice(&file);
        QString header = stream.readLine();
        if (!header.isEmpty() && header.front() == QChar(0xfeff)) header.remove(0, 1);
        bool ok = false; headers = parseCsv(header, ok);
        if (!ok) error = QStringLiteral("Malformed CSV header.");
    }
    bool valid() const { return error.isEmpty(); }
};

bool requiredHeaders(const Input& input, const QStringList& names, QString& error)
{
    for (const QString& name : names) if (!input.headers.contains(name)) {
        error = QStringLiteral("Missing required header: %1.").arg(name); return false;
    }
    return true;
}

bool begin(QSqlDatabase& database, RebrickableInventoryCompositionImporter::Result& result)
{
    if (database.transaction()) return true;
    result.message = QStringLiteral("Unable to begin import transaction: %1")
                         .arg(database.lastError().text());
    return false;
}

bool commit(QSqlDatabase& database, RebrickableInventoryCompositionImporter::Result& result)
{
    if (!database.commit()) {
        result.message = QStringLiteral("Unable to commit import transaction: %1")
                             .arg(database.lastError().text());
        database.rollback(); return false;
    }
    result.success = true; result.message = QStringLiteral("Import completed successfully.");
    return true;
}
}

RebrickableInventoryCompositionImporter::Result
RebrickableInventoryCompositionImporter::importInventories(
    const QString& fileName, QSqlDatabase& database,
    const RebrickableImportCancellation* token, const RebrickableRowProgress& progress)
{
    m_inventoryOwners.clear();
    m_minifigInventoryOwners.clear();
    Result result; QElapsedTimer timer; timer.start(); Input input(fileName, "inventories.csv");
    if (!input.valid()) { result.message = input.error; return result; }
    if (!requiredHeaders(input, {"id","version","set_num"}, result.message)) return result;
    if (token && token->isCancellationRequested()) {
        result.message = QStringLiteral("inventories.csv import cancelled.");
        return result;
    }
    QHash<QString,int> sets, minifigs;
    if (!loadMap(database, "SELECT set_number,id FROM set_catalog", sets, result.message)
        || !loadMap(database, "SELECT external_id,minifig_catalog_id FROM minifig_external_identifier WHERE provider='Rebrickable' AND is_active=1", minifigs, result.message)) return result;
    if (!begin(database, result)) return result;
    QSqlQuery existing(database);
    if (!existing.exec("SELECT external_inventory_id,id,set_catalog_id,version,is_active FROM set_inventory_revision WHERE provider='Rebrickable'")) { result.message=existing.lastError().text(); database.rollback(); return result; }
    struct Old { int id,setId,version; bool active; }; QHash<QString,Old> old;
    while (existing.next()) old.insert(existing.value(0).toString(), {existing.value(1).toInt(),existing.value(2).toInt(),existing.value(3).toInt(),existing.value(4).toBool()});
    QSqlQuery insert(database), update(database);
    insert.prepare("INSERT INTO set_inventory_revision(provider,external_inventory_id,set_catalog_id,version,is_active,is_preferred,created_utc,modified_utc) VALUES('Rebrickable',:external,:set_id,:version,1,0,:now,:now)");
    update.prepare("UPDATE set_inventory_revision SET set_catalog_id=:set_id,version=:version,is_active=1,modified_utc=:now WHERE id=:id");
    const int idCol=input.headers.indexOf("id"), versionCol=input.headers.indexOf("version"), setCol=input.headers.indexOf("set_num");
    const QString now=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QSet<QString> seen, seenSets, versions; QHash<QString,bool> owners;
    QHash<QString,int> minifigOwners;
    while (!input.stream.atEnd()) {
        const QString line=input.stream.readLine(); if (line.trimmed().isEmpty()) continue; ++result.rowsRead;
        if (cancelled(result.rowsRead,token,progress)) { result.message="Inventories import cancelled."; database.rollback(); return result; }
        bool ok=false; const auto f=parseCsv(line,ok); bool versionOk=false;
        const QString external=ok&&f.size()==input.headers.size()?f.at(idCol).trimmed():QString();
        const int version=ok&&f.size()==input.headers.size()?f.at(versionCol).trimmed().toInt(&versionOk):-1;
        const QString setNumber=ok&&f.size()==input.headers.size()?f.at(setCol).trimmed():QString();
        const auto setIt=sets.constFind(setNumber), minifigIt=minifigs.constFind(setNumber);
        const bool isSet=setIt!=sets.constEnd(), isMinifig=minifigIt!=minifigs.constEnd();
        const QString versionKey=setNumber+'\x1f'+QString::number(version);
        if (!ok || f.size()!=input.headers.size() || external.isEmpty() || !versionOk || version<0 || seen.contains(external) || isSet==isMinifig || (isSet && versions.contains(versionKey))) {
            result.message=QStringLiteral("Invalid, duplicate, ambiguous, or unresolved Inventory at row %1.").arg(result.rowsRead); database.rollback(); return result;
        }
        seen.insert(external); owners.insert(external,isSet);
        if (!isSet) { minifigOwners.insert(external,minifigIt.value()); ++result.recognizedMinifigInventories; continue; }
        seenSets.insert(external); versions.insert(versionKey); ++result.setInventoryRows;
        const auto oldIt=old.constFind(external);
        if (oldIt==old.constEnd()) {
            insert.bindValue(":external",external); insert.bindValue(":set_id",setIt.value()); insert.bindValue(":version",version); insert.bindValue(":now",now);
            if (!insert.exec()) { result.message=insert.lastError().text(); database.rollback(); return result; } ++result.inserted;
        } else if (oldIt->setId==setIt.value() && oldIt->version==version && oldIt->active) ++result.unchanged;
        else { update.bindValue(":set_id",setIt.value());update.bindValue(":version",version);update.bindValue(":now",now);update.bindValue(":id",oldIt->id);if(!update.exec()){result.message=update.lastError().text();database.rollback();return result;}++result.updated; }
    }
    if ((result.rowsRead & 1023) != 0 && progress) progress(result.rowsRead);
    if (token && token->isCancellationRequested()) {
        result.message = QStringLiteral("Inventories import cancelled.");
        database.rollback(); return result;
    }
    if (!result.rowsRead) { result.message="Inventories CSV contains no data rows."; database.rollback(); return result; }
    QSqlQuery deactivate(database); deactivate.prepare("UPDATE set_inventory_revision SET is_active=0,is_preferred=0,modified_utc=:now WHERE provider='Rebrickable' AND external_inventory_id=:external AND is_active=1");
    for(auto it=old.cbegin();it!=old.cend();++it) if(it->active&&!seenSets.contains(it.key())){deactivate.bindValue(":now",now);deactivate.bindValue(":external",it.key());if(!deactivate.exec()){result.message=deactivate.lastError().text();database.rollback();return result;}result.deactivated+=deactivate.numRowsAffected();}
    if(commit(database,result)) { m_inventoryOwners=owners; m_minifigInventoryOwners=minifigOwners; }
    qInfo()<<"Inventories import elapsed ms:" << timer.elapsed() << "rows:" << result.rowsRead
           << "Set inventories:" << result.setInventoryRows
           << "Minifig inventories recognized:" << result.recognizedMinifigInventories;
    return result;
}

namespace {
enum class ChildKind { Parts, Minifigs, Sets };
RebrickableInventoryCompositionImporter::Result importChildren(
    ChildKind kind, const QString& fileName, QSqlDatabase& database,
    const RebrickableImportCancellation* token, const RebrickableRowProgress& progress,
    const QHash<QString,bool>& owners, const QHash<QString,int>& minifigOwners)
{
    RebrickableInventoryCompositionImporter::Result result; QElapsedTimer timer;timer.start();
    const QString expected=kind==ChildKind::Parts?"inventory_parts.csv":kind==ChildKind::Minifigs?"inventory_minifigs.csv":"inventory_sets.csv";
    Input input(fileName,expected); if(!input.valid()){result.message=input.error;return result;}
    QStringList headers={"inventory_id","quantity"}; headers << (kind==ChildKind::Parts?QStringList{"part_num","color_id","is_spare","img_url"}:kind==ChildKind::Minifigs?QStringList{"fig_num"}:QStringList{"set_num"});
    if(!requiredHeaders(input,headers,result.message))return result;
    if(kind==ChildKind::Parts&&owners.isEmpty()){
        result.message="Inventories source is required to classify Set and Minifig inventory rows.";
        return result;
    }
    QHash<QString,int> revisions, identities, colors;
    if(!loadMap(database,"SELECT external_inventory_id,id FROM set_inventory_revision WHERE provider='Rebrickable' AND is_active=1",revisions,result.message))return result;
    if(kind==ChildKind::Parts){if(!loadMap(database,"SELECT part_number,id FROM part",identities,result.message)||!loadMap(database,"SELECT CAST(rebrickable_id AS TEXT),id FROM color WHERE rebrickable_id IS NOT NULL",colors,result.message))return result;}
    else if(kind==ChildKind::Minifigs){if(!loadMap(database,"SELECT external_id,minifig_catalog_id FROM minifig_external_identifier WHERE provider='Rebrickable' AND is_active=1",identities,result.message))return result;}
    else if(!loadMap(database,"SELECT set_number,id FROM set_catalog",identities,result.message))return result;
    if(token && token->isCancellationRequested()){result.message=expected+" import cancelled.";return result;}
    if(!begin(database,result))return result;
    const QString table=kind==ChildKind::Parts?"set_inventory_part":kind==ChildKind::Minifigs?"set_inventory_minifig":"set_inventory_contained_set";
    QSqlQuery temporary(database);
    if(!temporary.exec("CREATE TEMP TABLE IF NOT EXISTS current_inventory_child(owner_kind INTEGER,revision_id INTEGER,identity_id INTEGER,color_id INTEGER,spare INTEGER,PRIMARY KEY(owner_kind,revision_id,identity_id,color_id,spare))")
        ||!temporary.exec("DELETE FROM current_inventory_child")
        ||!temporary.exec("CREATE TEMP TABLE IF NOT EXISTS current_minifig_owner(minifig_catalog_id INTEGER PRIMARY KEY)")
        ||!temporary.exec("DELETE FROM current_minifig_owner")){result.message=temporary.lastError().text();database.rollback();return result;}
    QSqlQuery seen(database);seen.prepare("INSERT INTO current_inventory_child(owner_kind,revision_id,identity_id,color_id,spare) VALUES(:kind,:revision,:identity,:color,:spare)");
    QSqlQuery insert(database),update(database),minifigInsert(database),minifigUpdate(database);
    if(kind==ChildKind::Parts){
        insert.prepare("INSERT OR IGNORE INTO set_inventory_part(set_inventory_revision_id,part_id,color_id,quantity,is_spare,image_url,created_utc,modified_utc) VALUES(:revision,:identity,:color,:quantity,:spare,:image,:now,:now)");
        update.prepare("UPDATE set_inventory_part SET quantity=:quantity,image_url=:image,modified_utc=:now WHERE set_inventory_revision_id=:revision AND part_id=:identity AND color_id=:color AND is_spare=:spare AND (quantity<>:quantity OR COALESCE(image_url,'')<>:image)");
        minifigInsert.prepare("INSERT OR IGNORE INTO minifig_catalog_part(minifig_catalog_id,part_id,color_id,quantity_required,is_spare,provider,source,created_utc,modified_utc) VALUES(:minifig,:identity,:color,:quantity,:spare,'Rebrickable','Global inventory_parts.csv',:now,:now)");
        minifigUpdate.prepare("UPDATE minifig_catalog_part SET quantity_required=:quantity,provider='Rebrickable',source='Global inventory_parts.csv',modified_utc=:now WHERE minifig_catalog_id=:minifig AND part_id=:identity AND color_id=:color AND is_spare=:spare AND (quantity_required<>:quantity OR provider<>'Rebrickable' OR source<>'Global inventory_parts.csv')");
    } else if(kind==ChildKind::Minifigs){
        insert.prepare("INSERT OR IGNORE INTO set_inventory_minifig(set_inventory_revision_id,minifig_catalog_id,quantity,created_utc,modified_utc) VALUES(:revision,:identity,:quantity,:now,:now)");
        update.prepare("UPDATE set_inventory_minifig SET quantity=:quantity,modified_utc=:now WHERE set_inventory_revision_id=:revision AND minifig_catalog_id=:identity AND quantity<>:quantity");
    } else {
        insert.prepare("INSERT OR IGNORE INTO set_inventory_contained_set(set_inventory_revision_id,contained_set_catalog_id,quantity,created_utc,modified_utc) VALUES(:revision,:identity,:quantity,:now,:now)");
        update.prepare("UPDATE set_inventory_contained_set SET quantity=:quantity,modified_utc=:now WHERE set_inventory_revision_id=:revision AND contained_set_catalog_id=:identity AND quantity<>:quantity");
    }
    const int inventoryCol=input.headers.indexOf("inventory_id"),quantityCol=input.headers.indexOf("quantity");
    const int identityCol=input.headers.indexOf(kind==ChildKind::Parts?"part_num":kind==ChildKind::Minifigs?"fig_num":"set_num");
    const int colorCol=input.headers.indexOf("color_id"),spareCol=input.headers.indexOf("is_spare"),imageCol=input.headers.indexOf("img_url");
    const QString now=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); QSet<int> affected;
    if(kind==ChildKind::Parts){
        QSqlQuery ownerInsert(database);ownerInsert.prepare("INSERT OR IGNORE INTO current_minifig_owner(minifig_catalog_id) VALUES(:id)");
        for(int minifigId:minifigOwners){ownerInsert.bindValue(":id",minifigId);if(!ownerInsert.exec()){result.message=ownerInsert.lastError().text();database.rollback();return result;}}
    }
    while(!input.stream.atEnd()){
        const QString line=input.stream.readLine();if(line.trimmed().isEmpty())continue;++result.rowsRead;
        if(cancelled(result.rowsRead,token,progress)){result.message=expected+" import cancelled.";database.rollback();return result;}
        bool ok=false;const auto f=parseCsv(line,ok);bool quantityOk=false; qint64 quantity=ok&&f.size()==input.headers.size()?f.at(quantityCol).trimmed().toLongLong(&quantityOk):-1;
        const QString inventory=ok&&f.size()==input.headers.size()?f.at(inventoryCol).trimmed():QString();const QString identity=ok&&f.size()==input.headers.size()?f.at(identityCol).trimmed():QString();
        const auto revisionIt=revisions.constFind(inventory),identityIt=identities.constFind(identity); bool spare=false,spareOk=true; auto colorIt=colors.cend();
        if(kind==ChildKind::Parts)spareOk=parseBoolean(f.size()==input.headers.size()?f.at(spareCol):QString(),spare);
        if(!ok||f.size()!=input.headers.size()||!quantityOk||quantity<=0||!spareOk){result.message=QStringLiteral("Invalid %1 row %2.").arg(expected).arg(result.rowsRead);database.rollback();return result;}
        const auto ownerIt=owners.constFind(inventory);
        if(ownerIt!=owners.constEnd()&&!ownerIt.value()){
            if(kind!=ChildKind::Parts){result.message=QStringLiteral("Unexpected Minifig-owned inventory ID in %1 row %2.").arg(expected).arg(result.rowsRead);database.rollback();return result;}
            const auto minifigIt=minifigOwners.constFind(inventory);
            if(minifigIt==minifigOwners.constEnd()){result.message=QStringLiteral("Unresolved Minifig inventory owner at row %1.").arg(result.rowsRead);database.rollback();return result;}
            colorIt=colors.constFind(f.at(colorCol).trimmed());
            if(identityIt==identities.constEnd()||colorIt==colors.constEnd()){result.message=QStringLiteral("Invalid or unresolved %1 row %2.").arg(expected).arg(result.rowsRead);database.rollback();return result;}
            seen.bindValue(":kind",1);seen.bindValue(":revision",minifigIt.value());seen.bindValue(":identity",identityIt.value());seen.bindValue(":color",colorIt.value());seen.bindValue(":spare",spare);
            if(!seen.exec()){result.message=QStringLiteral("Duplicate logical %1 row at row %2.").arg(expected).arg(result.rowsRead);database.rollback();return result;}
            minifigInsert.bindValue(":minifig",minifigIt.value());minifigInsert.bindValue(":identity",identityIt.value());minifigInsert.bindValue(":color",colorIt.value());minifigInsert.bindValue(":quantity",quantity);minifigInsert.bindValue(":spare",spare);minifigInsert.bindValue(":now",now);
            if(!minifigInsert.exec()){result.message=minifigInsert.lastError().text();database.rollback();return result;}
            if(minifigInsert.numRowsAffected()>0)++result.inserted;
            else {minifigUpdate.bindValue(":minifig",minifigIt.value());minifigUpdate.bindValue(":identity",identityIt.value());minifigUpdate.bindValue(":color",colorIt.value());minifigUpdate.bindValue(":quantity",quantity);minifigUpdate.bindValue(":spare",spare);minifigUpdate.bindValue(":now",now);if(!minifigUpdate.exec()){result.message=minifigUpdate.lastError().text();database.rollback();return result;}if(minifigUpdate.numRowsAffected()>0)++result.updated;else ++result.unchanged;}
            ++result.minifigPartRows;continue;
        }
        if((!owners.isEmpty()&&ownerIt==owners.constEnd())||revisionIt==revisions.constEnd()){
            result.message=QStringLiteral("Unknown inventory ID in %1 row %2.").arg(expected).arg(result.rowsRead);database.rollback();return result;
        }
        if(kind==ChildKind::Parts)colorIt=colors.constFind(f.at(colorCol).trimmed());
        if(identityIt==identities.constEnd()||(kind==ChildKind::Parts&&colorIt==colors.constEnd())){result.message=QStringLiteral("Invalid or unresolved %1 row %2.").arg(expected).arg(result.rowsRead);database.rollback();return result;}
        ++result.setInventoryRows;
        const int revision=revisionIt.value();affected.insert(revision);
        seen.bindValue(":kind",0);seen.bindValue(":revision",revision);seen.bindValue(":identity",identityIt.value());seen.bindValue(":color",kind==ChildKind::Parts?colorIt.value():0);seen.bindValue(":spare",kind==ChildKind::Parts?spare:false);
        if(!seen.exec()){result.message=QStringLiteral("Duplicate logical %1 row at row %2.").arg(expected).arg(result.rowsRead);database.rollback();return result;}
        insert.bindValue(":revision",revision);insert.bindValue(":identity",identityIt.value());insert.bindValue(":quantity",quantity);insert.bindValue(":now",now);
        if(kind==ChildKind::Parts){insert.bindValue(":color",colorIt.value());insert.bindValue(":spare",spare);insert.bindValue(":image",f.at(imageCol).trimmed());}
        if(!insert.exec()){result.message=QStringLiteral("Unable to persist %1 row %2: %3").arg(expected).arg(result.rowsRead).arg(insert.lastError().text());database.rollback();return result;}
        if(insert.numRowsAffected()>0)++result.inserted;
        else {
            update.bindValue(":revision",revision);update.bindValue(":identity",identityIt.value());update.bindValue(":quantity",quantity);update.bindValue(":now",now);
            if(kind==ChildKind::Parts){update.bindValue(":color",colorIt.value());update.bindValue(":spare",spare);update.bindValue(":image",f.at(imageCol).trimmed());}
            if(!update.exec()){result.message=update.lastError().text();database.rollback();return result;}
            if(update.numRowsAffected()>0)++result.updated;else ++result.unchanged;
        }
    }
    if((result.rowsRead&1023)!=0&&progress)progress(result.rowsRead);
    if(token&&token->isCancellationRequested()){result.message=expected+" import cancelled.";database.rollback();return result;}
    if(!result.rowsRead){result.message=expected+" contains no data rows.";database.rollback();return result;}
    QSqlQuery cleanup(database);
    const QString identityColumn=kind==ChildKind::Parts?"part_id":kind==ChildKind::Minifigs?"minifig_catalog_id":"contained_set_catalog_id";
    QString cleanupSql="DELETE FROM "+table+" WHERE set_inventory_revision_id=:revision AND NOT EXISTS (SELECT 1 FROM current_inventory_child c WHERE c.owner_kind=0 AND c.revision_id="+table+".set_inventory_revision_id AND c.identity_id="+table+"."+identityColumn;
    if(kind==ChildKind::Parts)cleanupSql+=" AND c.color_id="+table+".color_id AND c.spare="+table+".is_spare";
    cleanupSql+=")";cleanup.prepare(cleanupSql);
    for(int revision:affected){cleanup.bindValue(":revision",revision);if(!cleanup.exec()){result.message=cleanup.lastError().text();database.rollback();return result;}result.replaced+=cleanup.numRowsAffected();}
    if(kind==ChildKind::Parts){
        QSqlQuery minifigCleanup(database);
        if(!minifigCleanup.exec("DELETE FROM minifig_catalog_part WHERE provider='Rebrickable' AND minifig_catalog_id IN (SELECT minifig_catalog_id FROM current_minifig_owner) AND NOT EXISTS (SELECT 1 FROM current_inventory_child c WHERE c.owner_kind=1 AND c.revision_id=minifig_catalog_part.minifig_catalog_id AND c.identity_id=minifig_catalog_part.part_id AND c.color_id=minifig_catalog_part.color_id AND c.spare=minifig_catalog_part.is_spare)")){result.message=minifigCleanup.lastError().text();database.rollback();return result;}
        result.replaced+=minifigCleanup.numRowsAffected();
    }
    commit(database,result);qInfo()<<expected<<"import elapsed ms:" <<timer.elapsed()<<"rows:" <<result.rowsRead<<"Set-inventory rows:" <<result.setInventoryRows<<"Minifig-owned rows processed:" <<result.minifigPartRows<<"replaced:" <<result.replaced;return result;
}
}

RebrickableInventoryCompositionImporter::Result RebrickableInventoryCompositionImporter::importParts(const QString& f,QSqlDatabase& d,const RebrickableImportCancellation* c,const RebrickableRowProgress& p){return importChildren(ChildKind::Parts,f,d,c,p,m_inventoryOwners,m_minifigInventoryOwners);}
RebrickableInventoryCompositionImporter::Result RebrickableInventoryCompositionImporter::importMinifigs(const QString& f,QSqlDatabase& d,const RebrickableImportCancellation* c,const RebrickableRowProgress& p){return importChildren(ChildKind::Minifigs,f,d,c,p,m_inventoryOwners,m_minifigInventoryOwners);}
RebrickableInventoryCompositionImporter::Result RebrickableInventoryCompositionImporter::importSets(const QString& f,QSqlDatabase& d,const RebrickableImportCancellation* c,const RebrickableRowProgress& p){return importChildren(ChildKind::Sets,f,d,c,p,m_inventoryOwners,m_minifigInventoryOwners);}

RebrickableInventoryCompositionImporter::Result RebrickableInventoryCompositionImporter::selectPreferredRevisions(QSqlDatabase& database) const
{
    Result result;
    QSet<int> desired, current;
    QSqlQuery select(database);if(!select.exec("SELECT r.id FROM set_inventory_revision r WHERE r.provider='Rebrickable' AND r.is_active=1 AND r.id=(SELECT r2.id FROM set_inventory_revision r2 WHERE r2.provider=r.provider AND r2.set_catalog_id=r.set_catalog_id AND r2.is_active=1 ORDER BY r2.version DESC,r2.external_inventory_id DESC LIMIT 1)")){result.message=select.lastError().text();return result;}
    while(select.next()) desired.insert(select.value(0).toInt());
    if(!select.exec("SELECT id FROM set_inventory_revision WHERE provider='Rebrickable' AND is_preferred=1")){result.message=select.lastError().text();return result;}
    while(select.next()) current.insert(select.value(0).toInt());
    if(desired==current){result.success=true;result.message="Preferred revisions are unchanged.";return result;}
    if(!begin(database,result))return result;QSqlQuery clear(database);
    if(!clear.exec("UPDATE set_inventory_revision SET is_preferred=0 WHERE provider='Rebrickable' AND is_preferred=1")){result.message=clear.lastError().text();database.rollback();return result;}
    QSqlQuery update(database);update.prepare("UPDATE set_inventory_revision SET is_preferred=1,modified_utc=:now WHERE id=:id");const QString now=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    for(int id:desired){update.bindValue(":now",now);update.bindValue(":id",id);if(!update.exec()){result.message=update.lastError().text();database.rollback();return result;}}
    result.preferredChanged=(desired-current).size()+(current-desired).size();
    return commit(database,result),result;
}
