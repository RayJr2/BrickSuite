#include "InventoryBuildabilityRepository.h"

#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>

namespace {
QString key(int partId, int colorId)
{
    return QString::number(partId) + QLatin1Char(':') + QString::number(colorId);
}

struct CollectionSourceData {
    int id = 0;
    QString label;
    QString state;
    QHash<QString, int> quantities;
};

const char* effectivePartsCte = R"(
    WITH RECURSIVE effective_parts AS (
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
    )
)";

bool execPrepared(QSqlQuery& query, QString* error)
{
    if (query.exec()) return true;
    if (error) *error = query.lastError().text();
    return false;
}
}

InventoryBuildabilitySearchResult InventoryBuildabilityRepository::search(
    const InventoryBuildabilitySearch& request) const
{
    InventoryBuildabilitySearchResult out;
    if (request.workspaceId <= 0) {
        out.errorMessage = QStringLiteral("Select a Workspace before evaluating buildability.");
        return out;
    }
    if (request.yearFrom > 0 && request.yearTo > 0 && request.yearFrom > request.yearTo) {
        out.errorMessage = QStringLiteral("Year From cannot be later than Year To.");
        return out;
    }
    QSqlDatabase db = repositoryDatabase();
    QHash<QString, int> loose;
    QSqlQuery inventory(db);
    inventory.prepare(R"(
        WITH owned AS (
            SELECT ir.part_id,ir.color_id,SUM(ir.quantity) quantity
              FROM inventory_record ir
              JOIN storage_location sl ON sl.id=ir.storage_location_id
             WHERE ir.workspace_id=:workspace AND ir.ownership_type='Owned'
               AND ir.quantity>0 AND sl.is_active=1 AND sl.allows_inventory=1
               AND NOT EXISTS (SELECT 1 FROM storage_location child
                                WHERE child.parent_location_id=sl.id AND child.is_active=1)
             GROUP BY ir.part_id,ir.color_id
        ), allocated AS (
            SELECT ba.part_id,ba.color_id,SUM(ba.quantity_allocated) quantity
              FROM build_allocation ba JOIN build b ON b.id=ba.build_id
             WHERE b.workspace_id=:workspace
             GROUP BY ba.part_id,ba.color_id
        )
        SELECT o.part_id,o.color_id,MAX(o.quantity-COALESCE(a.quantity,0),0)
          FROM owned o LEFT JOIN allocated a
            ON a.part_id=o.part_id AND a.color_id=o.color_id
    )");
    inventory.bindValue(QStringLiteral(":workspace"), request.workspaceId);
    if (!execPrepared(inventory, &out.errorMessage)) return out;
    while (inventory.next()) loose.insert(key(inventory.value(0).toInt(), inventory.value(1).toInt()),
                                           inventory.value(2).toInt());

    QList<CollectionSourceData> collection;
    if (request.includeCollection) {
        QSqlQuery sources(db);
        sources.prepare(R"(
            SELECT ci.id,ci.set_catalog_id,
                   COALESCE(NULLIF(ci.nickname,''),sc.set_number||' — '||sc.name),ci.state
              FROM collection_item ci JOIN set_catalog sc ON sc.id=ci.set_catalog_id
             WHERE ci.workspace_id=:workspace AND ci.item_type='Set' AND ci.is_active=1
               AND ci.allow_parts_source=1 AND ci.completeness='Complete'
               AND (EXISTS (SELECT 1 FROM set_inventory_revision sir
                             JOIN set_inventory_part sip ON sip.set_inventory_revision_id=sir.id
                            WHERE sir.set_catalog_id=ci.set_catalog_id
                              AND sir.provider='Rebrickable' AND sir.is_active=1
                              AND sir.is_preferred=1 AND sip.is_spare=0)
                    OR (NOT EXISTS (SELECT 1 FROM set_inventory_revision sir
                                    WHERE sir.set_catalog_id=ci.set_catalog_id
                                      AND sir.provider='Rebrickable' AND sir.is_active=1
                                      AND sir.is_preferred=1)
                        AND EXISTS (SELECT 1 FROM set_catalog_part scp
                                    WHERE scp.set_catalog_id=ci.set_catalog_id AND scp.is_spare=0)))
             ORDER BY ci.id
        )");
        sources.bindValue(QStringLiteral(":workspace"), request.workspaceId);
        if (!execPrepared(sources, &out.errorMessage)) return out;
        QHash<int, int> sourceIndexById;
        QHash<int, QList<int>> sourceIndexesBySet;
        while (sources.next()) {
            CollectionSourceData source;
            source.id=sources.value(0).toInt(); source.label=sources.value(2).toString();
            source.state=sources.value(3).toString();
            sourceIndexById.insert(source.id, collection.size());
            sourceIndexesBySet[sources.value(1).toInt()].append(collection.size());
            collection.append(source);
        }
        out.eligibleCollectionSources = collection.size();
        QSqlQuery dormant(db);
        dormant.prepare(R"(
            SELECT COUNT(*) FROM collection_item ci
             WHERE ci.workspace_id=:workspace AND ci.item_type='Set'
               AND ci.allow_parts_source=1
               AND (ci.is_active=0 OR ci.completeness<>'Complete'
                    OR NOT (EXISTS (SELECT 1 FROM set_inventory_revision sir
                                     JOIN set_inventory_part sip ON sip.set_inventory_revision_id=sir.id
                                    WHERE sir.set_catalog_id=ci.set_catalog_id
                                      AND sir.provider='Rebrickable' AND sir.is_active=1
                                      AND sir.is_preferred=1 AND sip.is_spare=0)
                            OR (NOT EXISTS (SELECT 1 FROM set_inventory_revision sir
                                           WHERE sir.set_catalog_id=ci.set_catalog_id
                                             AND sir.provider='Rebrickable' AND sir.is_active=1
                                             AND sir.is_preferred=1)
                                AND EXISTS (SELECT 1 FROM set_catalog_part scp
                                            WHERE scp.set_catalog_id=ci.set_catalog_id AND scp.is_spare=0))))
        )");
        dormant.bindValue(QStringLiteral(":workspace"),request.workspaceId);
        if(!execPrepared(dormant,&out.errorMessage)||!dormant.next())return out;
        out.dormantCollectionSources=dormant.value(0).toInt();
        if (!collection.isEmpty()) {
            QSqlQuery pieces(db);
            const QString sql=QString::fromLatin1(effectivePartsCte)+QStringLiteral(R"(
                SELECT ep.set_id,ep.part_id,ep.color_id,SUM(ep.quantity)
                  FROM effective_parts ep
                 WHERE EXISTS (SELECT 1 FROM collection_item ci
                                WHERE ci.set_catalog_id=ep.set_id
                                  AND ci.workspace_id=:workspace
                                  AND ci.item_type='Set' AND ci.is_active=1
                                  AND ci.allow_parts_source=1
                                  AND ci.completeness='Complete')
                 GROUP BY ep.set_id,ep.part_id,ep.color_id
            )");
            pieces.prepare(sql); pieces.bindValue(QStringLiteral(":workspace"),request.workspaceId);
            if (!execPrepared(pieces,&out.errorMessage)) return out;
            while(pieces.next()) {
                const QString k=key(pieces.value(1).toInt(),pieces.value(2).toInt());
                const int quantity=pieces.value(3).toInt();
                for(int index:sourceIndexesBySet.value(pieces.value(0).toInt()))
                    collection[index].quantities.insert(k,quantity);
            }
        }
    }

    // Stage one discovers only Set candidates and presentation metadata. No unsafe
    // buildability cut-off is applied here: Collection sources can turn a weak loose
    // match into a fully buildable advisory result.
    QSqlQuery beforeCount(db);
    const QString beforeSql=QString::fromLatin1(effectivePartsCte)+QStringLiteral(R"(
        SELECT COUNT(*) FROM set_catalog sc
         WHERE (:text='' OR sc.set_number LIKE :numberPattern OR sc.name LIKE :namePattern)
           AND EXISTS (SELECT 1 FROM effective_parts ep WHERE ep.set_id=sc.id)
    )");
    if(!beforeCount.prepare(beforeSql)){out.errorMessage=beforeCount.lastError().text();return out;}
    const QString text=request.text.trimmed(); const QString pattern=QStringLiteral("%%1%").arg(text);
    beforeCount.bindValue(QStringLiteral(":text"),text);beforeCount.bindValue(QStringLiteral(":numberPattern"),pattern);beforeCount.bindValue(QStringLiteral(":namePattern"),pattern);
    if(!execPrepared(beforeCount,&out.errorMessage)||!beforeCount.next())return out;
    out.candidateCountBeforeFilters=beforeCount.value(0).toInt();

    QSqlQuery candidates(db);
    const QString candidateSql=QString::fromLatin1(effectivePartsCte)+QStringLiteral(R"(
        , effective_totals AS (
            SELECT set_id,SUM(quantity) total_quantity FROM effective_parts GROUP BY set_id
        ), selected_theme(id) AS (
            SELECT id FROM theme_catalog WHERE id=:theme AND is_active=1
            UNION ALL
            SELECT tc.id FROM theme_catalog tc JOIN selected_theme st
              ON tc.parent_theme_catalog_id=st.id WHERE tc.is_active=1
        ), theme_paths(id,qualified_name) AS (
            SELECT id,name FROM theme_catalog
             WHERE parent_theme_catalog_id IS NULL AND is_active=1
            UNION ALL
            SELECT tc.id,tp.qualified_name||' → '||tc.name
              FROM theme_catalog tc JOIN theme_paths tp
                ON tc.parent_theme_catalog_id=tp.id
             WHERE tc.is_active=1
        )
        SELECT sc.id,sc.set_number,sc.name,sc.year,sc.theme_id,
               COALESCE(CAST(tei.external_id AS INTEGER),0),
               COALESCE(tp.qualified_name,''),sc.image_url,sc.num_parts
          FROM set_catalog sc
          JOIN effective_totals totals ON totals.set_id=sc.id
          LEFT JOIN theme_paths tp ON tp.id=sc.theme_id
          LEFT JOIN theme_external_identifier tei
            ON tei.theme_catalog_id=sc.theme_id AND tei.provider='Rebrickable'
           AND tei.is_active=1
         WHERE (:text='' OR sc.set_number LIKE :numberPattern OR sc.name LIKE :namePattern)
           AND (:exactSet=0 OR sc.id=:exactSet)
           AND totals.total_quantity>=:minimumSetParts
           AND (:yearFrom=0 OR (sc.year>0 AND sc.year>=:yearFrom))
           AND (:yearTo=0 OR (sc.year>0 AND sc.year<=:yearTo))
           AND (:theme=0 OR sc.theme_id IN (SELECT id FROM selected_theme))
         ORDER BY sc.id
    )");
    if (!candidates.prepare(candidateSql)) { out.errorMessage=candidates.lastError().text(); return out; }
    candidates.bindValue(QStringLiteral(":text"),text);
    candidates.bindValue(QStringLiteral(":numberPattern"),pattern);
    candidates.bindValue(QStringLiteral(":namePattern"),pattern);
    candidates.bindValue(QStringLiteral(":minimumSetParts"),qBound(1,request.minimumSetParts,10000));
    candidates.bindValue(QStringLiteral(":yearFrom"),qMax(0,request.yearFrom));
    candidates.bindValue(QStringLiteral(":yearTo"),qMax(0,request.yearTo));
    candidates.bindValue(QStringLiteral(":theme"),qMax(0,request.themeCatalogId));
    candidates.bindValue(QStringLiteral(":exactSet"),qMax(0,request.exactSetCatalogId));
    if (!execPrepared(candidates,&out.errorMessage)) return out;

    QHash<int,InventoryBuildabilitySetResult> metadata;
    QStringList candidateParameters;
    int candidateNumber=0;
    while(candidates.next()) {
        InventoryBuildabilitySetResult value;value.setCatalogId=candidates.value(0).toInt();
        value.setNumber=candidates.value(1).toString();value.name=candidates.value(2).toString();
        value.year=candidates.value(3).toInt();value.themeCatalogId=candidates.value(4).toInt();
        value.rebrickableThemeId=candidates.value(5).toInt();
        value.themeName=candidates.value(6).toString();value.imageUrl=candidates.value(7).toString();
        value.catalogPartCount=candidates.value(8).toInt();metadata.insert(value.setCatalogId,value);
        candidateParameters.append(QStringLiteral(":candidate%1").arg(candidateNumber++));
    }
    out.candidateCountAfterCatalogFilters=metadata.size();
    if (metadata.isEmpty()) { out.success=true; return out; }

    // Stage two batch-loads exact effective Part+Color requirements for the complete
    // safe candidate set. This is one query, never a per-Set composition lookup.
    QSqlQuery requirements(db);
    const QString requirementSql=QString::fromLatin1(effectivePartsCte)+QStringLiteral(R"(
        SELECT ep.set_id,ep.part_id,ep.color_id,p.part_number,p.name,c.name,
               COALESCE(c.rebrickable_id,-1),SUM(ep.quantity)
          FROM effective_parts ep JOIN part p ON p.id=ep.part_id JOIN color c ON c.id=ep.color_id
         WHERE ep.set_id IN (%1)
         GROUP BY ep.set_id,ep.part_id,ep.color_id
         ORDER BY ep.set_id,ep.part_id,ep.color_id
    )").arg(candidateParameters.join(','));
    if(!requirements.prepare(requirementSql)){out.errorMessage=requirements.lastError().text();return out;}
    int bindIndex=0;for(auto it=metadata.cbegin();it!=metadata.cend();++it)
        requirements.bindValue(candidateParameters.at(bindIndex++),it.key());
    if(!execPrepared(requirements,&out.errorMessage))return out;

    int current=0;
    InventoryBuildabilitySetResult item;
    auto finish=[&] {
        if (item.setCatalogId<=0 || item.totalQuantity<=0) return;
        item.missingQuantity=item.totalQuantity-item.advisorySatisfiedQuantity;
        const int activePercent=request.includeCollection?item.advisoryPercent():item.loosePercent();
        if ((!request.fullyBuildableOnly && activePercent>=qBound(0,request.minimumPercent,100))
            || (request.fullyBuildableOnly && activePercent==100)) out.sets.append(item);
    };
    while(requirements.next()) {
        const int id=requirements.value(0).toInt();
        if(id!=current){finish(); current=id; item=metadata.value(id);}
        InventoryBuildabilityRequirement req;
        req.partId=requirements.value(1).toInt();req.colorId=requirements.value(2).toInt();
        req.partNumber=requirements.value(3).toString();req.partName=requirements.value(4).toString();
        req.colorName=requirements.value(5).toString();
        req.rebrickableColorId=requirements.value(6).toInt();req.required=requirements.value(7).toInt();
        const QString k=key(req.partId,req.colorId);
        req.looseAvailable=loose.value(k);req.looseUsed=qMin(req.required,req.looseAvailable);
        int remaining=req.required-req.looseUsed;
        for(const auto& source:collection){if(remaining<=0)break;const int used=qMin(remaining,source.quantities.value(k));
            if(used<=0)continue;remaining-=used;req.collectionUsed+=used;
            auto it=std::find_if(item.sources.begin(),item.sources.end(),[&](const auto& x){return x.collectionItemId==source.id;});
            if(it==item.sources.end())item.sources.append({source.id,source.label,source.state,used});else it->piecesUsed+=used;}
        req.missing=remaining; item.totalQuantity+=req.required;item.looseSatisfiedQuantity+=req.looseUsed;
        item.advisorySatisfiedQuantity+=req.looseUsed+req.collectionUsed; ++item.totalRequirements;
        if(req.looseUsed==req.required)++item.looseSatisfiedRequirements;
        if(req.missing==0)++item.advisorySatisfiedRequirements;
        item.requirements.append(req);
    }
    finish();
    out.preciseEvaluationCount=metadata.size();
    std::sort(out.sets.begin(),out.sets.end(),[&](const auto&a,const auto&b){
        const int ap=request.includeCollection?a.advisoryPercent():a.loosePercent();
        const int bp=request.includeCollection?b.advisoryPercent():b.loosePercent();
        if(request.fullyBuildableFirst&&(ap==100)!=(bp==100))return ap==100;
        if(ap!=bp)return ap>bp;if(a.missingQuantity!=b.missingQuantity)return a.missingQuantity<b.missingQuantity;
        if(a.loosePercent()!=b.loosePercent())return a.loosePercent()>b.loosePercent();
        return a.setNumber.compare(b.setNumber,Qt::CaseInsensitive)<0;});
    out.qualifyingCount=out.sets.size();const int cap=qBound(50,request.maximumResults,2000);
    out.capReached=out.sets.size()>cap;if(out.capReached)out.sets=out.sets.mid(0,cap);
    out.success=true;return out;
}
