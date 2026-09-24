#include "ManufacturingMeshService.h"
#include "FrictionlessTechnicPinSemantic.h"
#include "FrictionTechnicPinSemantic.h"
#include "TechnicAxleSemantic.h"
#include "RoundTechnicPassageSemantic.h"
#include "StudReceivingPostSemantic.h"
#include "StudReceivingWallPocketSemantic.h"
#include "StudReceivingAntiStudSemantic.h"
#include "StandardBarSemantic.h"
#include "CClipBarReceiverSemantic.h"
#include "BallJointSemantic.h"
#include "BallSocketSemantic.h"
#include "PinBarrelHingeSemantic.h"
#include "InterleavedFingerHingeSemantic.h"
#include "ClickHingeSemantic.h"
#include "StandardStudSourceSemantic.h"
#include "FunctionalOperandRegenerator.h"
#include "McutMeshBooleanService.h"
#include "PrintMeshAnalysis.h"
#include <QCryptographicHash>
#include <algorithm>
#include <cmath>
#include <limits>

namespace PrintGeometry { namespace {
ManufacturingMeshResult fail(ManufacturingMeshError e,const QString&m){ManufacturingMeshResult r;r.error=e;r.diagnostic=m;return r;}
int order(SemanticRole r){return r==SemanticRole::PrimaryBody?0:r==SemanticRole::SubtractivePassage?1:r==SemanticRole::AdditiveAttachment?2:3;}
QString operandIdentity(const SemanticOperand&o){return o.sourceFiles.join('|');}
QString orientationName(FitPrintedOrientation o){return o==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate?"feature-axis-perpendicular-to-build-plate":o==FitPrintedOrientation::FeatureAxisParallelToBuildPlate?"feature-axis-parallel-to-build-plate":"unsupported";}
bool matches(const FitProfileCorrection&c,const QString&family,const QString&role,const QString&semantics,const QString&orientation){return c.featureFamily==family&&c.featureRole==role&&c.printedOrientation==orientation&&c.units==QStringLiteral("millimetres")&&c.semantics==semantics;}
const FitProfileCorrection* wallPocketCorrection(const FitProfile& profile,
    const FunctionalFeature& feature, FitPrintedOrientation orientation)
{
    if (feature.constructionRecipe != QStringLiteral("stud-receiving-wall-pocket-square-v1") ||
        orientation != FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate ||
        !FitCalibrationLibrary::profileCompatibility(profile)) return nullptr;
    const FitProfileCorrection* sharedOpening = nullptr;
    for (const auto& correction : profile.corrections)
        if (matches(correction,"StudReceivingClutch","female",
                    "female-stud-receiver-wall-pocket-opening-width",
                    "feature-axis-perpendicular-to-build-plate") &&
            correction.correctionContractVersion == QStringLiteral("female-stud-receiver-wall-pocket-opening-width-v1")) {
            if (correction.semanticContractVersion == feature.evidenceContract) return &correction;
            // The shallow fixture measures the common stud-engagement opening, not pocket depth.
            if (feature.evidenceContract == QStringLiteral("official-ldraw-box5-wall-pocket-brick-v1") &&
                correction.semanticContractVersion == QStringLiteral("official-ldraw-box5-wall-pocket-plate-v1"))
                sharedOpening = &correction;
        }
    return sharedOpening;
}
bool correctionApplies(const FunctionalFeature&feature,const ManufacturingMeshCorrections&corrections){if(feature.constructionRecipe==QStringLiteral("stud-receiving-antistud-bore-v1"))return corrections.receivingAntiStudBoreDiameter&&feature.evidenceContract==corrections.receivingAntiStudBoreDiameter->semanticContractVersion;return (feature.family==FunctionalInterfaceFamily::RoundTechnicPassage&&feature.role==FunctionalInterfaceRole::Female&&corrections.femaleDiameter&&feature.evidenceContract==corrections.femaleDiameter->semanticContractVersion)||(feature.family==FunctionalInterfaceFamily::StandardStud&&feature.role==FunctionalInterfaceRole::Male&&((corrections.studDiameter&&feature.evidenceContract==corrections.studDiameter->semanticContractVersion)||(corrections.studHeight&&feature.evidenceContract==corrections.studHeight->semanticContractVersion)))||(feature.family==FunctionalInterfaceFamily::StudReceivingClutch&&feature.role==FunctionalInterfaceRole::Female&&((corrections.receivingTubeDiameter&&feature.evidenceContract==corrections.receivingTubeDiameter->semanticContractVersion)||(corrections.receivingPostDiameter&&feature.evidenceContract==corrections.receivingPostDiameter->semanticContractVersion)||(corrections.receivingWallPocketWidth&&feature.evidenceContract==corrections.receivingWallPocketWidth->semanticContractVersion)))||(feature.family==FunctionalInterfaceFamily::FrictionlessTechnicPin&&feature.role==FunctionalInterfaceRole::Male&&corrections.frictionlessPinDiameter&&feature.evidenceContract==corrections.frictionlessPinDiameter->semanticContractVersion)||(feature.family==FunctionalInterfaceFamily::FrictionTechnicPin&&feature.role==FunctionalInterfaceRole::Male&&corrections.frictionPinDiameter&&feature.evidenceContract==corrections.frictionPinDiameter->semanticContractVersion)||(feature.family==FunctionalInterfaceFamily::TechnicAxle&&feature.role==FunctionalInterfaceRole::Male&&corrections.technicAxleTipToTip&&feature.evidenceContract==corrections.technicAxleTipToTip->semanticContractVersion)||(feature.family==FunctionalInterfaceFamily::TechnicAxleHole&&feature.role==FunctionalInterfaceRole::Female&&corrections.technicAxleHoleArmWidth)||(feature.family==FunctionalInterfaceFamily::StandardBar&&feature.role==FunctionalInterfaceRole::Male&&corrections.standardBarDiameter&&feature.evidenceContract==corrections.standardBarDiameter->semanticContractVersion);}
QString axisName(const Point&axis){return QStringLiteral("(%1, %2, %3)").arg(axis.x,0,'g',4).arg(axis.y,0,'g',4).arg(axis.z,0,'g',4);}
Point add(Point a,Point b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Point subtract(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Point scaled(Point a,double s){return {a.x*s,a.y*s,a.z*s};}
double dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
double length(Point a){return std::sqrt(dot(a,a));}
Point cross(Point a,Point b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Point converted(const QVector3D& p){return {p.x()*.4,p.z()*.4,-p.y()*.4};}
Point closest(Point p,Point a,Point b,Point c){
    const auto ab=subtract(b,a),ac=subtract(c,a),ap=subtract(p,a);
    const double d1=dot(ab,ap),d2=dot(ac,ap);
    if(d1<=0&&d2<=0)return a;
    const auto bp=subtract(p,b);const double d3=dot(ab,bp),d4=dot(ac,bp);
    if(d3>=0&&d4<=d3)return b;
    const double vc=d1*d4-d3*d2;
    if(vc<=0&&d1>=0&&d3<=0)return add(a,scaled(ab,d1/(d1-d3)));
    const auto cp=subtract(p,c);const double d5=dot(ab,cp),d6=dot(ac,cp);
    if(d6>=0&&d5<=d6)return c;
    const double vb=d5*d2-d1*d6;
    if(vb<=0&&d2>=0&&d6<=0)return add(a,scaled(ac,d2/(d2-d6)));
    const double va=d3*d6-d5*d4;
    if(va<=0&&d4-d3>=0&&d5-d6>=0){const auto bc=subtract(c,b);return add(b,scaled(bc,(d4-d3)/(d4-d3+d5-d6)));}
    const double inv=1.0/(va+vb+vc);return add(a,add(scaled(ab,vb*inv),scaled(ac,vc*inv)));
}
bool adjustCertifiedStud(const LDrawGeometry::LDrawLoadResult& source,const PrintMesh& nominal,
                         const CertifiedSourceStud& stud,double diameterCorrection,double heightCorrection,
                         PrintMesh* adjusted,QString* diagnostic){
    if(!adjusted||!source.sourceModel||!std::isfinite(diameterCorrection)||
       !std::isfinite(heightCorrection)||4.8+diameterCorrection<=0||1.6+heightCorrection<=0)
        return false;
    if(diameterCorrection==0.0&&heightCorrection==0.0){*adjusted=nominal;return true;}
    const auto& model=*source.sourceModel;
    const bool open=stud.feature.constructionRecipe==QStringLiteral("standard-open-stud-v1");
    struct Surface{Point a,b,c;bool owned;};
    QVector<Surface> surfaces;surfaces.reserve(model.surfaces.size());
    for(const auto& surface:model.surfaces){
        if(!surface.certified||surface.triangleIndex<0||surface.triangleIndex>=source.mesh.triangles.size())
            return false;
        int ref=surface.referenceId;
        while(ref>=0&&ref<model.references.size()&&ref!=stud.owner)
            ref=model.references[ref].parentId;
        const auto& triangle=source.mesh.triangles[surface.triangleIndex];
        surfaces.push_back({converted(triangle.a),converted(triangle.b),converted(triangle.c),ref==stud.owner});
    }
    const auto& frame=stud.feature.frame;
    QVector<std::size_t> ownedVertices;
    double measuredRadius=0,measuredHeight=0;
    for(std::size_t i=0;i<nominal.vertices.size();++i){
        const auto relative=subtract(nominal.vertices[i],frame.origin);
        const double axial=dot(relative,frame.axis);
        const auto radial=subtract(relative,scaled(frame.axis,axial));
        const double radius=length(radial);
        if(axial<-.02||axial>1.72||radius>2.55)continue;
        double ownedDistance=std::numeric_limits<double>::max();
        double otherDistance=std::numeric_limits<double>::max();
        for(const auto& surface:surfaces){
            const double distance=length(subtract(nominal.vertices[i],
                closest(nominal.vertices[i],surface.a,surface.b,surface.c)));
            auto& nearest=surface.owned?ownedDistance:otherDistance;
            nearest=std::min(nearest,distance);
        }
        if(ownedDistance>.12||ownedDistance>otherDistance+1e-10)continue;
        ownedVertices.push_back(i);
        if(axial>.4)measuredRadius=std::max(measuredRadius,radius);
        measuredHeight=std::max(measuredHeight,axial);
    }
    if(ownedVertices.size()<24||measuredRadius<2.2||measuredHeight<1.4){
        if(diagnostic)*diagnostic=QStringLiteral("Certified stud surface was not retained in the PreparedMesh.");
        return false;
    }
    *adjusted=nominal;
    const double radialDelta=diameterCorrection==0.0?0.0:(2.4+diameterCorrection*.5)-measuredRadius;
    const double axialDelta=heightCorrection==0.0?0.0:(1.6+heightCorrection)-measuredHeight;
    for(const auto index:ownedVertices){
        auto& point=adjusted->vertices[index];
        const auto relative=subtract(point,frame.origin);
        const double axial=dot(relative,frame.axis);
        const auto radial=subtract(relative,scaled(frame.axis,axial));
        const double radius=length(radial);
        const double baseWeight=std::clamp(axial/.16,0.0,1.0);
        // The open stud's inner 3.20 mm bore is not an OD fit surface. Move
        // its upper rim axially with the stud height, but never radially.
        const double exteriorWeight=open?std::clamp((radius-1.6)/.8,0.0,1.0):1.0;
        const double radialScale=radius>1e-9?radialDelta*baseWeight*exteriorWeight/measuredRadius:0.0;
        point=add(point,add(scaled(radial,radialScale),
                             scaled(frame.axis,axialDelta*std::clamp(axial/measuredHeight,0.0,1.0))));
    }
    if(diagnostic)*diagnostic=QStringLiteral("Certified stud surface adjusted to %1 mm OD and %2 mm height (%3 vertices).")
        .arg(4.8+diameterCorrection,0,'f',3).arg(1.6+heightCorrection,0,'f',3).arg(ownedVertices.size());
    return true;
}
}

ManufacturingMeshService::ManufacturingMeshService(BooleanServiceFactory f,SemanticBuilderFunction b):m_factory(f?std::move(f):[]{return std::make_unique<McutMeshBooleanService>();}),m_builder(std::move(b)){}

ManufacturingMeshCorrections ManufacturingMeshService::compatibleCorrections(const FitProfile&profile,FitPrintedOrientation orientation,QString*reason)
{
    ManufacturingMeshCorrections result;QString compatibility;if(!FitCalibrationLibrary::profileCompatibility(profile,&compatibility)){if(reason)*reason=compatibility;return result;}const QString printedOrientation=orientationName(orientation);const QString barOrientation=orientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate?QStringLiteral("axis-perpendicular-to-build-plate"):QStringLiteral("unsupported");const FitProfileCorrection*heightCandidate=nullptr;
    for(const auto&correction:profile.corrections){if(matches(correction,"RoundTechnicPassage","female","female-diameter-clearance",printedOrientation))result.femaleDiameter=&correction;else if(matches(correction,"StandardStud","male","male-stud-diameter",printedOrientation))result.studDiameter=&correction;else if(matches(correction,"StandardStud","male","male-stud-height",printedOrientation))heightCandidate=&correction;else if(matches(correction,"StudReceivingClutch","female","female-stud-receiver-tube-od",printedOrientation))result.receivingTubeDiameter=&correction;else if(matches(correction,"StudReceivingClutch","female","female-stud-receiver-post-od",printedOrientation))result.receivingPostDiameter=&correction;else if(matches(correction,"StudReceivingClutch","female","female-stud-receiver-wall-pocket-opening-width",printedOrientation))result.receivingWallPocketWidth=&correction;else if((matches(correction,"StandardBar","male","male-standard-bar-diameter",printedOrientation)||matches(correction,"StandardBar","male","male-standard-bar-diameter",barOrientation))&&correction.correctionContractVersion==QStringLiteral("male-standard-bar-diameter-v1"))result.standardBarDiameter=&correction;else if(matches(correction,"FrictionlessTechnicPin","male","male-frictionless-technic-pin-envelope-diameter",printedOrientation))result.frictionlessPinDiameter=&correction;else if(matches(correction,"FrictionTechnicPin","male","male-friction-technic-pin-ridge-envelope-diameter",printedOrientation))result.frictionPinDiameter=&correction;else if(matches(correction,"TechnicAxle","male","male-technic-axle-tip-to-tip-envelope",printedOrientation))result.technicAxleTipToTip=&correction;else if(matches(correction,"TechnicAxleHole","female","female-technic-axle-hole-arm-width-clearance",printedOrientation)&&correction.correctionContractVersion==QStringLiteral("female-technic-axle-hole-arm-width-clearance-v2")&&correction.hasFixedTipToTipCorrection)result.technicAxleHoleArmWidth=&correction;}
    for(const auto& correction:profile.corrections)
        if(matches(correction,"StudReceivingClutch","female","female-stud-receiver-antistud-bore-diameter",printedOrientation) &&
           correction.correctionContractVersion==QStringLiteral("female-stud-receiver-antistud-bore-diameter-v1"))
            result.receivingAntiStudBoreDiameter=&correction;
    if(orientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate ||
       orientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate)
        for(const auto& correction:profile.corrections)
            if(matches(correction,"CClipBarReceiver","female",
                       "female-c-clip-contact-arc-and-throat-clearance",printedOrientation) &&
               correction.correctionContractVersion==QStringLiteral("female-c-clip-contact-arc-and-throat-clearance-v1"))
                result.cClipClearance=&correction;
    if(orientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate ||
       orientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate)
        for(const auto& correction:profile.corrections)
            if(matches(correction,"BallJoint","male","male-ball-joint-spherical-diameter",printedOrientation) &&
               correction.correctionContractVersion==QStringLiteral("male-ball-joint-spherical-diameter-v1"))
                result.ballJointDiameter=&correction;
    if(orientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate)
        for(const auto& correction:profile.corrections)
            if(matches(correction,"BallSocket","female",
                       "female-ball-socket-contact-and-throat-clearance",printedOrientation) &&
               correction.correctionContractVersion==QStringLiteral("female-ball-socket-contact-and-throat-clearance-v1"))
                result.ballSocketClearance=&correction;
    if(orientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate)
        for(const auto& correction:profile.corrections)
            if(matches(correction,"PinBarrelHinge","male","male-pin-barrel-hinge-pin-od",printedOrientation) &&
               correction.correctionContractVersion==QStringLiteral("male-pin-barrel-hinge-pin-od-v1"))
                result.pinBarrelHingeDiameter=&correction;
    if(orientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate)
        for(const auto& correction:profile.corrections)
            if(matches(correction,"InterleavedFingerHinge","male",
                       "male-interleaved-finger-contact-bump-protrusion",printedOrientation) &&
               correction.correctionContractVersion==QStringLiteral("male-interleaved-finger-contact-bump-protrusion-v1"))
                result.interleavedFingerBump=&correction;
    if(orientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate)
        for(const auto& correction:profile.corrections)
            if(matches(correction,"ClickHinge","male",
                       "male-click-hinge-arrestor-radial-reach",printedOrientation) &&
               correction.correctionContractVersion==QStringLiteral("male-click-hinge-arrestor-radial-reach-v1"))
                result.clickHingeArrestor=&correction;
    if(heightCandidate){if(!heightCandidate->hasRequiredDiameterCorrection)result.studHeightDiagnostic=QStringLiteral("The Verified Stud Height correction does not record its required Stud OD context and was not applied.");else if(!result.studDiameter)result.studHeightDiagnostic=QStringLiteral("The Verified Stud Height correction requires its matching Verified Stud OD correction and was not applied.");else if(std::abs(result.studDiameter->valueMillimetres-heightCandidate->requiredDiameterCorrectionMillimetres)>1e-9)result.studHeightDiagnostic=QStringLiteral("The Verified Stud Height correction was measured with a different Stud OD correction and was not applied.");else result.studHeight=heightCandidate;}
    if(reason)*reason=result.any()?(result.studHeightDiagnostic.isEmpty()?QStringLiteral("Compatible"):result.studHeightDiagnostic):(result.studHeightDiagnostic.isEmpty()?QStringLiteral("The selected profile has no compatible verified functional correction for this print orientation."):result.studHeightDiagnostic);return result;
}

const FitProfileCorrection* ManufacturingMeshService::compatibleCorrection(const FitProfile&profile,FitPrintedOrientation orientation,QString*reason)
{
    const auto corrections=compatibleCorrections(profile,orientation,reason);for(const auto* candidate:{corrections.femaleDiameter,corrections.studDiameter,corrections.studHeight,corrections.receivingTubeDiameter,corrections.receivingPostDiameter,corrections.receivingWallPocketWidth,corrections.receivingAntiStudBoreDiameter,corrections.frictionlessPinDiameter,corrections.frictionPinDiameter,corrections.technicAxleTipToTip,corrections.technicAxleHoleArmWidth,corrections.standardBarDiameter,corrections.cClipClearance,corrections.ballJointDiameter,corrections.ballSocketClearance,corrections.pinBarrelHingeDiameter,corrections.interleavedFingerBump,corrections.clickHingeArrestor})if(candidate)return candidate;return nullptr;
}

FitPrintedOrientation ManufacturingMeshService::transformedOrientation(const FunctionalFeature&feature,const PrintOrientation&printOrientation)
{
    const auto axis=printOrientation.map(feature.frame.axis);const double z=std::abs(axis.z);
    if(z>=1.0-1e-6)return FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;
    if(z<=1e-6)return FitPrintedOrientation::FeatureAxisParallelToBuildPlate;
    return FitPrintedOrientation::Unknown;
}

bool ManufacturingMeshService::hasApplicableCorrection(const FitProfile&profile,const LDrawSemanticOperandBuilder::Result&semantic,const PrintOrientation&printOrientation,QString*reason)
{
    if(!semantic.ok()){if(reason)*reason=semantic.diagnostics.join(' ');return false;}
    for(const auto&operand:semantic.operands)for(const auto&feature:operand.functionalFeatures){QString featureReason;const auto orientation=transformedOrientation(feature,printOrientation);const auto corrections=compatibleCorrections(profile,orientation,&featureReason);if(correctionApplies(feature,corrections)||wallPocketCorrection(profile,feature,orientation)||(feature.family==FunctionalInterfaceFamily::CClipBarReceiver&&corrections.cClipClearance&&feature.evidenceContract==corrections.cClipClearance->semanticContractVersion)||(feature.family==FunctionalInterfaceFamily::BallJoint&&corrections.ballJointDiameter&&feature.evidenceContract==corrections.ballJointDiameter->semanticContractVersion)||(feature.family==FunctionalInterfaceFamily::BallSocket&&corrections.ballSocketClearance&&feature.evidenceContract==corrections.ballSocketClearance->semanticContractVersion)){if(reason)*reason=QStringLiteral("Compatible correction for feature %1 after Print Orientation %2.").arg(feature.stableIdentity,printOrientation.summary());return true;}}
    if(reason)*reason=QStringLiteral("No recognized functional operand has a compatible Verified correction after Print Orientation %1.").arg(printOrientation.summary());return false;
}

bool ManufacturingMeshService::hasApplicableCorrection(const FitProfile&profile,const LDrawGeometry::LDrawLoadResult&source,const PrintOrientation&printOrientation,QString*reason)
{
    QString semanticReason;if(hasApplicableCorrection(profile,LDrawSemanticOperandBuilder::build(source),printOrientation,&semanticReason))return true;
    for(const auto&feature:FrictionlessTechnicPinSemantic::recognize(source)){QString featureReason;const auto corrections=compatibleCorrections(profile,transformedOrientation(feature,printOrientation),&featureReason);if(correctionApplies(feature,corrections)){if(reason)*reason=QStringLiteral("Compatible Verified frictionless-pin correction for feature %1 after Print Orientation %2.").arg(feature.stableIdentity,printOrientation.summary());return true;}}
    for(const auto&feature:FrictionTechnicPinSemantic::recognize(source)){QString featureReason;const auto corrections=compatibleCorrections(profile,transformedOrientation(feature,printOrientation),&featureReason);if(correctionApplies(feature,corrections)){if(reason)*reason=QStringLiteral("Compatible Verified friction-pin ridge correction for feature %1 after Print Orientation %2.").arg(feature.stableIdentity,printOrientation.summary());return true;}}
    for(const auto&feature:TechnicAxleSemantic::recognizeAxles(source)){QString featureReason;const auto corrections=compatibleCorrections(profile,transformedOrientation(feature,printOrientation),&featureReason);if(correctionApplies(feature,corrections)){if(reason)*reason=QStringLiteral("Compatible Verified Technic-axle correction for feature %1 after Print Orientation %2.").arg(feature.stableIdentity,printOrientation.summary());return true;}}
    for(const auto&feature:TechnicAxleSemantic::recognizeAxleHoles(source)){QString featureReason;const auto corrections=compatibleCorrections(profile,transformedOrientation(feature,printOrientation),&featureReason);if(correctionApplies(feature,corrections)){if(reason)*reason=QStringLiteral("Compatible Verified Technic axle-hole arm-width correction for feature %1 after Print Orientation %2.").arg(feature.stableIdentity,printOrientation.summary());return true;}}
    for(const auto&feature:RoundTechnicPassageSemantic::recognize(source)){QString featureReason;const auto corrections=compatibleCorrections(profile,transformedOrientation(feature,printOrientation),&featureReason);if(correctionApplies(feature,corrections)){if(reason)*reason=QStringLiteral("Compatible Verified round Technic passage correction for feature %1 after Print Orientation %2.").arg(feature.stableIdentity,printOrientation.summary());return true;}}
    for(const auto&feature:StudReceivingPostSemantic::recognize(source)){QString featureReason;const auto corrections=compatibleCorrections(profile,transformedOrientation(feature,printOrientation),&featureReason);if(correctionApplies(feature,corrections)){if(reason)*reason=QStringLiteral("Compatible Verified PostWallCell correction for feature %1 after Print Orientation %2.").arg(feature.stableIdentity,printOrientation.summary());return true;}}
    for(const auto& stud:certifiedSourceStuds(source)){
        const auto corrections=compatibleCorrections(profile,transformedOrientation(stud.feature,printOrientation));
        if(correctionApplies(stud.feature,corrections)){
            if(reason)*reason=QStringLiteral("Compatible Verified source-owned Standard Stud correction for feature %1 after Print Orientation %2.")
                .arg(stud.feature.stableIdentity,printOrientation.summary());
            return true;
        }
    }
    for(const auto& feature:CClipBarReceiverSemantic::recognize(source)) {
        const auto corrections=compatibleCorrections(profile,transformedOrientation(feature,printOrientation));
        if(corrections.cClipClearance && corrections.cClipClearance->semanticContractVersion==feature.evidenceContract) {
            if(reason)*reason=QStringLiteral("Compatible Verified C-Clip contact/throat correction for feature %1 after Print Orientation %2.").arg(feature.stableIdentity,printOrientation.summary());
            return true;
        }
    }
    for(const auto& feature:BallJointSemantic::recognize(source)) {
        const auto corrections=compatibleCorrections(profile,transformedOrientation(feature,printOrientation));
        if(corrections.ballJointDiameter && corrections.ballJointDiameter->semanticContractVersion==feature.evidenceContract) {
            if(reason)*reason=QStringLiteral("Compatible Verified Ball Joint sphere correction for feature %1 after Print Orientation %2.").arg(feature.stableIdentity,printOrientation.summary());
            return true;
        }
    }
    for(const auto& feature:BallSocketSemantic::recognize(source)) {
        const auto corrections=compatibleCorrections(profile,transformedOrientation(feature,printOrientation));
        if(corrections.ballSocketClearance && corrections.ballSocketClearance->semanticContractVersion==feature.evidenceContract) {
            if(reason)*reason=QStringLiteral("Compatible Verified Ball Socket contact/throat correction for feature %1 after Print Orientation %2.").arg(feature.stableIdentity,printOrientation.summary());
            return true;
        }
    }
    for(const auto& feature:PinBarrelHingeSemantic::recognize(source)) {
        if(feature.role!=FunctionalInterfaceRole::Male)continue;
        const auto corrections=compatibleCorrections(profile,transformedOrientation(feature,printOrientation));
        if(corrections.pinBarrelHingeDiameter&&
           corrections.pinBarrelHingeDiameter->semanticContractVersion==feature.evidenceContract) {
            if(reason)*reason=QStringLiteral("Compatible Verified Pin / Barrel Hinge male-pin OD correction for feature %1 after Print Orientation %2.")
                .arg(feature.stableIdentity,printOrientation.summary());
            return true;
        }
    }
    for(const auto& feature:InterleavedFingerHingeSemantic::recognize(source)) {
        if(feature.role!=FunctionalInterfaceRole::Male)continue;
        const auto corrections=compatibleCorrections(profile,transformedOrientation(feature,printOrientation));
        if(corrections.interleavedFingerBump&&
           corrections.interleavedFingerBump->semanticContractVersion==feature.evidenceContract) {
            if(reason)*reason=QStringLiteral("Compatible Verified interleaved-finger contact-bump correction for %1 after Print Orientation %2.")
                .arg(feature.stableIdentity,printOrientation.summary());
            return true;
        }
    }
    for(const auto& feature:ClickHingeSemantic::recognize(source)) {
        if(feature.role!=FunctionalInterfaceRole::Male)continue;
        const auto corrections=compatibleCorrections(profile,transformedOrientation(feature,printOrientation));
        if(corrections.clickHingeArrestor&&
           corrections.clickHingeArrestor->semanticContractVersion==feature.evidenceContract) {
            if(reason)*reason=QStringLiteral("Compatible Verified paired click-arrestor correction for %1 after Print Orientation %2.")
                .arg(feature.stableIdentity,printOrientation.summary());
            return true;
        }
    }
    if(reason)*reason=semanticReason;return false;
}

ManufacturingMeshResult ManufacturingMeshService::generate(const LDrawGeometry::LDrawLoadResult&source,const PreparedMesh&prepared,const FitProfile&profile,const PrintOrientation&printOrientation)const
{
    if(!source.ok()||prepared.mesh.faces.empty()||prepared.partReference.isEmpty())return fail(ManufacturingMeshError::InvalidInput,"Source and nominal PreparedMesh are required.");
    const bool nominalHingeProfile=profile.profileIdentity.isEmpty()&&profile.corrections.isEmpty();
    QString reason;if(!nominalHingeProfile&&!FitCalibrationLibrary::profileCompatibility(profile,&reason))return fail(ManufacturingMeshError::IncompatibleProfile,reason);
    // Localized, source-owned families share one mesh. Compose every applicable
    // correction and reject overlapping vertex ownership rather than returning
    // after the first family. Semantic operands are handled by the path below.
    enum class SurfaceKind { Ball, Socket, Clip, Bar, HingePin, InterleavedBump, ClickArrestor, Stud };
    struct SurfaceFeature { FunctionalFeature feature; SurfaceKind kind; int owner=-1; };
    QVector<SurfaceFeature> surfaceFeatures;
    const auto retained=[&](const FunctionalFeature& feature){
        return std::any_of(prepared.functionalFeatures.cbegin(),prepared.functionalFeatures.cend(),
            [&](const FunctionalFeature& candidate){return candidate.stableIdentity==feature.stableIdentity;});
    };
    const auto balls=BallJointSemantic::recognize(source);
    if(balls.size()==1&&retained(balls.front()))
        surfaceFeatures.push_back({balls.front(),SurfaceKind::Ball,-1});
    const auto sockets=BallSocketSemantic::recognize(source);
    if(sockets.size()==1&&retained(sockets.front()))
        surfaceFeatures.push_back({sockets.front(),SurfaceKind::Socket,-1});
    const auto clips=CClipBarReceiverSemantic::recognize(source);
    if(clips.size()==1&&retained(clips.front()))
        surfaceFeatures.push_back({clips.front(),SurfaceKind::Clip,-1});
    const auto bars=StandardBarSemantic::recognize(source);
    if(bars.size()==1&&retained(bars.front()))
        surfaceFeatures.push_back({bars.front(),SurfaceKind::Bar,-1});
    const auto hinges=PinBarrelHingeSemantic::recognize(source);
    for(const auto& hinge:hinges)
        if(hinge.role==FunctionalInterfaceRole::Male&&retained(hinge))
            surfaceFeatures.push_back({hinge,SurfaceKind::HingePin,-1});
    const auto interleaved=InterleavedFingerHingeSemantic::recognize(source);
    for(const auto& hinge:interleaved)
        if((hinge.role==FunctionalInterfaceRole::Male||nominalHingeProfile)&&retained(hinge))
            surfaceFeatures.push_back({hinge,SurfaceKind::InterleavedBump,-1});
    const auto clicks=ClickHingeSemantic::recognize(source);
    for(const auto& hinge:clicks)
        if(hinge.role==FunctionalInterfaceRole::Male&&retained(hinge))
            surfaceFeatures.push_back({hinge,SurfaceKind::ClickArrestor,-1});
    if(nominalHingeProfile&&std::none_of(surfaceFeatures.cbegin(),surfaceFeatures.cend(),
       [](const SurfaceFeature& item){return item.kind==SurfaceKind::InterleavedBump;}))
        return fail(ManufacturingMeshError::MissingCorrection,
            QStringLiteral("Nominal fallback requires a retained certified three-finger hinge."));
    if(prepared.preparationMethod.contains(QStringLiteral("source-surface"),Qt::CaseInsensitive)||
       !surfaceFeatures.isEmpty())
        for(const auto& stud:certifiedSourceStuds(source))
            if(stud.feature.constructionRecipe!=QStringLiteral("standard-open-stud-v1")||retained(stud.feature))
                surfaceFeatures.push_back({stud.feature,SurfaceKind::Stud,stud.owner});
    if(!surfaceFeatures.isEmpty()){
        PrintMesh adjusted=prepared.mesh;
        std::vector<bool> moved(adjusted.vertices.size(),false);
        QStringList featureIdentities,semanticContracts,correctionContracts,correctionIdentities,provenance,unmatched;
        double nominalDiameter=0,diameterCorrection=0,manufacturingDiameter=0;
        double nominalHeight=0,heightCorrection=0,manufacturingHeight=0;
        SurfaceKind firstKind=surfaceFeatures.front().kind;
        auto accept=[&](PrintMesh&& next)->bool{
            if(next.faces!=adjusted.faces||next.vertices.size()!=adjusted.vertices.size())return false;
            for(std::size_t i=0;i<next.vertices.size();++i){
                const auto& a=adjusted.vertices[i];const auto& b=next.vertices[i];
                const bool changed=a.x!=b.x||a.y!=b.y||a.z!=b.z;
                if(changed&&moved[i])return false;
            }
            for(std::size_t i=0;i<next.vertices.size();++i){
                const auto& a=adjusted.vertices[i];const auto& b=next.vertices[i];
                if(a.x!=b.x||a.y!=b.y||a.z!=b.z)moved[i]=true;
            }
            adjusted=std::move(next);
            return true;
        };
        for(const auto& item:surfaceFeatures){
            const auto orientation=transformedOrientation(item.feature,printOrientation);
            QString correctionReason;
            const auto corrections=compatibleCorrections(profile,orientation,&correctionReason);
            const FitProfileCorrection* primary=nullptr;
            const FitProfileCorrection* height=nullptr;
            switch(item.kind){
            case SurfaceKind::Ball: primary=corrections.ballJointDiameter;break;
            case SurfaceKind::Socket: primary=corrections.ballSocketClearance;break;
            case SurfaceKind::Clip: primary=corrections.cClipClearance;break;
            case SurfaceKind::Bar: primary=corrections.standardBarDiameter;break;
            case SurfaceKind::HingePin: primary=corrections.pinBarrelHingeDiameter;break;
            case SurfaceKind::InterleavedBump: primary=corrections.interleavedFingerBump;break;
            case SurfaceKind::ClickArrestor: primary=corrections.clickHingeArrestor;break;
            case SurfaceKind::Stud: primary=corrections.studDiameter;height=corrections.studHeight;break;
            }
            if(primary&&primary->semanticContractVersion!=item.feature.evidenceContract)primary=nullptr;
            if(height&&height->semanticContractVersion!=item.feature.evidenceContract)height=nullptr;
            if(!primary&&!height){
                if(item.kind==SurfaceKind::InterleavedBump){
                    featureIdentities<<item.feature.stableIdentity;
                    if(featureIdentities.size()==1){firstKind=item.kind;nominalDiameter=.3;manufacturingDiameter=.3;}
                    provenance<<QStringLiteral("%1 [%2]: nominal/unverified interleaved-finger contact bumps (0.300 mm protrusion, 0.000 mm adjustment); no applicable Verified evidence. Source-owned fit geometry retained.")
                        .arg(item.feature.stableIdentity,orientationName(orientation));
                    continue;
                }
                unmatched<<QStringLiteral("%1 [%2] remained nominal: %3")
                    .arg(item.feature.stableIdentity,orientationName(orientation),correctionReason);
                continue;
            }
            PrintMesh next;
            QString adjustmentDiagnostic;
            bool adjustedSuccessfully=false;
            switch(item.kind){
            case SurfaceKind::Ball:
                adjustedSuccessfully=BallJointSemantic::adjustPrepared(source,adjusted,item.feature,
                    primary->valueMillimetres,&next,&adjustmentDiagnostic);break;
            case SurfaceKind::Socket:
                adjustedSuccessfully=BallSocketSemantic::adjustPrepared(source,adjusted,item.feature,
                    primary->valueMillimetres,&next,&adjustmentDiagnostic);break;
            case SurfaceKind::Clip:
                adjustedSuccessfully=CClipBarReceiverSemantic::adjustPrepared(source,adjusted,item.feature,
                    primary->valueMillimetres,&next,&adjustmentDiagnostic);break;
            case SurfaceKind::Bar:
                adjustedSuccessfully=StandardBarSemantic::adjustPrepared(source,adjusted,item.feature,
                    primary->valueMillimetres,&next,&adjustmentDiagnostic);break;
            case SurfaceKind::HingePin:
                adjustedSuccessfully=PinBarrelHingeSemantic::adjustPrepared(source,adjusted,item.feature,
                    primary->valueMillimetres,&next,&adjustmentDiagnostic);break;
            case SurfaceKind::InterleavedBump:
                adjustedSuccessfully=InterleavedFingerHingeSemantic::adjustPrepared(source,adjusted,item.feature,
                    primary->valueMillimetres,&next,&adjustmentDiagnostic);break;
            case SurfaceKind::ClickArrestor:
                adjustedSuccessfully=ClickHingeSemantic::adjustPrepared(source,adjusted,item.feature,
                    primary->valueMillimetres,&next,&adjustmentDiagnostic);break;
            case SurfaceKind::Stud:
                adjustedSuccessfully=adjustCertifiedStud(source,adjusted,{item.feature,item.owner},
                    primary?primary->valueMillimetres:0.0,height?height->valueMillimetres:0.0,
                    &next,&adjustmentDiagnostic);break;
            }
            if(!adjustedSuccessfully)
                return fail(ManufacturingMeshError::RegenerationFailure,
                    QStringLiteral("Certified source-surface correction failed for %1: %2")
                        .arg(item.feature.stableIdentity,adjustmentDiagnostic));
            if(!accept(std::move(next)))
                return fail(ManufacturingMeshError::SemanticFailure,
                    QStringLiteral("Independent certified source-surface corrections overlap at %1.")
                        .arg(item.feature.stableIdentity));
            if(featureIdentities.isEmpty()){
                firstKind=item.kind;
                nominalDiameter=item.kind==SurfaceKind::InterleavedBump?.3:
                    item.kind==SurfaceKind::ClickArrestor?2.186:item.feature.nominalDiameterMillimetres;
                diameterCorrection=primary?primary->valueMillimetres:0.0;
                manufacturingDiameter=nominalDiameter+diameterCorrection;
            }
            if(item.kind==SurfaceKind::Stud){
                nominalHeight=1.6;
                heightCorrection=height?height->valueMillimetres:0.0;
                manufacturingHeight=nominalHeight+heightCorrection;
            }
            featureIdentities<<item.feature.stableIdentity;
            if(primary){
                semanticContracts<<primary->semanticContractVersion;
                correctionContracts<<primary->correctionContractVersion;
                correctionIdentities<<QStringLiteral("%1=%2").arg(primary->correctionContractVersion)
                    .arg(primary->valueMillimetres,0,'g',17);
            }
            if(height){
                semanticContracts<<height->semanticContractVersion;
                correctionContracts<<height->correctionContractVersion;
                correctionIdentities<<QStringLiteral("%1=%2").arg(height->correctionContractVersion)
                    .arg(height->valueMillimetres,0,'g',17);
            }
            provenance<<QStringLiteral("%1 [%2]: %3 mm + %4 mm = %5 mm%6; %7")
                .arg(item.feature.stableIdentity,orientationName(orientation))
                .arg(item.kind==SurfaceKind::InterleavedBump?.3:
                     item.kind==SurfaceKind::ClickArrestor?2.186:item.feature.nominalDiameterMillimetres,0,'f',3)
                .arg(primary?primary->valueMillimetres:0.0,0,'f',3)
                .arg((item.kind==SurfaceKind::InterleavedBump?.3:
                      item.kind==SurfaceKind::ClickArrestor?2.186:item.feature.nominalDiameterMillimetres)+
                     (primary?primary->valueMillimetres:0.0),0,'f',3)
                .arg(item.kind==SurfaceKind::Stud
                    ?QStringLiteral("; height 1.600 mm + %1 mm = %2 mm")
                        .arg(height?height->valueMillimetres:0.0,0,'f',3)
                        .arg(1.6+(height?height->valueMillimetres:0.0),0,'f',3)
                    :QString())
                .arg(adjustmentDiagnostic);
        }
        if(featureIdentities.isEmpty()){
            if(surfaceFeatures.front().kind==SurfaceKind::Ball)
                unmatched<<QStringLiteral("Ball Joint requires Verified evidence matching its actual print orientation; parallel and perpendicular evidence do not substitute for each other.");
            return fail(ManufacturingMeshError::MissingCorrection,unmatched.join(' '));
        }
        const auto analysis=analyzeSource(adjusted);
        if(!validatePreparedMesh(analysis).ok())
            return fail(ManufacturingMeshError::InvalidResult,
                QStringLiteral("The composed source-surface ManufacturingMesh failed strict validation."));
        semanticContracts.removeDuplicates();correctionContracts.removeDuplicates();
        correctionIdentities.removeDuplicates();
        auto output=std::make_shared<ManufacturingMesh>();
        output->mesh=std::move(adjusted);
        output->analysis=analysis;
        output->partReference=prepared.partReference;
        output->fitProfileIdentity=profile.profileIdentity;
        output->sourceSessionIdentity=profile.sourceSessionIdentity;
        output->featureIdentities=featureIdentities;
        output->featureIdentity=featureIdentities.join('|');
        output->semanticContractVersion=semanticContracts.join('|');
        output->correctionContractVersion=correctionContracts.join('|');
        output->regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();
        output->booleanVersion=featureIdentities.size()==1&&firstKind==SurfaceKind::Ball
            ?QStringLiteral("not-used-ball-joint-source-surface-v1")
            :featureIdentities.size()==1&&firstKind==SurfaceKind::Socket
            ?QStringLiteral("not-used-ball-socket-source-surface-v1")
            :featureIdentities.size()==1&&firstKind==SurfaceKind::Clip
            ?QStringLiteral("not-used-c-clip-source-surface-v1")
            :featureIdentities.size()==1&&firstKind==SurfaceKind::Bar
            ?QStringLiteral("not-used-standard-bar-source-surface-v1")
            :featureIdentities.size()==1&&firstKind==SurfaceKind::HingePin
            ?QStringLiteral("not-used-hinge-pin-source-surface-v1")
            :featureIdentities.size()==1&&firstKind==SurfaceKind::InterleavedBump
            ?QStringLiteral("not-used-interleaved-finger-source-surface-v1")
            :featureIdentities.size()==1&&firstKind==SurfaceKind::ClickArrestor
            ?QStringLiteral("not-used-click-arrestor-source-surface-v1")
            :QStringLiteral("not-used-certified-source-surface-composition-v1");
        output->nominalDiameterMillimetres=nominalDiameter;
        output->diameterCorrectionMillimetres=diameterCorrection;
        output->manufacturingDiameterMillimetres=manufacturingDiameter;
        output->nominalHeightMillimetres=nominalHeight;
        output->heightCorrectionMillimetres=heightCorrection;
        output->manufacturingHeightMillimetres=manufacturingHeight;
        output->nominalPreparationIdentity=prepared.partReference+'|'+prepared.ldrawIdentity+'|'+
            prepared.preparationProfileVersion+'|'+prepared.mcutVersion;
        output->provenance<<QStringLiteral("Nominal PreparedMesh: %1").arg(output->nominalPreparationIdentity)
            <<(nominalHingeProfile?QStringLiteral("No Verified Fit Profile: certified hinge uses nominal/unverified contact bumps."):
                QStringLiteral("Verified Fit Profile: %1").arg(profile.profileIdentity))
            <<QStringLiteral("Print Orientation: %1").arg(printOrientation.summary())
            <<provenance<<unmatched;
        const QByteArray identity=(output->nominalPreparationIdentity+'|'+profile.profileIdentity+'|'+
            profile.processFingerprint+'|'+printOrientation.summary()+'|'+featureIdentities.join('|')+'|'+
            correctionIdentities.join('|')+'|'+output->regeneratorAlgorithmVersion+'|'+
            output->booleanVersion).toUtf8();
        output->identity=QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());
        ManufacturingMeshResult result;
        result.error=ManufacturingMeshError::None;
        result.manufacturingMesh=output;
        result.diagnostic=QStringLiteral("%1 composed %2 certified source-surface feature(s) after Print Orientation %3. Source and nominal PreparedMesh were not modified. %4")
            .arg(nominalHingeProfile?QStringLiteral("Nominal/unverified interleaved-finger fallback"):
                 QStringLiteral("Verified profile %1").arg(profile.name))
            .arg(featureIdentities.size()).arg(printOrientation.summary(),provenance.join(' '));
        return result;
    }
    auto semantic=m_builder?m_builder(source):LDrawSemanticOperandBuilder::build(source);
    const auto axleFeatures=TechnicAxleSemantic::recognizeAxles(source);const auto axleHoleFeatures=TechnicAxleSemantic::recognizeAxleHoles(source);const auto roundPassageFeatures=RoundTechnicPassageSemantic::recognize(source);const auto receivingPostFeatures=StudReceivingPostSemantic::recognize(source);
    if(!semantic.ok()&&(!axleFeatures.isEmpty()||!axleHoleFeatures.isEmpty()||!roundPassageFeatures.isEmpty()||!receivingPostFeatures.isEmpty())){
        auto backend=m_factory();if(!backend)return fail(ManufacturingMeshError::BooleanFailure,"No Boolean composition service is available.");PrintMesh accumulated=prepared.mesh;QStringList featureIdentities,semanticIdentities,contractIdentities,correctionIdentity,provenance;int skippedFeatures=0;double nominalDimension=0,appliedCorrection=0,manufacturingDimension=0;
        auto apply=[&](const FunctionalFeature&feature,bool female)->ManufacturingMeshResult{const auto orientation=transformedOrientation(feature,printOrientation);QString featureReason;const auto corrections=compatibleCorrections(profile,orientation,&featureReason);FunctionalOperandRegenerationResult regenerated;const FitProfileCorrection*correction=nullptr;if(!female){correction=corrections.technicAxleTipToTip;if(correction&&feature.evidenceContract==correction->semanticContractVersion)regenerated=FunctionalOperandRegenerator::regenerateTechnicAxleProfile(feature,{correction->valueMillimetres});}else{correction=corrections.technicAxleHoleArmWidth;if(correction){auto armFeature=feature;armFeature.constructionRecipe=QStringLiteral("technic-axle-hole-arm-width-clearance-v2");armFeature.evidenceContract=QStringLiteral("official-ldraw-axlehole-arm-width-clearance-v2");regenerated=FunctionalOperandRegenerator::regenerateTechnicAxleHoleArmWidth(armFeature,{correction->valueMillimetres,correction->fixedTipToTipCorrectionMillimetres});}}
            const auto transformedAxis=printOrientation.map(feature.frame.axis);const QString evidence=QStringLiteral("Part axis %1 -> build axis %2 (%3)").arg(axisName(feature.frame.axis),axisName(transformedAxis),orientationName(orientation));if(correction&&!regenerated.ok())return fail(ManufacturingMeshError::RegenerationFailure,regenerated.diagnostic);if(!correction){++skippedFeatures;provenance<<QStringLiteral("Feature %1 [%2] remained nominal: %3").arg(feature.stableIdentity,evidence,featureReason);return {};}
            const auto operation=female?backend->subtract(accumulated,regenerated.mesh):backend->unite(accumulated,regenerated.mesh);if(!operation.ok())return fail(ManufacturingMeshError::BooleanFailure,QString::fromStdString(operation.message));accumulated=operation.mesh;featureIdentities<<feature.stableIdentity;semanticIdentities<<correction->semanticContractVersion;contractIdentities<<correction->correctionContractVersion;correctionIdentity<<QStringLiteral("%1=%2").arg(correction->correctionContractVersion).arg(correction->valueMillimetres,0,'g',17);appliedCorrection=correction->valueMillimetres;if(female){nominalDimension=feature.protectedCrossArmHalfWidthMillimetres*2.0;manufacturingDimension=nominalDimension+appliedCorrection;provenance<<QStringLiteral("Feature %1 [%2]: axle-hole arm opening %3 mm + %4 mm = %5 mm; tip-to-tip fixed at %6 mm and engagement depth %7 mm retained.").arg(feature.stableIdentity,evidence).arg(nominalDimension,0,'f',3).arg(appliedCorrection,0,'f',3).arg(manufacturingDimension,0,'f',3).arg(feature.nominalDiameterMillimetres+correction->fixedTipToTipCorrectionMillimetres,0,'f',3).arg(feature.nominalEngagementExtentMillimetres,0,'f',3);}else{nominalDimension=feature.nominalDiameterMillimetres;manufacturingDimension=nominalDimension+appliedCorrection;provenance<<QStringLiteral("Feature %1 [%2]: axle tip-to-tip %3 mm + %4 mm = %5 mm; cross arm proportions, shoulders, topology, and engagement length retained.").arg(feature.stableIdentity,evidence).arg(nominalDimension,0,'f',3).arg(appliedCorrection,0,'f',3).arg(manufacturingDimension,0,'f',3);}return {};};
        auto applyRoundPassage=[&](const FunctionalFeature&feature)->ManufacturingMeshResult{const auto orientation=transformedOrientation(feature,printOrientation);QString featureReason;const auto corrections=compatibleCorrections(profile,orientation,&featureReason);const auto*correction=corrections.femaleDiameter;const auto transformedAxis=printOrientation.map(feature.frame.axis);const QString evidence=QStringLiteral("Part axis %1 -> build axis %2 (%3)").arg(axisName(feature.frame.axis),axisName(transformedAxis),orientationName(orientation));if(!correction||feature.evidenceContract!=correction->semanticContractVersion){++skippedFeatures;provenance<<QStringLiteral("Feature %1 [%2] remained nominal: %3").arg(feature.stableIdentity,evidence,featureReason);return {};}const auto regenerated=FunctionalOperandRegenerator::regenerate(feature,{correction->valueMillimetres});if(!regenerated.ok())return fail(ManufacturingMeshError::RegenerationFailure,regenerated.diagnostic);const auto operation=backend->subtract(accumulated,regenerated.mesh);if(!operation.ok())return fail(ManufacturingMeshError::BooleanFailure,QString::fromStdString(operation.message));accumulated=operation.mesh;featureIdentities<<feature.stableIdentity;semanticIdentities<<correction->semanticContractVersion;contractIdentities<<correction->correctionContractVersion;correctionIdentity<<QStringLiteral("%1=%2").arg(correction->correctionContractVersion).arg(correction->valueMillimetres,0,'g',17);nominalDimension=feature.nominalDiameterMillimetres;appliedCorrection=correction->valueMillimetres;manufacturingDimension=regenerated.resultingGoverningRadiusMillimetres*2.0;provenance<<QStringLiteral("Feature %1 [%2]: round Technic passage diameter %3 mm + %4 mm = %5 mm; entrance profile, engagement depth, surrounding geometry, and neighboring functional features retained.").arg(feature.stableIdentity,evidence).arg(nominalDimension,0,'f',3).arg(appliedCorrection,0,'f',3).arg(manufacturingDimension,0,'f',3);return {};};
        auto applyReceivingPost=[&](const FunctionalFeature&feature)->ManufacturingMeshResult{const auto orientation=transformedOrientation(feature,printOrientation);QString featureReason;const auto corrections=compatibleCorrections(profile,orientation,&featureReason);const auto*correction=corrections.receivingPostDiameter;const auto transformedAxis=printOrientation.map(feature.frame.axis);const QString evidence=QStringLiteral("Part axis %1 -> build axis %2 (%3)").arg(axisName(feature.frame.axis),axisName(transformedAxis),orientationName(orientation));if(!correction||feature.evidenceContract!=correction->semanticContractVersion){++skippedFeatures;provenance<<QStringLiteral("Feature %1 [%2] remained nominal: %3").arg(feature.stableIdentity,evidence,featureReason);return {};}const auto regenerated=FunctionalOperandRegenerator::regenerateReceivingPost(feature,{correction->valueMillimetres});if(!regenerated.ok())return fail(ManufacturingMeshError::RegenerationFailure,regenerated.diagnostic);const auto operation=backend->unite(accumulated,regenerated.mesh);if(!operation.ok())return fail(ManufacturingMeshError::BooleanFailure,QString::fromStdString(operation.message));accumulated=operation.mesh;featureIdentities<<feature.stableIdentity;semanticIdentities<<correction->semanticContractVersion;contractIdentities<<correction->correctionContractVersion;correctionIdentity<<QStringLiteral("%1=%2").arg(correction->correctionContractVersion).arg(correction->valueMillimetres,0,'g',17);nominalDimension=feature.nominalDiameterMillimetres;appliedCorrection=correction->valueMillimetres;manufacturingDimension=regenerated.resultingGoverningRadiusMillimetres*2.0;provenance<<QStringLiteral("Feature %1 [%2]: PostWallCell post OD %3 mm + %4 mm = %5 mm; walls, height, seating, pitch, and external dimensions retained.").arg(feature.stableIdentity,evidence).arg(nominalDimension,0,'f',3).arg(appliedCorrection,0,'f',3).arg(manufacturingDimension,0,'f',3);return {};};
        for(const auto&feature:axleFeatures){const auto failure=apply(feature,false);if(failure.error!=ManufacturingMeshError::InvalidInput)return failure;}for(const auto&feature:axleHoleFeatures){const auto failure=apply(feature,true);if(failure.error!=ManufacturingMeshError::InvalidInput)return failure;}for(const auto&feature:roundPassageFeatures){const auto failure=applyRoundPassage(feature);if(failure.error!=ManufacturingMeshError::InvalidInput)return failure;}for(const auto&feature:receivingPostFeatures){const auto failure=applyReceivingPost(feature);if(failure.error!=ManufacturingMeshError::InvalidInput)return failure;}
        if(!featureIdentities.isEmpty()){const auto analysis=analyzeSource(accumulated);if(!validatePreparedMesh(analysis).ok())return fail(ManufacturingMeshError::InvalidResult,"The mixed-feature ManufacturingMesh failed strict validation.");semanticIdentities.removeDuplicates();contractIdentities.removeDuplicates();correctionIdentity.removeDuplicates();auto output=std::make_shared<ManufacturingMesh>();output->mesh=std::move(accumulated);output->analysis=analysis;output->partReference=prepared.partReference;output->fitProfileIdentity=profile.profileIdentity;output->sourceSessionIdentity=profile.sourceSessionIdentity;output->featureIdentities=featureIdentities;output->featureIdentity=featureIdentities.join('|');output->semanticContractVersion=semanticIdentities.join('|');output->correctionContractVersion=contractIdentities.join('|');output->regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();output->booleanVersion=backend->versionIdentity();output->nominalDiameterMillimetres=nominalDimension;output->diameterCorrectionMillimetres=appliedCorrection;output->manufacturingDiameterMillimetres=manufacturingDimension;output->nominalPreparationIdentity=prepared.partReference+'|'+prepared.ldrawIdentity+'|'+prepared.preparationProfileVersion+'|'+prepared.mcutVersion;output->provenance<<QStringLiteral("Nominal PreparedMesh: %1").arg(output->nominalPreparationIdentity)<<QStringLiteral("Verified Fit Profile: %1").arg(profile.profileIdentity)<<QStringLiteral("Print Orientation: %1").arg(printOrientation.summary())<<provenance;const QByteArray identity=(output->nominalPreparationIdentity+'|'+profile.profileIdentity+'|'+profile.processFingerprint+'|'+printOrientation.summary()+'|'+featureIdentities.join('|')+'|'+correctionIdentity.join('|')+'|'+output->regeneratorAlgorithmVersion+'|'+output->booleanVersion).toUtf8();output->identity=QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());ManufacturingMeshResult result;result.error=ManufacturingMeshError::None;result.manufacturingMesh=output;result.diagnostic=QStringLiteral("Separate profile-driven ManufacturingMesh generated from %1 Verified mixed functional feature(s); %2 recognized feature(s) remained nominal. Source and nominal PreparedMesh were not modified.").arg(featureIdentities.size()).arg(skippedFeatures);return result;}return fail(ManufacturingMeshError::MissingCorrection,provenance.join(' '));
    }
    if(!m_builder&&!semantic.ok()){const auto pinFeatures=FrictionlessTechnicPinSemantic::recognize(source);QVector<QPair<FunctionalFeature,const FitProfileCorrection*>> verifiedZeroPins;QStringList pinProvenance;int unmatchedPins=0;for(const auto&feature:pinFeatures){const auto orientation=transformedOrientation(feature,printOrientation);QString featureReason;const auto corrections=compatibleCorrections(profile,orientation,&featureReason);const auto*correction=corrections.frictionlessPinDiameter;if(correction&&feature.evidenceContract==correction->semanticContractVersion){if(std::abs(correction->valueMillimetres)<=1e-12)verifiedZeroPins.push_back({feature,correction});else pinProvenance<<QStringLiteral("Feature %1 matched a non-zero frictionless-pin correction that requires geometric regeneration.").arg(feature.stableIdentity);}else{++unmatchedPins;pinProvenance<<QStringLiteral("Feature %1 remained nominal: %2").arg(feature.stableIdentity,featureReason);}}
        if(!verifiedZeroPins.isEmpty()){const auto analysis=analyzeSource(prepared.mesh);if(!validatePreparedMesh(analysis).ok())return fail(ManufacturingMeshError::InvalidResult,"The nominal PreparedMesh for Verified-zero application failed strict validation.");auto output=std::make_shared<ManufacturingMesh>();output->mesh=prepared.mesh;output->analysis=analysis;output->partReference=prepared.partReference;output->fitProfileIdentity=profile.profileIdentity;output->sourceSessionIdentity=profile.sourceSessionIdentity;output->regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();output->booleanVersion=QStringLiteral("verified-zero-identity-v1");output->nominalDiameterMillimetres=verifiedZeroPins.front().first.nominalDiameterMillimetres;output->diameterCorrectionMillimetres=0.0;output->manufacturingDiameterMillimetres=output->nominalDiameterMillimetres;QStringList correctionIdentity;for(const auto&match:verifiedZeroPins){const auto&feature=match.first;const auto*correction=match.second;output->featureIdentities<<feature.stableIdentity;output->semanticContractVersion=correction->semanticContractVersion;output->correctionContractVersion=correction->correctionContractVersion;correctionIdentity<<QStringLiteral("%1=0").arg(correction->correctionContractVersion);const auto transformedAxis=printOrientation.map(feature.frame.axis);output->provenance<<QStringLiteral("Feature %1 [Part axis %2 -> build axis %3 (%4)]: Verified frictionless-pin envelope %5 mm + 0.000 mm = %6 mm; nominal geometry intentionally retained, including bore, slots, entrance profile, engagement length, and attachment geometry.").arg(feature.stableIdentity,axisName(feature.frame.axis),axisName(transformedAxis),orientationName(transformedOrientation(feature,printOrientation))).arg(feature.nominalDiameterMillimetres,0,'f',3).arg(feature.nominalDiameterMillimetres,0,'f',3);}output->featureIdentity=output->featureIdentities.join('|');output->nominalPreparationIdentity=prepared.partReference+'|'+prepared.ldrawIdentity+'|'+prepared.preparationProfileVersion+'|'+prepared.mcutVersion;output->provenance.prepend(QStringLiteral("Verified Fit Profile: %1").arg(profile.profileIdentity));output->provenance.prepend(QStringLiteral("Nominal PreparedMesh: %1").arg(output->nominalPreparationIdentity));output->provenance.prepend(QStringLiteral("Print Orientation: %1").arg(printOrientation.summary()));output->provenance<<pinProvenance;const QByteArray identity=(output->nominalPreparationIdentity+'|'+profile.profileIdentity+'|'+profile.processFingerprint+'|'+printOrientation.summary()+'|'+output->featureIdentity+'|'+correctionIdentity.join('|')+'|'+output->regeneratorAlgorithmVersion+'|'+output->booleanVersion).toUtf8();output->identity=QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());ManufacturingMeshResult result;result.error=ManufacturingMeshError::None;result.manufacturingMesh=output;result.diagnostic=QStringLiteral("Separate profile-driven ManufacturingMesh generated from %1 Verified-zero frictionless-pin feature(s); the validated nominal 6.40 mm geometry was intentionally retained and %2 recognized feature(s) remained nominal. Source and nominal PreparedMesh were not modified.").arg(verifiedZeroPins.size()).arg(unmatchedPins);return result;}
        const auto frictionFeatures=FrictionTechnicPinSemantic::recognize(source);QVector<QPair<FunctionalFeature,const FitProfileCorrection*>> verifiedFrictionPins;QStringList frictionProvenance;int unmatchedFrictionPins=0;
        for(const auto&feature:frictionFeatures){const auto orientation=transformedOrientation(feature,printOrientation);QString featureReason;const auto corrections=compatibleCorrections(profile,orientation,&featureReason);const auto*correction=corrections.frictionPinDiameter;if(correction&&feature.evidenceContract==correction->semanticContractVersion)verifiedFrictionPins.push_back({feature,correction});else{++unmatchedFrictionPins;frictionProvenance<<QStringLiteral("Feature %1 remained nominal: %2").arg(feature.stableIdentity,featureReason);}}
        if(!verifiedFrictionPins.isEmpty()){PrintMesh accumulated=prepared.mesh;QStringList featureIdentities,semanticIdentities,contractIdentities,correctionIdentity,provenance;double nominalDiameter=0.0,diameterCorrection=0.0,manufacturingDiameter=0.0;
            for(const auto&match:verifiedFrictionPins){const auto&feature=match.first;const auto*correction=match.second;const double coreRadius=feature.radialProfile.front().radiusMillimetres;double ridgeStart=feature.nominalAxialExtentMillimetres,ridgeEnd=0.0;for(const auto&section:feature.radialProfile)if(std::abs(section.radiusMillimetres-feature.nominalRadiusMillimetres)<=1e-9){ridgeStart=std::min(ridgeStart,section.axialPositionMillimetres);ridgeEnd=std::max(ridgeEnd,section.axialPositionMillimetres);}int adjustedVertices=0;for(auto&vertex:accumulated.vertices){const double dx=vertex.x-feature.frame.origin.x,dy=vertex.y-feature.frame.origin.y,dz=vertex.z-feature.frame.origin.z,axial=dx*feature.frame.axis.x+dy*feature.frame.axis.y+dz*feature.frame.axis.z;if(axial<ridgeStart-1e-6||axial>ridgeEnd+1e-6)continue;const double rx=dx-feature.frame.axis.x*axial,ry=dy-feature.frame.axis.y*axial,rz=dz-feature.frame.axis.z*axial,radius=std::sqrt(rx*rx+ry*ry+rz*rz);if(radius<=coreRadius+1e-6||radius>feature.nominalRadiusMillimetres+0.25)continue;const double weight=std::clamp((radius-coreRadius)/(feature.nominalRadiusMillimetres-coreRadius),0.0,1.0),delta=correction->valueMillimetres*.5*weight;if(std::abs(delta)<=1e-15)continue;vertex.x+=rx/radius*delta;vertex.y+=ry/radius*delta;vertex.z+=rz/radius*delta;++adjustedVertices;}if(adjustedVertices==0&&std::abs(correction->valueMillimetres)>1e-12)return fail(ManufacturingMeshError::RegenerationFailure,QStringLiteral("The recognized friction ridge has no matching PreparedMesh surface vertices."));nominalDiameter=feature.nominalDiameterMillimetres;diameterCorrection=correction->valueMillimetres;manufacturingDiameter=nominalDiameter+diameterCorrection;featureIdentities<<feature.stableIdentity;semanticIdentities<<correction->semanticContractVersion;contractIdentities<<correction->correctionContractVersion;correctionIdentity<<QStringLiteral("%1=%2").arg(correction->correctionContractVersion).arg(correction->valueMillimetres,0,'g',17);const auto transformedAxis=printOrientation.map(feature.frame.axis);provenance<<QStringLiteral("Feature %1 [Part axis %2 -> build axis %3 (%4)]: friction-ridge envelope %5 mm + %6 mm = %7 mm; %8 PreparedMesh ridge vertices adjusted while the 4.800 mm compliant core, 3.200 mm bore, slots, axial transitions, entrance geometry, and engagement length remained unchanged.").arg(feature.stableIdentity,axisName(feature.frame.axis),axisName(transformedAxis),orientationName(transformedOrientation(feature,printOrientation))).arg(nominalDiameter,0,'f',3).arg(diameterCorrection,0,'f',3).arg(manufacturingDiameter,0,'f',3).arg(adjustedVertices);}
            const auto analysis=analyzeSource(accumulated);if(!validatePreparedMesh(analysis).ok())return fail(ManufacturingMeshError::InvalidResult,"The friction-pin ManufacturingMesh failed strict validation.");semanticIdentities.removeDuplicates();contractIdentities.removeDuplicates();correctionIdentity.removeDuplicates();auto output=std::make_shared<ManufacturingMesh>();output->mesh=std::move(accumulated);output->analysis=analysis;output->partReference=prepared.partReference;output->fitProfileIdentity=profile.profileIdentity;output->sourceSessionIdentity=profile.sourceSessionIdentity;output->featureIdentities=featureIdentities;output->featureIdentity=featureIdentities.join('|');output->semanticContractVersion=semanticIdentities.join('|');output->correctionContractVersion=contractIdentities.join('|');output->regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();output->booleanVersion=QStringLiteral("localized-friction-ridge-deformation-v1");output->nominalDiameterMillimetres=nominalDiameter;output->diameterCorrectionMillimetres=diameterCorrection;output->manufacturingDiameterMillimetres=manufacturingDiameter;output->nominalPreparationIdentity=prepared.partReference+'|'+prepared.ldrawIdentity+'|'+prepared.preparationProfileVersion+'|'+prepared.mcutVersion;output->provenance<<QStringLiteral("Nominal PreparedMesh: %1").arg(output->nominalPreparationIdentity)<<QStringLiteral("Verified Fit Profile: %1").arg(profile.profileIdentity)<<QStringLiteral("Print Orientation: %1").arg(printOrientation.summary())<<provenance<<frictionProvenance;const QByteArray identity=(output->nominalPreparationIdentity+'|'+profile.profileIdentity+'|'+profile.processFingerprint+'|'+printOrientation.summary()+'|'+featureIdentities.join('|')+'|'+correctionIdentity.join('|')+'|'+output->regeneratorAlgorithmVersion+'|'+output->booleanVersion).toUtf8();output->identity=QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());ManufacturingMeshResult result;result.error=ManufacturingMeshError::None;result.manufacturingMesh=output;result.diagnostic=QStringLiteral("Separate profile-driven ManufacturingMesh generated from %1 Verified friction-pin feature(s); only the ridge envelope changed and %2 recognized feature(s) remained nominal. Source and nominal PreparedMesh were not modified.").arg(featureIdentities.size()).arg(unmatchedFrictionPins);return result;}
        if(!frictionFeatures.isEmpty())return fail(ManufacturingMeshError::MissingCorrection,frictionProvenance.isEmpty()?QStringLiteral("No recognized friction-pin feature matches the selected Verified correction and Print Orientation."):frictionProvenance.join(' '));
        if(!pinFeatures.isEmpty())return fail(ManufacturingMeshError::MissingCorrection,pinProvenance.isEmpty()?QStringLiteral("No recognized frictionless-pin feature matches the selected Verified correction and Print Orientation."):pinProvenance.join(' '));
    }
    if(!semantic.ok())return fail(ManufacturingMeshError::SemanticFailure,semantic.diagnostics.join(' '));
    QVector<SemanticOperand> operands=semantic.operands;auto alreadyRetained=[&](const FunctionalFeature&candidate){for(const auto&operand:operands)for(const auto&feature:operand.functionalFeatures){if(feature.stableIdentity==candidate.stableIdentity)return true;if(feature.family==candidate.family&&feature.role==candidate.role){const Point delta{feature.frame.origin.x-candidate.frame.origin.x,feature.frame.origin.y-candidate.frame.origin.y,feature.frame.origin.z-candidate.frame.origin.z};const double originDistance=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);const double axisAlignment=std::abs(feature.frame.axis.x*candidate.frame.axis.x+feature.frame.axis.y*candidate.frame.axis.y+feature.frame.axis.z*candidate.frame.axis.z);if(originDistance<=1e-6&&axisAlignment>=1.0-1e-6)return true;}}return false;};auto appendAxleOperand=[&](const FunctionalFeature&feature,SemanticRole role){if(alreadyRetained(feature))return;const auto nominal=FunctionalOperandRegenerator::regenerateTechnicAxleProfile(feature,{0});if(!nominal.ok())return;SemanticOperand operand;operand.role=role;operand.closedMesh=nominal.mesh;operand.analysis=nominal.analysis;operand.functionalFeatures.push_back(feature);operand.sourceFiles={feature.stableIdentity};operands.push_back(operand);};auto appendRoundPassageOperand=[&](const FunctionalFeature&feature){if(alreadyRetained(feature))return;const auto nominal=FunctionalOperandRegenerator::regenerate(feature,{0});if(!nominal.ok())return;SemanticOperand operand;operand.role=SemanticRole::SubtractivePassage;operand.closedMesh=nominal.mesh;operand.analysis=nominal.analysis;operand.functionalFeatures.push_back(feature);operand.sourceFiles={feature.stableIdentity};operands.push_back(operand);};auto appendReceivingPostOperand=[&](const FunctionalFeature&feature){if(alreadyRetained(feature))return;const auto nominal=FunctionalOperandRegenerator::regenerateReceivingPost(feature,{0});if(!nominal.ok())return;SemanticOperand operand;operand.role=SemanticRole::AdditiveAttachment;operand.closedMesh=nominal.mesh;operand.analysis=nominal.analysis;operand.functionalFeatures.push_back(feature);operand.sourceFiles={feature.stableIdentity};operands.push_back(operand);};for(const auto&feature:axleFeatures)appendAxleOperand(feature,SemanticRole::AdditiveAttachment);for(const auto&feature:axleHoleFeatures)appendAxleOperand(feature,SemanticRole::SubtractivePassage);for(const auto&feature:roundPassageFeatures)appendRoundPassageOperand(feature);for(const auto&feature:receivingPostFeatures)appendReceivingPostOperand(feature);std::stable_sort(operands.begin(),operands.end(),[](const auto&a,const auto&b){const int ao=order(a.role),bo=order(b.role);return ao==bo?operandIdentity(a)<operandIdentity(b):ao<bo;});
    bool replaced=false;int skipped=0;QStringList featureIdentities,semanticIdentities,contractIdentities,correctionIdentity,provenance;double nominalDiameter=0,diameterCorrection=0,manufacturingDiameter=0,nominalHeight=0,heightCorrection=0,manufacturingHeight=0;
    provenance<<QStringLiteral("Print Orientation: %1").arg(printOrientation.summary());
    for(auto& operand:operands){
        if(operand.role!=SemanticRole::PrimaryBody)continue;
        QVector<FunctionalFeature> retained;
        for(const auto& feature:operand.functionalFeatures){
            if(feature.constructionRecipe!=QStringLiteral("stud-receiving-wall-pocket-square-v1")){
                retained.push_back(feature);continue;
            }
            const auto orientation=transformedOrientation(feature,printOrientation);
            QString featureReason;
            const auto corrections=compatibleCorrections(profile,orientation,&featureReason);
            const auto* correction=wallPocketCorrection(profile,feature,orientation);
            if(!correction){
                ++skipped;
                provenance<<QStringLiteral("WallPocket %1 remained nominal after Print Orientation %2: %3")
                    .arg(feature.stableIdentity,printOrientation.summary(),featureReason);
                continue;
            }
            PrintMesh adjusted;QString diagnostic;
            if(!StudReceivingWallPocketSemantic::adjustPrepared(operand.closedMesh,feature,
                    correction->valueMillimetres,&adjusted,&diagnostic))
                return fail(ManufacturingMeshError::RegenerationFailure,diagnostic);
            operand.closedMesh=std::move(adjusted);
            operand.analysis=analyzeSource(operand.closedMesh);
            nominalDiameter=feature.nominalDiameterMillimetres;
            diameterCorrection=correction->valueMillimetres;
            manufacturingDiameter=nominalDiameter+diameterCorrection;
            featureIdentities<<feature.stableIdentity;
            semanticIdentities<<correction->semanticContractVersion;
            contractIdentities<<correction->correctionContractVersion;
            correctionIdentity<<QStringLiteral("%1=%2").arg(correction->correctionContractVersion)
                .arg(diameterCorrection,0,'g',17);
            provenance<<QStringLiteral("WallPocket %1: certified square opening width %2 mm + %3 mm = %4 mm; "
                                       "four inner walls changed, floor depth and exterior shell retained.")
                .arg(feature.stableIdentity).arg(nominalDiameter,0,'f',3)
                .arg(diameterCorrection,0,'f',3).arg(manufacturingDiameter,0,'f',3);
            if (correction->semanticContractVersion != feature.evidenceContract)
                provenance<<QStringLiteral("Shared shallow WallPocket opening calibration %1 applied to distinct production depth contract %2; pocket depth remains nominal.")
                    .arg(correction->semanticContractVersion,feature.evidenceContract);
            replaced=true;
        }
        operand.functionalFeatures=retained;
    }
    for(auto& operand:operands){
        if(operand.role!=SemanticRole::PrimaryBody)continue;
        QVector<FunctionalFeature> retained;
        for(const auto& feature:operand.functionalFeatures){
            if(feature.constructionRecipe!=QStringLiteral("stud-receiving-antistud-bore-v1")){
                retained.push_back(feature);continue;
            }
            const auto orientation=transformedOrientation(feature,printOrientation);
            const auto corrections=compatibleCorrections(profile,orientation);
            const auto* correction=corrections.receivingAntiStudBoreDiameter;
            if(!correction || correction->semanticContractVersion!=feature.evidenceContract){
                ++skipped;
                provenance<<QStringLiteral("AntiStudBore %1 remained nominal: no applicable Verified bore correction for %2.")
                    .arg(feature.stableIdentity,printOrientation.summary());
                continue;
            }
            PrintMesh adjusted;QString diagnostic;
            if(!StudReceivingAntiStudSemantic::adjustPrepared(source,operand.closedMesh,feature,
                    correction->valueMillimetres,&adjusted,&diagnostic))
                return fail(ManufacturingMeshError::RegenerationFailure,diagnostic);
            operand.closedMesh=std::move(adjusted);
            operand.analysis=analyzeSource(operand.closedMesh);
            nominalDiameter=feature.nominalDiameterMillimetres;
            diameterCorrection=correction->valueMillimetres;
            manufacturingDiameter=nominalDiameter+diameterCorrection;
            featureIdentities<<feature.stableIdentity;
            semanticIdentities<<correction->semanticContractVersion;
            contractIdentities<<correction->correctionContractVersion;
            correctionIdentity<<QStringLiteral("%1=%2").arg(correction->correctionContractVersion)
                .arg(diameterCorrection,0,'g',17);
            provenance<<QStringLiteral("AntiStudBore %1: certified centered bore %2 mm + %3 mm = %4 mm; source-owned inner cylindrical wall adjusted, exterior retained.")
                .arg(feature.stableIdentity).arg(nominalDiameter,0,'f',3)
                .arg(diameterCorrection,0,'f',3).arg(manufacturingDiameter,0,'f',3);
            replaced=true;
        }
        operand.functionalFeatures=retained;
    }
    for(auto&operand:operands){QVector<FunctionalFeature>retained;for(const auto&feature:operand.functionalFeatures){const auto orientation=transformedOrientation(feature,printOrientation);QString featureReason;const auto corrections=compatibleCorrections(profile,orientation,&featureReason);if(feature.family==FunctionalInterfaceFamily::StudReceivingClutch&&feature.role==FunctionalInterfaceRole::Female&&corrections.receivingPostDiameter&&feature.evidenceContract==corrections.receivingPostDiameter->semanticContractVersion){const auto regenerated=FunctionalOperandRegenerator::regenerateReceivingPost(feature,{corrections.receivingPostDiameter->valueMillimetres});if(!regenerated.ok())return fail(ManufacturingMeshError::RegenerationFailure,regenerated.diagnostic);operand.closedMesh=regenerated.mesh;operand.analysis=regenerated.analysis;diameterCorrection=corrections.receivingPostDiameter->valueMillimetres;nominalDiameter=feature.nominalDiameterMillimetres;manufacturingDiameter=regenerated.resultingGoverningRadiusMillimetres*2.0;featureIdentities<<feature.stableIdentity;semanticIdentities<<corrections.receivingPostDiameter->semanticContractVersion;contractIdentities<<corrections.receivingPostDiameter->correctionContractVersion;correctionIdentity<<QStringLiteral("%1=%2").arg(corrections.receivingPostDiameter->correctionContractVersion).arg(diameterCorrection,0,'g',17);const auto transformedAxis=printOrientation.map(feature.frame.axis);provenance<<QStringLiteral("Feature %1 [Part axis %2 -> build axis %3 (%4)]: PostWallCell post OD %5 mm + %6 mm = %7 mm; post height, seating depth, wall positions, stud pitch, external dimensions, and unrelated underside geometry retained.").arg(feature.stableIdentity,axisName(feature.frame.axis),axisName(transformedAxis),orientationName(orientation)).arg(nominalDiameter,0,'f',3).arg(diameterCorrection,0,'f',3).arg(manufacturingDiameter,0,'f',3);replaced=true;}else retained.push_back(feature);}operand.functionalFeatures=retained;}
    for(auto&operand:operands){QVector<FunctionalFeature>retained;for(const auto&feature:operand.functionalFeatures){const auto transformedAxis=printOrientation.map(feature.frame.axis);const auto orientation=transformedOrientation(feature,printOrientation);QString featureReason;const auto corrections=compatibleCorrections(profile,orientation,&featureReason);if(feature.family==FunctionalInterfaceFamily::FrictionTechnicPin&&feature.role==FunctionalInterfaceRole::Male&&corrections.frictionPinDiameter&&feature.evidenceContract==corrections.frictionPinDiameter->semanticContractVersion){const auto regenerated=FunctionalOperandRegenerator::regenerateFrictionPin(feature,{corrections.frictionPinDiameter->valueMillimetres});if(!regenerated.ok())return fail(ManufacturingMeshError::RegenerationFailure,regenerated.diagnostic);operand.closedMesh=regenerated.mesh;operand.analysis=regenerated.analysis;diameterCorrection=corrections.frictionPinDiameter->valueMillimetres;nominalDiameter=feature.nominalDiameterMillimetres;manufacturingDiameter=regenerated.resultingGoverningRadiusMillimetres*2.0;featureIdentities<<feature.stableIdentity;semanticIdentities<<corrections.frictionPinDiameter->semanticContractVersion;contractIdentities<<corrections.frictionPinDiameter->correctionContractVersion;correctionIdentity<<QStringLiteral("%1=%2").arg(corrections.frictionPinDiameter->correctionContractVersion).arg(diameterCorrection,0,'g',17);provenance<<QStringLiteral("Feature %1 [Part axis %2 -> build axis %3 (%4)]: friction-ridge envelope %5 mm + %6 mm = %7 mm; protected 4.800 mm compliant core, 3.200 mm bore, slots, axial transitions, entrance geometry, and engagement length retained.").arg(feature.stableIdentity,axisName(feature.frame.axis),axisName(transformedAxis),orientationName(orientation)).arg(nominalDiameter,0,'f',3).arg(diameterCorrection,0,'f',3).arg(manufacturingDiameter,0,'f',3);replaced=true;}else retained.push_back(feature);}operand.functionalFeatures=retained;}
    for(auto&operand:operands){for(const auto&feature:operand.functionalFeatures){const auto transformedAxis=printOrientation.map(feature.frame.axis);const auto orientation=transformedOrientation(feature,printOrientation);QString featureReason;const auto corrections=compatibleCorrections(profile,orientation,&featureReason);const QString orientationEvidence=QStringLiteral("Part axis %1 -> build axis %2 (%3)").arg(axisName(feature.frame.axis),axisName(transformedAxis),orientationName(orientation));FunctionalOperandRegenerationResult regenerated;if(feature.family==FunctionalInterfaceFamily::RoundTechnicPassage&&feature.role==FunctionalInterfaceRole::Female&&corrections.femaleDiameter&&feature.evidenceContract==corrections.femaleDiameter->semanticContractVersion){regenerated=FunctionalOperandRegenerator::regenerate(feature,{corrections.femaleDiameter->valueMillimetres});if(!regenerated.ok())return fail(ManufacturingMeshError::RegenerationFailure,regenerated.diagnostic);diameterCorrection=corrections.femaleDiameter->valueMillimetres;nominalDiameter=regenerated.resultingGoverningRadiusMillimetres*2.0-diameterCorrection;manufacturingDiameter=regenerated.resultingGoverningRadiusMillimetres*2.0;semanticIdentities<<corrections.femaleDiameter->semanticContractVersion;contractIdentities<<corrections.femaleDiameter->correctionContractVersion;correctionIdentity<<QStringLiteral("%1=%2").arg(corrections.femaleDiameter->correctionContractVersion).arg(diameterCorrection,0,'g',17);provenance<<QStringLiteral("Feature %1 [%2]: diameter %3 mm + %4 mm = %5 mm").arg(feature.stableIdentity,orientationEvidence).arg(nominalDiameter,0,'f',3).arg(diameterCorrection,0,'f',3).arg(manufacturingDiameter,0,'f',3);}else if(feature.family==FunctionalInterfaceFamily::StandardStud&&feature.role==FunctionalInterfaceRole::Male&&(corrections.studDiameter||corrections.studHeight)&&((corrections.studDiameter&&feature.evidenceContract==corrections.studDiameter->semanticContractVersion)||(corrections.studHeight&&feature.evidenceContract==corrections.studHeight->semanticContractVersion))){MaleStudDimensionalCorrection requested;requested.diameterMillimetres=corrections.studDiameter?corrections.studDiameter->valueMillimetres:0;requested.heightMillimetres=corrections.studHeight?corrections.studHeight->valueMillimetres:0;regenerated=FunctionalOperandRegenerator::regenerateStud(feature,requested);if(!regenerated.ok())return fail(ManufacturingMeshError::RegenerationFailure,regenerated.diagnostic);diameterCorrection=requested.diameterMillimetres;heightCorrection=requested.heightMillimetres;nominalDiameter=regenerated.resultingGoverningRadiusMillimetres*2.0-diameterCorrection;manufacturingDiameter=regenerated.resultingGoverningRadiusMillimetres*2.0;nominalHeight=regenerated.resultingAxialExtentMillimetres-heightCorrection;manufacturingHeight=regenerated.resultingAxialExtentMillimetres;if(corrections.studDiameter){semanticIdentities<<corrections.studDiameter->semanticContractVersion;contractIdentities<<corrections.studDiameter->correctionContractVersion;correctionIdentity<<QStringLiteral("%1=%2").arg(corrections.studDiameter->correctionContractVersion).arg(diameterCorrection,0,'g',17);}if(corrections.studHeight){semanticIdentities<<corrections.studHeight->semanticContractVersion;contractIdentities<<corrections.studHeight->correctionContractVersion;correctionIdentity<<QStringLiteral("%1=%2").arg(corrections.studHeight->correctionContractVersion).arg(heightCorrection,0,'g',17);}provenance<<QStringLiteral("Feature %1 [%2]: stud OD %3 mm + %4 mm = %5 mm; height %6 mm + %7 mm = %8 mm").arg(feature.stableIdentity,orientationEvidence).arg(nominalDiameter,0,'f',3).arg(diameterCorrection,0,'f',3).arg(manufacturingDiameter,0,'f',3).arg(nominalHeight,0,'f',3).arg(heightCorrection,0,'f',3);if(!corrections.studHeightDiagnostic.isEmpty())provenance<<corrections.studHeightDiagnostic;}else if(feature.family==FunctionalInterfaceFamily::StudReceivingClutch&&feature.role==FunctionalInterfaceRole::Female&&corrections.receivingTubeDiameter&&feature.evidenceContract==corrections.receivingTubeDiameter->semanticContractVersion){regenerated=FunctionalOperandRegenerator::regenerateReceivingTube(feature,{corrections.receivingTubeDiameter->valueMillimetres});if(!regenerated.ok())return fail(ManufacturingMeshError::RegenerationFailure,regenerated.diagnostic);diameterCorrection=corrections.receivingTubeDiameter->valueMillimetres;nominalDiameter=regenerated.resultingGoverningRadiusMillimetres*2.0-diameterCorrection;manufacturingDiameter=regenerated.resultingGoverningRadiusMillimetres*2.0;semanticIdentities<<corrections.receivingTubeDiameter->semanticContractVersion;contractIdentities<<corrections.receivingTubeDiameter->correctionContractVersion;correctionIdentity<<QStringLiteral("%1=%2").arg(corrections.receivingTubeDiameter->correctionContractVersion).arg(diameterCorrection,0,'g',17);provenance<<QStringLiteral("Feature %1 [%2]: receiving tube OD %3 mm + %4 mm = %5 mm; protected bore %6 mm").arg(feature.stableIdentity,orientationEvidence).arg(nominalDiameter,0,'f',3).arg(diameterCorrection,0,'f',3).arg(manufacturingDiameter,0,'f',3).arg(feature.protectedInnerRadiusMillimetres*2.0,0,'f',3);}else if(feature.family==FunctionalInterfaceFamily::TechnicAxle&&feature.role==FunctionalInterfaceRole::Male&&corrections.technicAxleTipToTip&&feature.evidenceContract==corrections.technicAxleTipToTip->semanticContractVersion){regenerated=FunctionalOperandRegenerator::regenerateTechnicAxleProfile(feature,{corrections.technicAxleTipToTip->valueMillimetres});diameterCorrection=corrections.technicAxleTipToTip->valueMillimetres;nominalDiameter=feature.nominalDiameterMillimetres;manufacturingDiameter=nominalDiameter+diameterCorrection;semanticIdentities<<corrections.technicAxleTipToTip->semanticContractVersion;contractIdentities<<corrections.technicAxleTipToTip->correctionContractVersion;correctionIdentity<<QStringLiteral("%1=%2").arg(corrections.technicAxleTipToTip->correctionContractVersion).arg(diameterCorrection,0,'g',17);provenance<<QStringLiteral("Feature %1 [%2]: axle tip-to-tip %3 mm + %4 mm = %5 mm").arg(feature.stableIdentity,orientationEvidence).arg(nominalDiameter,0,'f',3).arg(diameterCorrection,0,'f',3).arg(manufacturingDiameter,0,'f',3);}else if(feature.family==FunctionalInterfaceFamily::TechnicAxleHole&&feature.role==FunctionalInterfaceRole::Female&&corrections.technicAxleHoleArmWidth){auto armFeature=feature;armFeature.constructionRecipe=QStringLiteral("technic-axle-hole-arm-width-clearance-v2");armFeature.evidenceContract=QStringLiteral("official-ldraw-axlehole-arm-width-clearance-v2");regenerated=FunctionalOperandRegenerator::regenerateTechnicAxleHoleArmWidth(armFeature,{corrections.technicAxleHoleArmWidth->valueMillimetres,corrections.technicAxleHoleArmWidth->fixedTipToTipCorrectionMillimetres});diameterCorrection=corrections.technicAxleHoleArmWidth->valueMillimetres;nominalDiameter=feature.protectedCrossArmHalfWidthMillimetres*2.0;manufacturingDiameter=nominalDiameter+diameterCorrection;semanticIdentities<<corrections.technicAxleHoleArmWidth->semanticContractVersion;contractIdentities<<corrections.technicAxleHoleArmWidth->correctionContractVersion;correctionIdentity<<QStringLiteral("%1=%2").arg(corrections.technicAxleHoleArmWidth->correctionContractVersion).arg(diameterCorrection,0,'g',17);provenance<<QStringLiteral("Feature %1 [%2]: axle-hole arm width %3 mm + %4 mm = %5 mm; tip-to-tip fixed at %6 mm").arg(feature.stableIdentity,orientationEvidence).arg(nominalDiameter,0,'f',3).arg(diameterCorrection,0,'f',3).arg(manufacturingDiameter,0,'f',3).arg(feature.nominalDiameterMillimetres+corrections.technicAxleHoleArmWidth->fixedTipToTipCorrectionMillimetres,0,'f',3);}if(regenerated.ok()){operand.closedMesh=regenerated.mesh;operand.analysis=regenerated.analysis;featureIdentities<<feature.stableIdentity;replaced=true;break;}++skipped;provenance<<QStringLiteral("Feature %1 [%2] remained nominal: %3").arg(feature.stableIdentity,orientationEvidence,featureReason);}}
    if(!replaced)return fail(ManufacturingMeshError::MissingCorrection,provenance.isEmpty()?QStringLiteral("No recognized functional operand matches the compatible profile corrections."):provenance.join(' '));
    auto backend=m_factory();if(!backend||operands.isEmpty())return fail(ManufacturingMeshError::BooleanFailure,"No Boolean composition service is available.");PrintMesh accumulated=operands.front().closedMesh;for(int i=1;i<operands.size();++i){auto operation=operands[i].role==SemanticRole::SubtractivePassage?backend->subtract(accumulated,operands[i].closedMesh):backend->unite(accumulated,operands[i].closedMesh);if(!operation.ok())return fail(ManufacturingMeshError::BooleanFailure,QString::fromStdString(operation.message));accumulated=std::move(operation.mesh);}
    const auto analysis=analyzeSource(accumulated);if(!validatePreparedMesh(analysis).ok())return fail(ManufacturingMeshError::InvalidResult,"The composed ManufacturingMesh failed strict validation.");
    semanticIdentities.removeDuplicates();contractIdentities.removeDuplicates();correctionIdentity.removeDuplicates();
    auto output=std::make_shared<ManufacturingMesh>();output->mesh=std::move(accumulated);output->analysis=analysis;output->partReference=prepared.partReference;output->fitProfileIdentity=profile.profileIdentity;output->sourceSessionIdentity=profile.sourceSessionIdentity;output->featureIdentities=featureIdentities;output->featureIdentity=featureIdentities.join('|');output->semanticContractVersion=semanticIdentities.join('|');output->correctionContractVersion=contractIdentities.join('|');output->regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();output->booleanVersion=backend->versionIdentity();output->nominalDiameterMillimetres=nominalDiameter;output->diameterCorrectionMillimetres=diameterCorrection;output->manufacturingDiameterMillimetres=manufacturingDiameter;output->nominalHeightMillimetres=nominalHeight;output->heightCorrectionMillimetres=heightCorrection;output->manufacturingHeightMillimetres=manufacturingHeight;
    output->nominalPreparationIdentity=prepared.partReference+'|'+prepared.ldrawIdentity+'|'+prepared.preparationProfileVersion+'|'+prepared.mcutVersion;output->provenance<<QStringLiteral("Nominal PreparedMesh: %1").arg(output->nominalPreparationIdentity)<<QStringLiteral("Verified Fit Profile: %1").arg(profile.profileIdentity)<<provenance;
    const QByteArray identity=(output->nominalPreparationIdentity+'|'+profile.profileIdentity+'|'+profile.processFingerprint+'|'+printOrientation.summary()+'|'+featureIdentities.join('|')+'|'+correctionIdentity.join('|')+'|'+output->regeneratorAlgorithmVersion+'|'+output->booleanVersion).toUtf8();output->identity=QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());
    ManufacturingMeshResult result;result.error=ManufacturingMeshError::None;result.manufacturingMesh=output;result.diagnostic=QStringLiteral("Separate profile-driven ManufacturingMesh generated per functional operand; %1 feature(s) corrected and %2 recognized feature(s) left nominal. Source and nominal PreparedMesh were not modified.").arg(featureIdentities.size()).arg(skipped);return result;
}

ManufacturingMeshResult ManufacturingMeshService::generate(const LDrawGeometry::LDrawLoadResult&source,const PreparedMesh&prepared,const FitProfile&profile,FitPrintedOrientation orientation)const
{
    PrintOrientation printOrientation;if(orientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate)printOrientation.rotate(PrintOrientation::Rotation::XPositive);return generate(source,prepared,profile,printOrientation);
}
}
