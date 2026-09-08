#include "RebrickableElementImporter.h"

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
QStringList parseCsv(const QString& line,bool& ok){QStringList out;QString field;bool quoted=false;ok=true;for(int i=0;i<line.size();++i){QChar c=line.at(i);if(c=='"'){if(quoted&&i+1<line.size()&&line.at(i+1)=='"'){field+=c;++i;}else quoted=!quoted;}else if(c==','&&!quoted){out.append(field);field.clear();}else field+=c;}ok=!quoted;out.append(field);return out;}
bool loadMap(QSqlDatabase& db,const QString& sql,QHash<QString,int>& map,QString& error){QSqlQuery q(db);if(!q.exec(sql)){error=q.lastError().text();return false;}while(q.next())map.insert(q.value(0).toString(),q.value(1).toInt());return true;}
}

RebrickableElementImporter::Result RebrickableElementImporter::importFile(
    const QString& fileName,QSqlDatabase& database,const RebrickableImportCancellation* cancellation,
    const RebrickableRowProgress& progress) const
{
    Result result;QElapsedTimer timer;timer.start();QTemporaryDir temporary;QString path,error;
    if(!RebrickableCsvInputResolver::resolve(fileName,"elements.csv",temporary,path,error)){result.message=error;return result;}
    QFile file(path);if(!file.open(QIODevice::ReadOnly|QIODevice::Text)){result.message=file.errorString();return result;}
    QTextStream stream(&file);QString headerLine=stream.readLine();if(!headerLine.isEmpty()&&headerLine.front()==QChar(0xfeff))headerLine.remove(0,1);
    bool ok=false;const QStringList headers=parseCsv(headerLine,ok),expected={"element_id","part_num","color_id","design_id"};
    if(!ok||headers!=expected){result.message="Unsupported Elements CSV header.";return result;}
    if(cancellation&&cancellation->isCancellationRequested()){result.message="Elements import cancelled.";return result;}
    QHash<QString,int> parts,colors;
    if(!loadMap(database,"SELECT part_number,id FROM part",parts,result.message)||!loadMap(database,"SELECT CAST(rebrickable_id AS TEXT),id FROM color WHERE rebrickable_id IS NOT NULL",colors,result.message))return result;
    if(!database.transaction()){result.message=database.lastError().text();return result;}
    struct Existing{int id,part,color;QString design;bool active;};QHash<QString,Existing> old;QSqlQuery q(database);
    if(!q.exec("SELECT element_id,id,part_id,color_id,design_id,is_active FROM part_element_identifier WHERE provider='Rebrickable'")){result.message=q.lastError().text();database.rollback();return result;}
    while(q.next())old.insert(q.value(0).toString(),{q.value(1).toInt(),q.value(2).toInt(),q.value(3).toInt(),q.value(4).toString(),q.value(5).toBool()});
    QSqlQuery insert(database),update(database);insert.prepare("INSERT INTO part_element_identifier(provider,element_id,part_id,color_id,design_id,is_active,created_utc,modified_utc) VALUES('Rebrickable',:element,:part,:color,:design,1,:now,:now)");
    update.prepare("UPDATE part_element_identifier SET part_id=:part,color_id=:color,design_id=:design,is_active=1,modified_utc=:now WHERE id=:id");
    const QString now=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);QSet<QString> seen;
    while(!stream.atEnd()){
        const QString line=stream.readLine();if(line.trimmed().isEmpty())continue;++result.rowsRead;
        if((result.rowsRead&1023)==0){if(progress)progress(result.rowsRead);if(cancellation&&cancellation->isCancellationRequested()){result.message="Elements import cancelled.";database.rollback();return result;}}
        const QStringList f=parseCsv(line,ok);bool colorOk=false;if(f.size()==4)f.at(2).trimmed().toInt(&colorOk);
        const QString element=f.size()==4?f.at(0).trimmed():QString(),part=f.size()==4?f.at(1).trimmed():QString(),color=f.size()==4?f.at(2).trimmed():QString(),design=f.size()==4?f.at(3).trimmed():QString();
        const auto partIt=parts.constFind(part),colorIt=colors.constFind(color);
        if(!ok||f.size()!=4||element.isEmpty()||part.isEmpty()||design.isEmpty()||!colorOk||partIt==parts.constEnd()||colorIt==colors.constEnd()||seen.contains(element)){result.message=QStringLiteral("Invalid, duplicate, or unresolved Element at row %1.").arg(result.rowsRead);database.rollback();return result;}
        seen.insert(element);const auto oldIt=old.constFind(element);
        if(oldIt==old.constEnd()){insert.bindValue(":element",element);insert.bindValue(":part",partIt.value());insert.bindValue(":color",colorIt.value());insert.bindValue(":design",design);insert.bindValue(":now",now);if(!insert.exec()){result.message=insert.lastError().text();database.rollback();return result;}++result.inserted;}
        else if(oldIt->part==partIt.value()&&oldIt->color==colorIt.value()&&oldIt->design==design&&oldIt->active)++result.unchanged;
        else{update.bindValue(":part",partIt.value());update.bindValue(":color",colorIt.value());update.bindValue(":design",design);update.bindValue(":now",now);update.bindValue(":id",oldIt->id);if(!update.exec()){result.message=update.lastError().text();database.rollback();return result;}if(!oldIt->active)++result.reactivated;if(oldIt->part!=partIt.value()||oldIt->color!=colorIt.value()||oldIt->design!=design)++result.updated;}
    }
    if((result.rowsRead&1023)!=0&&progress)progress(result.rowsRead);
    if(cancellation&&cancellation->isCancellationRequested()){result.message="Elements import cancelled.";database.rollback();return result;}
    if(!result.rowsRead){result.message="Elements CSV contains no data rows.";database.rollback();return result;}
    QSqlQuery deactivate(database);deactivate.prepare("UPDATE part_element_identifier SET is_active=0,modified_utc=:now WHERE provider='Rebrickable' AND element_id=:element AND is_active=1");
    for(auto it=old.cbegin();it!=old.cend();++it)if(it->active&&!seen.contains(it.key())){deactivate.bindValue(":now",now);deactivate.bindValue(":element",it.key());if(!deactivate.exec()){result.message=deactivate.lastError().text();database.rollback();return result;}result.deactivated+=deactivate.numRowsAffected();}
    if(!database.commit()){result.message=database.lastError().text();database.rollback();return result;}
    result.success=true;result.message="Elements import completed successfully.";qInfo()<<"Elements import elapsed ms:" <<timer.elapsed()<<"rows:" <<result.rowsRead;return result;
}
