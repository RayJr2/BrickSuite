#pragma once

#include "SemanticOperand.h"
#include "../LDrawLoadResult.h"
#include <QVector3D>
#include <cmath>

namespace PrintGeometry {

struct CertifiedSourceStud { FunctionalFeature feature; int owner=-1; };

// Only official, certified stud and stud2 ancestry qualifies. In particular,
// neither a hollow cylindrical shape nor another stud-sized primitive does.
inline QVector<CertifiedSourceStud> certifiedSourceStuds(const LDrawGeometry::LDrawLoadResult& source)
{
    QVector<CertifiedSourceStud> studs;
    if(!source.ok()||!source.sourceModel)return studs;
    const auto& model=*source.sourceModel;
    for(const auto& reference:model.references){
        if(reference.fileId<0||reference.fileId>=model.files.size())continue;
        const auto path=model.files[reference.fileId].relativePath;
        const bool solid=path.compare(QStringLiteral("p/stud.dat"),Qt::CaseInsensitive)==0||
                         path.compare(QStringLiteral("stud.dat"),Qt::CaseInsensitive)==0;
        const bool open=path.compare(QStringLiteral("p/stud2.dat"),Qt::CaseInsensitive)==0||
                        path.compare(QStringLiteral("stud2.dat"),Qt::CaseInsensitive)==0;
        if(!solid&&!open)continue;
        const auto& t=reference.accumulatedTransform;
        const auto column=[&](int c){return Point{t[c]*.4,t[8+c]*.4,-t[4+c]*.4};};
        const auto u=column(0),axis=Point{-t[1]*.4,-t[9]*.4,t[5]*.4},v=column(2);
        const auto dot=[](Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;};
        const auto length=[&](Point a){return std::sqrt(dot(a,a));};
        if(std::abs(length(u)-.4)>1e-3||std::abs(length(axis)-.4)>1e-3||
           std::abs(length(v)-.4)>1e-3||std::abs(dot(u,axis))>1e-3||
           std::abs(dot(v,axis))>1e-3||std::abs(dot(u,v))>1e-3)continue;
        int owned=0;bool certified=true;
        for(const auto& surface:model.surfaces){
            int ref=surface.referenceId;
            while(ref>=0&&ref<model.references.size()&&ref!=reference.id)
                ref=model.references[ref].parentId;
            if(ref!=reference.id)continue;
            ++owned;certified&=surface.certified;
        }
        if(!certified||owned<20)continue;
        FunctionalFeature feature;
        feature.family=FunctionalInterfaceFamily::StandardStud;
        feature.role=FunctionalInterfaceRole::Male;
        feature.materialSide=FunctionalMaterialSide::MaterialInside;
        feature.eligibility=FunctionalEligibility::Eligible;
        feature.confidence=SemanticConfidence::HighConfidence;
        feature.operandAction=FunctionalOperandAction::Unite;
        feature.frame.origin={t[3]*.4,t[11]*.4,-t[7]*.4};
        feature.frame.axis={axis.x/.4,axis.y/.4,axis.z/.4};
        feature.frame.profileU={u.x/.4,u.y/.4,u.z/.4};
        feature.frame.profileV={
            feature.frame.axis.y*feature.frame.profileU.z-feature.frame.axis.z*feature.frame.profileU.y,
            feature.frame.axis.z*feature.frame.profileU.x-feature.frame.axis.x*feature.frame.profileU.z,
            feature.frame.axis.x*feature.frame.profileU.y-feature.frame.axis.y*feature.frame.profileU.x};
        feature.frame.mirrored=reference.mirrored;
        feature.nominalRadiusMillimetres=2.4;
        feature.nominalDiameterMillimetres=4.8;
        feature.nominalAxialExtentMillimetres=1.6;
        feature.nominalEngagementExtentMillimetres=1.6;
        feature.protectedInnerRadiusMillimetres=open?1.6:0.0;
        feature.radialProfile={{0,2.4},{1.6,2.4}};
        feature.constructionRecipe=open?QStringLiteral("standard-open-stud-v1"):
                                        QStringLiteral("standard-solid-stud-v1");
        feature.evidenceContract=QStringLiteral("official-ldraw-standard-stud-v1");
        feature.provenance={{path,reference.id,reference.sourceLine,reference.inverted}};
        feature.stableIdentity=QStringLiteral("source-surface-standard-stud:%1").arg(reference.id);
        feature.governingOperandIdentity=feature.stableIdentity+QStringLiteral(":surface");
        studs.push_back({feature,reference.id});
    }
    return studs;
}
}
