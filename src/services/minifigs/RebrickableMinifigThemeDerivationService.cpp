#include "RebrickableMinifigThemeDerivationService.h"

#include "../../database/DatabaseManager.h"

#include <QSqlError>
#include <QSqlQuery>

RebrickableMinifigThemeDerivationService::Result
RebrickableMinifigThemeDerivationService::rebuild(QSqlDatabase database) const
{
    Result result;if(!database.isValid())database=DatabaseManager::instance().database();
    if(!database.transaction()){result.message=database.lastError().text();return result;}
    auto fail=[&](const QString& message){database.rollback();result.message=message;return result;};
    QSqlQuery q(database);
    if(!q.exec("CREATE TEMP TABLE IF NOT EXISTS derived_rebrickable_minifig_theme(minifig_catalog_id INTEGER,theme_catalog_id INTEGER,PRIMARY KEY(minifig_catalog_id,theme_catalog_id))")
        ||!q.exec("DELETE FROM derived_rebrickable_minifig_theme"))return fail(q.lastError().text());
    if(!q.exec("INSERT INTO derived_rebrickable_minifig_theme(minifig_catalog_id,theme_catalog_id) "
               "SELECT DISTINCT sim.minifig_catalog_id,tei.theme_catalog_id "
               "FROM set_inventory_revision sir "
               "JOIN set_inventory_minifig sim ON sim.set_inventory_revision_id=sir.id "
               "JOIN set_catalog sc ON sc.id=sir.set_catalog_id "
               "JOIN theme_external_identifier tei ON tei.provider='Rebrickable' AND tei.is_active=1 AND tei.external_id=CAST(sc.theme_id AS TEXT) "
               "WHERE sir.provider='Rebrickable' AND sir.is_active=1 AND sir.is_preferred=1"))return fail(q.lastError().text());
    if(!q.exec("SELECT COUNT(*) FROM minifig_theme WHERE provider='Rebrickable'")||!q.next())return fail(q.lastError().text());result.replaced=q.value(0).toLongLong();
    if(!q.exec("DELETE FROM minifig_theme WHERE provider='Rebrickable'"))return fail(q.lastError().text());
    if(!q.exec("INSERT INTO minifig_theme(minifig_catalog_id,theme_catalog_id,provider) SELECT minifig_catalog_id,theme_catalog_id,'Rebrickable' FROM derived_rebrickable_minifig_theme"))return fail(q.lastError().text());
    result.associations=q.numRowsAffected();
    if(!database.commit())return fail(database.lastError().text());
    result.success=true;result.message="Rebrickable Minifig Theme associations rebuilt from preferred Set composition.";return result;
}
