#include "../src/database/DatabaseSchema.h"
#include "../src/import/RebrickableElementImporter.h"
#include "../src/repositories/EffectiveSetCompositionRepository.h"
#include "../src/repositories/PartElementIdentifierRepository.h"
#include "../src/services/minifigs/RebrickableMinifigThemeDerivationService.h"

#include <QCoreApplication>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <cstdio>

namespace {
bool require(bool ok,const QString& message){if(!ok)std::fprintf(stderr,"%s\n",qPrintable(message));return ok;}
bool exec(QSqlDatabase db,const QString& sql){QSqlQuery q(db);return q.exec(sql);}
int scalar(QSqlDatabase db,const QString& sql){QSqlQuery q(db);return q.exec(sql)&&q.next()?q.value(0).toInt():-1;}
bool write(const QString& path,const QByteArray& bytes){QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();}
}

int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);QTemporaryDir dir;if(!require(dir.isValid(),"Temporary directory failed."))return 1;
    const QString connection="m25-elements";QSqlDatabase db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(dir.filePath("test.db"));
    if(!require(db.open()&&DatabaseSchema::initialize(db)&&scalar(db,"SELECT version FROM schema_version")==35,"Fresh schema 35 failed."))return 1;
    const QString now="2026-01-01T00:00:00.000Z";
    if(!require(exec(db,"INSERT INTO part(part_number,name,rebrickable_part_id,is_active,created_utc,modified_utc,material) VALUES('p1','One','p1',1,'n','n','Plastic'),('p2','Two','p2',1,'n','n','Plastic')")
        &&exec(db,"INSERT INTO color(name,rebrickable_id,created_utc,modified_utc) VALUES('Red',1,'n','n'),('Blue',2,'n','n')")
        &&exec(db,"INSERT INTO set_catalog(set_number,name,year,theme_id,num_parts,image_url,created_utc,modified_utc) VALUES('s1','Set',2026,7,2,'','n','n')")
        &&exec(db,"INSERT INTO minifig_catalog(name,num_parts,image_url,is_active,created_utc,modified_utc) VALUES('Fig',1,'',1,'n','n')")
        &&exec(db,"INSERT INTO theme_catalog(name,parent_theme_catalog_id,is_active,created_utc,modified_utc) VALUES('Theme',NULL,1,'n','n'),('Other',NULL,1,'n','n')")
        &&exec(db,"INSERT INTO theme_external_identifier(theme_catalog_id,provider,external_id,source,is_active,created_utc,modified_utc) SELECT id,'Rebrickable','7','test',1,'n','n' FROM theme_catalog WHERE name='Theme'"),"Seed failed."))return 1;
    const int part=scalar(db,"SELECT id FROM part WHERE part_number='p1'"),color=scalar(db,"SELECT id FROM color WHERE rebrickable_id=1"),set=scalar(db,"SELECT id FROM set_catalog WHERE set_number='s1'"),fig=scalar(db,"SELECT id FROM minifig_catalog"),theme=scalar(db,"SELECT id FROM theme_catalog WHERE name='Theme'"),otherTheme=scalar(db,"SELECT id FROM theme_catalog WHERE name='Other'");
    const QString elements=dir.filePath("elements.csv");
    const QByteArray blankDesignSnapshot="element_id,part_num,color_id,design_id\ne1,p1,1,\ne2,p1,1,d1\n";
    write(elements,blankDesignSnapshot);
    RebrickableElementImporter importer;auto imported=importer.importFile(elements,db);
    PartElementIdentifierRepository elementRepo(db);
    if(!require(imported.success&&imported.inserted==2&&elementRepo.findByElementId("Rebrickable","e1").partId==part&&elementRepo.findByElementId("Rebrickable","e1").designId.isEmpty()&&elementRepo.findByPartColor(part,color,"Rebrickable").size()==2,"Blank Element design import/repository failed."))return 1;
    write(elements,blankDesignSnapshot);imported=importer.importFile(elements,db);
    if(!require(imported.success&&imported.rowsRead==2&&imported.unchanged==2&&imported.inserted==0&&imported.updated==0&&imported.reactivated==0&&imported.deactivated==0,"Identical Element reimport was not unchanged."))return 1;
    write(elements,"element_id,part_num,color_id,design_id\ne1,p1,1,d2\ne2,p1,1,d1\n");imported=importer.importFile(elements,db);
    if(!require(imported.success&&imported.updated==1&&elementRepo.findByElementId("Rebrickable","e1").designId=="d2","Blank-to-populated Element design update failed."))return 1;
    write(elements,"element_id,part_num,color_id,design_id\ne1,p1,1,d3\ne2,p1,1,d1\n");imported=importer.importFile(elements,db);
    if(!require(imported.success&&imported.updated==1&&elementRepo.findByElementId("Rebrickable","e1").designId=="d3","Populated-to-populated Element design update failed."))return 1;
    write(elements,blankDesignSnapshot);imported=importer.importFile(elements,db);
    if(!require(imported.success&&imported.updated==1&&elementRepo.findByElementId("Rebrickable","e1").designId.isEmpty(),"Populated-to-blank Element design update failed."))return 1;
    if(!require(scalar(db,"SELECT COUNT(*) FROM pragma_index_list('part_element_identifier') WHERE [unique]=1")==1
        &&scalar(db,"SELECT COUNT(*) FROM pragma_index_list('part_element_identifier') WHERE name='idx_part_element_part_color'")==1
        &&scalar(db,"SELECT COUNT(*) FROM pragma_foreign_key_list('part_element_identifier')")==2,"Element constraints/indexes are incomplete."))return 1;
    if(!require(exec(db,QStringLiteral("INSERT INTO part_element_identifier(provider,element_id,part_id,color_id,design_id,is_active,created_utc,modified_utc) VALUES('OtherProvider','other',%1,%2,'d',1,'n','n')").arg(part).arg(color)),"Provider seed failed."))return 1;
    write(elements,"element_id,part_num,color_id,design_id\ne1,p1,1,\n");imported=importer.importFile(elements,db);
    if(!require(imported.success&&imported.unchanged==1&&imported.deactivated==1&&scalar(db,"SELECT COUNT(*) FROM part_element_identifier WHERE provider='OtherProvider' AND is_active=1")==1,"Element deactivation/provider isolation failed."))return 1;
    write(elements,"element_id,part_num,color_id,design_id\ne1,missing,1,d\n");
    imported=importer.importFile(elements,db);
    if(!require(!imported.success&&imported.message.contains("Unresolved Element Part")&&scalar(db,"SELECT COUNT(*) FROM part_element_identifier WHERE provider='Rebrickable' AND is_active=1")==1,"Unresolved Element Part snapshot changed data."))return 1;
    write(elements,"element_id,part_num,color_id,design_id\ne1,p1,999,d\n");
    imported=importer.importFile(elements,db);
    if(!require(!imported.success&&imported.message.contains("Unresolved Element Color")&&scalar(db,"SELECT COUNT(*) FROM part_element_identifier WHERE provider='Rebrickable' AND is_active=1")==1,"Unresolved Element Color changed the snapshot."))return 1;
    write(elements,"element_id,part_num,color_id,design_id\ne1,p1,invalid,d\n");imported=importer.importFile(elements,db);
    if(!require(!imported.success&&imported.message.contains("Invalid Element color value")&&scalar(db,"SELECT COUNT(*) FROM part_element_identifier WHERE provider='Rebrickable' AND is_active=1")==1,"Invalid Element Color changed the snapshot."))return 1;
    write(elements,"element_id,part_num,color_id,design_id\ne1,p1,1,changed\ne1,p2,2,d2\n");imported=importer.importFile(elements,db);
    if(!require(!imported.success&&imported.message.contains("Duplicate Element ID")&&elementRepo.findByElementId("Rebrickable","e1").designId.isEmpty()&&scalar(db,"SELECT COUNT(*) FROM part_element_identifier WHERE provider='Rebrickable' AND is_active=1")==1,"Duplicate Element snapshot did not roll back."))return 1;
    write(elements,blankDesignSnapshot);imported=importer.importFile(elements,db);
    if(!require(imported.success&&imported.reactivated==1&&elementRepo.findByElementId("Rebrickable","e2").active,"Element reactivation failed."))return 1;
    RebrickableImportCancellation cancelled;cancelled.requestCancellation();if(!require(!importer.importFile(elements,db,&cancelled).success,"Cancelled Elements import succeeded."))return 1;

    if(!require(exec(db,QStringLiteral("INSERT INTO set_catalog_part(set_catalog_id,part_id,color_id,quantity_required,is_spare,provider,source,created_utc,modified_utc) VALUES(%1,%2,%3,9,0,'Legacy','test','n','n')").arg(set).arg(part).arg(color)),"Fallback seed failed."))return 1;
    EffectiveSetCompositionRepository effective(db);auto composition=effective.forSet(set,false);
    if(!require(composition.success&&composition.source==EffectiveSetCompositionSource::LegacyCatalogFallback&&composition.parts.size()==1&&composition.parts.first().quantity==9,"Legacy fallback failed."))return 1;
    if(!require(exec(db,QStringLiteral("INSERT INTO set_inventory_revision(provider,external_inventory_id,set_catalog_id,version,is_active,is_preferred,created_utc,modified_utc) VALUES('Rebrickable','r1',%1,2,1,1,'n','n')").arg(set)),"Revision seed failed."))return 1;
    const int revision=scalar(db,"SELECT id FROM set_inventory_revision WHERE external_inventory_id='r1'");
    if(!require(exec(db,QStringLiteral("INSERT INTO set_inventory_part(set_inventory_revision_id,part_id,color_id,quantity,is_spare,image_url,created_utc,modified_utc) VALUES(%1,%2,%3,3,0,'','n','n'),(%1,%2,%3,1,1,'','n','n')").arg(revision).arg(part).arg(color)),"Revision composition seed failed."))return 1;
    composition=effective.forSet(set,true);auto required=effective.forSet(set,false);
    if(!require(composition.source==EffectiveSetCompositionSource::PreferredRebrickableRevision&&composition.revisionId==revision&&composition.revisionVersion==2&&composition.parts.size()==2&&required.parts.size()==1&&required.parts.first().quantity==3,"Preferred/spare effective composition failed."))return 1;
    if(!require(exec(db,QStringLiteral("INSERT INTO set_inventory_minifig(set_inventory_revision_id,minifig_catalog_id,quantity,created_utc,modified_utc) VALUES(%1,%2,1,'n','n')").arg(revision).arg(fig))
        &&exec(db,QStringLiteral("INSERT INTO minifig_theme(minifig_catalog_id,theme_catalog_id,provider) VALUES(%1,%2,'Rebrickable'),(%1,%3,'OtherProvider')").arg(fig).arg(otherTheme).arg(theme)),"Theme seed failed."))return 1;
    if(!require(exec(db,"CREATE TRIGGER fail_derived_theme BEFORE INSERT ON minifig_theme WHEN NEW.provider='Rebrickable' BEGIN SELECT RAISE(ABORT,'forced theme failure'); END"),"Theme rollback trigger failed."))return 1;
    auto derived=RebrickableMinifigThemeDerivationService().rebuild(db);
    if(!require(!derived.success&&scalar(db,QStringLiteral("SELECT COUNT(*) FROM minifig_theme WHERE minifig_catalog_id=%1 AND theme_catalog_id=%2 AND provider='Rebrickable'").arg(fig).arg(otherTheme))==1&&scalar(db,"SELECT COUNT(*) FROM minifig_theme WHERE provider='OtherProvider'")==1,"Failed Theme rebuild did not roll back."))return 1;
    if(!require(exec(db,"DROP TRIGGER fail_derived_theme"),"Theme rollback trigger cleanup failed."))return 1;
    derived=RebrickableMinifigThemeDerivationService().rebuild(db);
    if(!require(derived.success&&derived.associations==1&&scalar(db,QStringLiteral("SELECT COUNT(*) FROM minifig_theme WHERE minifig_catalog_id=%1 AND theme_catalog_id=%2 AND provider='Rebrickable'").arg(fig).arg(theme))==1&&scalar(db,"SELECT COUNT(*) FROM minifig_theme WHERE provider='OtherProvider'")==1,"Theme derivation/provider isolation failed."))return 1;

    if(!require(exec(db,"DROP TABLE remote_mutation_receipt")&&exec(db,"UPDATE schema_version SET version=33")&&exec(db,"DROP TABLE part_element_identifier")&&DatabaseSchema::initialize(db)&&scalar(db,"SELECT version FROM schema_version")==35&&scalar(db,"SELECT COUNT(*) FROM set_inventory_revision")==1,"Schema 33 to 35 migration lost composition."))return 1;
    db.close();db=QSqlDatabase();QSqlDatabase::removeDatabase(connection);return 0;
}
