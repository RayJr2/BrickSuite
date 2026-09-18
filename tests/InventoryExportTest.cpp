#include "../src/services/inventory/InventoryCsvWriter.h"
#include "../src/services/inventory/InventoryExportService.h"
#include "../src/services/application/dto/RemoteReadDtos.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

namespace { bool check(bool value,const char*message){if(!value)qCritical()<<"FAILED:"<<message;return value;} }

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);bool ok=true;
    InventoryExportRow first;first.inventoryRecordId=11;first.partNumber="3001";first.partName="Brick, \"Special\"";first.category="Bricks";first.color="Bright Red";first.quantity=2;first.storagePath="Workshop / Drawer 1";first.manufacturer="LEGO";first.condition="Used";first.ownership="Owned";first.legoElementIds={"300121","300122"};first.rebrickablePartId="3001";first.rebrickableColorId=4;first.brickLinkPartIds={"3001","old3001"};first.brickLinkColorId="5";
    InventoryExportRow second=first;second.inventoryRecordId=12;second.storagePath="Workshop / Drawer 2";second.manufacturer="Alternate Bricks";second.quantity=1;second.legoElementIds.clear();
    auto defaults=InventoryExportService::defaultConfiguration();auto projection=InventoryExportService::project({first,second},defaults);
    ok&=check(projection.rows.size()==2,"Inventory records remain separate");
    ok&=check(projection.headers.first()=="Part Number"&&projection.headers.contains("Storage Location"),"useful defaults");
    auto bounded=InventoryExportService::project({first,second},defaults,1);ok&=check(bounded.rows.size()==1,"preview bounding");
    auto csv=InventoryCsvWriter::generate(projection);ok&=check(csv.success&&csv.csv.startsWith(QChar(0xFEFF)),"UTF-8 BOM convention");ok&=check(csv.csv.contains("\"Brick, \"\"Special\"\"\""),"CSV quoting");
    defaults.enabledFields.insert("legoElementId");defaults.enabledFields.insert("brickLinkPartId");auto identities=InventoryExportService::project({first},defaults);const int element=identities.headers.indexOf("LEGO Element ID"),brickLink=identities.headers.indexOf("BrickLink Part ID");ok&=check(identities.rows.first().at(element)=="300121;300122"&&identities.rows.first().at(brickLink)=="3001;old3001","deterministic multiple identities");
    auto normalized=InventoryExportService::normalizeConfiguration({"quantity","unknown","partNumber"},{"quantity","unknown"});ok&=check(normalized.fieldOrder.first()=="quantity"&&!normalized.fieldOrder.contains("unknown")&&normalized.enabledFields==QSet<QString>{"quantity"},"stale settings ignored");
    const QSet<QString> remoteFields=InventoryExportService::remoteEnrichmentFieldIds();
    ok&=check(remoteFields==QSet<QString>({"legoElementId","rebrickablePartId","brickLinkPartId","brickLinkColorId"})
                  && (InventoryExportService::defaultConfiguration().enabledFields&remoteFields).isEmpty(),
              "Remote export contract contains only optional enrichment fields");
    RemoteReadDto::InventoryExportRow remote;remote.inventoryRecordId=99;remote.partNumber="x";remote.quantity=7;remote.storagePath="Host / Bin";const auto remoteRows=InventoryExportService::createRemoteRows({remote});ok&=check(remoteRows.size()==1&&remoteRows.first().inventoryRecordId==99&&remoteRows.first().storagePath=="Host / Bin","Host DTO adapter");
    InventorySearchResult core;core.inventoryRecordId=1;core.partId=1;core.partNumber="3001";core.partName="Brick 2 x 4";core.categoryName="Bricks";core.colorId=2;core.rebrickableColorId=4;core.colorName="Red";core.storageLocationId=5;core.storageLocationName="Shelf / Bin";core.manufacturerId=3;core.manufacturerName="LEGO";core.condition="Used";core.ownershipType="Owned";core.quantity=2;
    const auto coreOnly=InventoryExportService().createRows({core},{});
    ok&=check(coreOnly.size()==1&&coreOnly.first().partNumber=="3001"&&coreOnly.first().rebrickablePartId.isEmpty()&&coreOnly.first().legoElementIds.isEmpty()&&coreOnly.first().brickLinkPartIds.isEmpty()&&coreOnly.first().brickLinkColorId.isEmpty(),"disabled expensive fields require no provider enrichment");
    const QString connection=QStringLiteral("InventoryExportTest_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(":memory:");ok&=check(db.open(),"open enrichment database");QSqlQuery q(db);
        ok&=check(q.exec("CREATE TABLE part(id INTEGER PRIMARY KEY,rebrickable_part_id TEXT)"),"create part table");
        ok&=check(q.exec("CREATE TABLE manufacturer(id INTEGER PRIMARY KEY,supports_lego_element_ids INTEGER)"),"create manufacturer table");
        ok&=check(q.exec("CREATE TABLE part_element_identifier(part_id INTEGER,color_id INTEGER,element_id TEXT,is_active INTEGER)"),"create element table");
        ok&=check(q.exec("CREATE TABLE external_part_mapping(part_id INTEGER,provider TEXT,external_id TEXT,mapping_status TEXT)"),"create part mapping table");
        ok&=check(q.exec("CREATE TABLE external_part_identifier(part_id INTEGER,provider TEXT,external_id TEXT,is_active INTEGER)"),"create part identifier table");
        ok&=check(q.exec("CREATE TABLE external_color_mapping(color_id INTEGER,provider TEXT,external_id TEXT,mapping_status TEXT)"),"create color mapping table");
        ok&=check(q.exec("INSERT INTO part VALUES(1,'3001')"),"seed part");ok&=check(q.exec("INSERT INTO manufacturer VALUES(3,1)"),"seed manufacturer");
        ok&=check(q.exec("INSERT INTO part_element_identifier VALUES(1,2,'300121',1)"),"seed element");
        ok&=check(q.exec("INSERT INTO external_part_identifier VALUES(1,'BrickLink','BL-3001',1)"),"seed part identity");
        ok&=check(q.exec("INSERT INTO external_color_mapping VALUES(2,'BrickLink','5','Mapped')"),"seed color identity");
        QList<InventorySearchResult> repeated;for(int i=0;i<600;++i){auto value=core;value.inventoryRecordId=i+1;repeated.append(value);}
        const QSet<QString> requested={"legoElementId","rebrickablePartId","brickLinkPartId","brickLinkColorId"};
        const auto enriched=InventoryExportService(db).createRows(repeated,requested);
        ok&=check(enriched.size()==600&&enriched.first().legoElementIds==QStringList{"300121"}&&enriched.last().rebrickablePartId=="3001"&&enriched.last().brickLinkPartIds==QStringList{"BL-3001"}&&enriched.last().brickLinkColorId=="5","batched enrichment covers complete repeated-identity dataset");
        db.close();
    }
    QSqlDatabase::removeDatabase(connection);
    return ok?0:1;
}
