#include "StudReceivingCalibrationArtifact.h"
#include "../print/McutMeshBooleanService.h"
#include "../print/PrintMeshAnalysis.h"
#include <algorithm>
#include <cmath>

namespace PrintGeometry { namespace {
PrintMesh box(double x0,double x1,double y0,double y1,double z0,double z1){PrintMesh m;m.vertices={{x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}};m.faces={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}};return m;}
PrintMesh cylinder(double cx,double cy,double radius,double bottom,double top){
    constexpr int segments=32;
    constexpr double pi=3.14159265358979323846;
    PrintMesh mesh;
    for(int ring=0;ring<2;++ring)for(int i=0;i<segments;++i){const double angle=2*pi*i/segments;mesh.vertices.push_back({cx+radius*std::cos(angle),cy+radius*std::sin(angle),ring?top:bottom});}
    const auto bottomCenter=std::uint32_t(mesh.vertices.size());mesh.vertices.push_back({cx,cy,bottom});
    const auto topCenter=std::uint32_t(mesh.vertices.size());mesh.vertices.push_back({cx,cy,top});
    for(int i=0;i<segments;++i){const auto j=(i+1)%segments;mesh.faces.push_back({std::uint32_t(i),std::uint32_t(j),std::uint32_t(segments+i)});mesh.faces.push_back({std::uint32_t(j),std::uint32_t(segments+j),std::uint32_t(segments+i)});mesh.faces.push_back({bottomCenter,std::uint32_t(j),std::uint32_t(i)});mesh.faces.push_back({topCenter,std::uint32_t(segments+i),std::uint32_t(segments+j)});}
    return mesh;
}
}
FunctionalFeature StudReceivingCalibrationArtifact::canonicalPrototype(){FunctionalFeature f;f.stableIdentity="official-stud4-tube-wall-cell-prototype";f.family=FunctionalInterfaceFamily::StudReceivingClutch;f.role=FunctionalInterfaceRole::Female;f.materialSide=FunctionalMaterialSide::MaterialInside;f.eligibility=FunctionalEligibility::Eligible;f.confidence=SemanticConfidence::HighConfidence;f.frame={{0,0,0},{0,0,1},{1,0,0},{0,1,0},false};f.nominalRadiusMillimetres=3.2;f.nominalDiameterMillimetres=6.4;f.nominalAxialExtentMillimetres=3.2;f.nominalEngagementExtentMillimetres=3.2;f.protectedInnerRadiusMillimetres=2.4;f.operandAction=FunctionalOperandAction::Unite;f.constructionRecipe="stud-receiving-tube-wall-cell-v1";f.evidenceContract="official-ldraw-stud4-tube-wall-cell-v1";f.governingOperandIdentity=f.stableIdentity+":operand";f.radialProfile={{0,3.2},{3.2,3.2}};return f;}
FunctionalFeature StudReceivingCalibrationArtifact::canonicalPostWallPrototype(){FunctionalFeature f;f.stableIdentity="official-stud3-post-wall-cell-prototype";f.family=FunctionalInterfaceFamily::StudReceivingClutch;f.role=FunctionalInterfaceRole::Female;f.materialSide=FunctionalMaterialSide::MaterialInside;f.eligibility=FunctionalEligibility::Eligible;f.confidence=SemanticConfidence::HighConfidence;f.frame={{0,0,1.6},{0,0,1},{1,0,0},{0,1,0},false};f.nominalRadiusMillimetres=1.6;f.nominalDiameterMillimetres=3.2;f.nominalAxialExtentMillimetres=3.2;f.nominalEngagementExtentMillimetres=3.2;f.operandAction=FunctionalOperandAction::Unite;f.constructionRecipe="stud-receiving-post-wall-cell-v1";f.evidenceContract="official-ldraw-stud3-post-wall-cell-v1";f.governingOperandIdentity=f.stableIdentity+":operand";f.radialProfile={{0,1.6},{3.2,1.6}};return f;}
QString StudReceivingCalibrationArtifact::artifactIdentity(){return "stud-receiving-clutch-tube-wall-cell-perpendicular-v1";}
QString StudReceivingCalibrationArtifact::postWallArtifactIdentity(){return "stud-receiving-clutch-post-wall-cell-perpendicular-v1";}
FunctionalFeature StudReceivingCalibrationArtifact::canonicalWallPocketPrototype(bool brickDepth)
{
    FunctionalFeature feature;
    feature.stableIdentity = brickDepth ? "official-box5-wall-pocket-brick-prototype" : "official-box5-wall-pocket-plate-prototype";
    feature.family = FunctionalInterfaceFamily::StudReceivingClutch;
    feature.role = FunctionalInterfaceRole::Female;
    feature.materialSide = FunctionalMaterialSide::EmptyInsideMaterialOutside;
    feature.eligibility = FunctionalEligibility::Eligible;
    feature.confidence = SemanticConfidence::HighConfidence;
    feature.frame = {{0,0,2},{0,0,1},{1,0,0},{0,1,0},false};
    feature.nominalRadiusMillimetres = 2.4;
    feature.nominalDiameterMillimetres = 4.8;
    feature.nominalAxialExtentMillimetres = brickDepth ? 8.0 : 1.6;
    feature.nominalEngagementExtentMillimetres = feature.nominalAxialExtentMillimetres;
    feature.operandAction = FunctionalOperandAction::Subtract;
    feature.constructionRecipe = "stud-receiving-wall-pocket-square-v1";
    feature.evidenceContract = brickDepth ? "official-ldraw-box5-wall-pocket-brick-v1" : "official-ldraw-box5-wall-pocket-plate-v1";
    feature.governingOperandIdentity = feature.stableIdentity + ":body-walls";
    feature.radialProfile = {{0,2.4},{feature.nominalAxialExtentMillimetres,2.4}};
    return feature;
}
QString StudReceivingCalibrationArtifact::wallPocketArtifactIdentity(bool brickDepth)
{
    return brickDepth ? "stud-receiving-clutch-wall-pocket-brick-perpendicular-coarse-v1"
                      : "stud-receiving-clutch-wall-pocket-plate-perpendicular-coarse-v1";
}
FunctionalFeature StudReceivingCalibrationArtifact::canonicalAntiStudBorePrototype()
{
    FunctionalFeature feature;
    feature.stableIdentity="official-stud4o-antistud-bore-prototype";
    feature.family=FunctionalInterfaceFamily::StudReceivingClutch;
    feature.role=FunctionalInterfaceRole::Female;
    feature.materialSide=FunctionalMaterialSide::EmptyInsideMaterialOutside;
    feature.eligibility=FunctionalEligibility::Eligible;
    feature.confidence=SemanticConfidence::HighConfidence;
    feature.frame={{0,0,4},{0,0,-1},{1,0,0},{0,-1,0},false};
    feature.nominalRadiusMillimetres=2.4;
    feature.nominalDiameterMillimetres=4.8;
    feature.nominalAxialExtentMillimetres=2.0;
    feature.nominalEngagementExtentMillimetres=2.0;
    feature.operandAction=FunctionalOperandAction::Subtract;
    feature.constructionRecipe="stud-receiving-antistud-bore-v1";
    feature.evidenceContract="official-ldraw-stud4o-antistud-bore-v1";
    feature.governingOperandIdentity=feature.stableIdentity+":body-bore";
    feature.radialProfile={{0,2.4},{2.0,2.4}};
    return feature;
}
QString StudReceivingCalibrationArtifact::antiStudBoreArtifactIdentity()
{
    return QStringLiteral("stud-receiving-clutch-antistud-bore-perpendicular-coarse-v1");
}
QString StudReceivingCalibrationArtifact::orientationIdentity(){return "flat-base-receiving-axis-perpendicular-v1";}
QVector<double> StudReceivingCalibrationArtifact::diameterCorrectionsMillimetres(){return {-.3,-.2,-.1,0,.1,.2,.3};}
FitCalibrationExperiment StudReceivingCalibrationArtifact::observationTemplate(const StudReceivingCalibrationArtifactResult&a){StudReceivingCalibrationArtifactDefinition d;d.artifactIdentity=a.artifactIdentity;d.parentArtifactIdentity=a.parentArtifactIdentity;return observationTemplate(a,d);}
StudReceivingCalibrationArtifactResult StudReceivingCalibrationArtifact::generate(){StudReceivingCalibrationArtifactDefinition d;d.artifactIdentity=artifactIdentity();return generate(canonicalPrototype(),d);}
StudReceivingCalibrationArtifactResult StudReceivingCalibrationArtifact::generateMarkedTubeWallCell(){
    StudReceivingCalibrationArtifactDefinition definition;
    definition.artifactIdentity=QStringLiteral("stud-receiving-clutch-tube-wall-cell-perpendicular-pilot-v2");
    auto result=generate(canonicalPrototype(),definition);
    if(!result.ok)return result;
    McutMeshBooleanService boolean;
    auto marked=boolean.subtract(result.mesh,box(1,3,0,0.8,3.8,5.0));
    if(!marked.ok()){result.ok=false;result.diagnostic=QStringLiteral("The Candidate #1-side marker could not be formed.");return result;}
    result.mesh=std::move(marked.mesh);
    result.analysis=analyzeSource(result.mesh);
    const auto valid=validatePreparedMesh(result.analysis);
    if(!valid.ok()){result.ok=false;result.diagnostic=QStringLiteral("The marked TubeWallCell artifact failed validation: %1").arg(QString::fromStdString(valid.message));}
    return result;
}
StudReceivingCalibrationArtifactResult StudReceivingCalibrationArtifact::generatePostWallCell(){StudReceivingCalibrationArtifactDefinition d;d.artifactIdentity=postWallArtifactIdentity();return generate(canonicalPostWallPrototype(),d);}
StudReceivingCalibrationArtifactResult StudReceivingCalibrationArtifact::generateWallPocket(bool brickDepth)
{
    StudReceivingCalibrationArtifactDefinition definition;
    definition.artifactIdentity = wallPocketArtifactIdentity(brickDepth);
    return generateWallPocket(canonicalWallPocketPrototype(brickDepth), definition);
}
StudReceivingCalibrationArtifactResult StudReceivingCalibrationArtifact::generateAntiStudBore()
{
    StudReceivingCalibrationArtifactDefinition definition;
    definition.artifactIdentity=antiStudBoreArtifactIdentity();
    return generateAntiStudBore(canonicalAntiStudBorePrototype(),definition);
}
StudReceivingCalibrationArtifactResult StudReceivingCalibrationArtifact::generateAntiStudBore(
    const FunctionalFeature& prototype,const StudReceivingCalibrationArtifactDefinition& definition)
{
    StudReceivingCalibrationArtifactResult result;
    result.artifactIdentity=definition.artifactIdentity;
    result.parentArtifactIdentity=definition.parentArtifactIdentity;
    result.orientationIdentity=orientationIdentity();
    result.regenerationPrototype=prototype;
    if(prototype.family!=FunctionalInterfaceFamily::StudReceivingClutch ||
       prototype.constructionRecipe!=QStringLiteral("stud-receiving-antistud-bore-v1") ||
       prototype.evidenceContract!=QStringLiteral("official-ldraw-stud4o-antistud-bore-v1") ||
       definition.artifactIdentity.isEmpty() || definition.candidateCount<3 ||
       definition.candidateCount>9 || definition.candidateCount%2==0 ||
       definition.candidateSpacingMillimetres<=0){
        result.diagnostic=QStringLiteral("The AntiStudBore fixture or retained source contract is invalid.");
        return result;
    }
    McutMeshBooleanService boolean;
    PrintMesh body=box(0,12*definition.candidateCount,0,12,0,4);
    const int half=definition.candidateCount/2;
    for(int i=0;i<definition.candidateCount;++i){
        const double correction=definition.centerDiameterCorrectionMillimetres+
            (i-half)*definition.candidateSpacingMillimetres;
        const double diameter=4.8+correction;
        if(diameter<=0 || std::abs(correction)>1.0){result.diagnostic=QStringLiteral("AntiStudBore candidate diameter is outside the certified fixture range.");return result;}
        const auto cut=boolean.subtract(body,cylinder(6+12*i,6,diameter*.5,2.0,4.1));
        if(!cut.ok()){result.diagnostic=QStringLiteral("AntiStudBore candidate %1 could not be cut: %2").arg(i+1).arg(QString::fromStdString(cut.message));return result;}
        body=cut.mesh;
        FitCalibrationCandidate candidate;candidate.index=i+1;
        candidate.diameterCorrectionMillimetres=correction;
        candidate.functionalDiameterMillimetres=diameter;
        result.candidates.push_back(candidate);
    }
    const auto marker=boolean.subtract(body,box(1,3,0,.8,3,4.1));
    if(!marker.ok()){result.diagnostic=QStringLiteral("The AntiStudBore Candidate #1 marker could not be formed.");return result;}
    result.mesh=marker.mesh;
    result.analysis=analyzeSource(result.mesh);
    const auto valid=validatePreparedMesh(result.analysis);
    if(!valid.ok()){result.diagnostic=QStringLiteral("AntiStudBore fixture failed strict mesh validation: %1").arg(QString::fromStdString(valid.message));return result;}
    result.ok=true;
    result.diagnostic=QStringLiteral("%1 independent centered AntiStudBore openings generated with fixed engagement depth and exterior.").arg(definition.candidateCount);
    return result;
}
StudReceivingCalibrationArtifactResult StudReceivingCalibrationArtifact::generateWallPocket(
    const FunctionalFeature& prototype, const StudReceivingCalibrationArtifactDefinition& definition)
{
    StudReceivingCalibrationArtifactResult result;
    result.artifactIdentity = definition.artifactIdentity;
    result.parentArtifactIdentity = definition.parentArtifactIdentity;
    result.orientationIdentity = orientationIdentity();
    result.regenerationPrototype = prototype;
    const bool plate = prototype.evidenceContract == QStringLiteral("official-ldraw-box5-wall-pocket-plate-v1");
    const bool brick = prototype.evidenceContract == QStringLiteral("official-ldraw-box5-wall-pocket-brick-v1");
    if (prototype.family != FunctionalInterfaceFamily::StudReceivingClutch ||
        prototype.role != FunctionalInterfaceRole::Female ||
        prototype.constructionRecipe != QStringLiteral("stud-receiving-wall-pocket-square-v1") ||
        (!plate && !brick) ||
        std::abs(prototype.nominalAxialExtentMillimetres-(brick ? 8.0 : 1.6)) > 1e-9 ||
        definition.artifactIdentity.isEmpty() || definition.candidateCount < 3 ||
        definition.candidateCount > 9 || !(definition.candidateCount % 2) ||
        definition.candidateSpacingMillimetres <= 0.0) {
        result.diagnostic = QStringLiteral("The depth-specific WallPocket fixture contract is invalid.");
        return result;
    }
    constexpr double pitch = 12.0, width = 12.0, floor = 2.0;
    const double height = floor + prototype.nominalAxialExtentMillimetres;
    McutMeshBooleanService booleanService;
    PrintMesh body = box(0,pitch*definition.candidateCount,0,width,0,height);
    for (int i = 0; i < definition.candidateCount; ++i) {
        const double correction = definition.centerDiameterCorrectionMillimetres +
            (i-definition.candidateCount/2)*definition.candidateSpacingMillimetres;
        const double opening = prototype.nominalDiameterMillimetres + correction;
        if (opening < 3.5 || opening > 6.0) {
            result.diagnostic = QStringLiteral("WallPocket candidate opening lies outside the safe fixture range.");
            return result;
        }
        const double x = pitch*(i+.5), y = width*.5, half = opening*.5;
        auto cut = booleanService.subtract(body,box(x-half,x+half,y-half,y+half,floor,height+.2));
        if (!cut.ok()) {
            result.diagnostic = QStringLiteral("WallPocket candidate %1 cavity failed: %2")
                .arg(i+1).arg(QString::fromStdString(cut.message));
            return result;
        }
        body = std::move(cut.mesh);
        FitCalibrationCandidate candidate;
        candidate.index = i+1;
        candidate.diameterCorrectionMillimetres = correction;
        candidate.functionalDiameterMillimetres = opening;
        result.candidates.push_back(candidate);
    }
    auto marked = booleanService.subtract(body,box(.5,2.5,0,.8,height-.9,height+.2));
    if (!marked.ok()) {
        result.diagnostic = QStringLiteral("The WallPocket Candidate #1-side marker failed.");
        return result;
    }
    result.mesh = std::move(marked.mesh);
    result.analysis = analyzeSource(result.mesh);
    const auto valid = validatePreparedMesh(result.analysis);
    if (!valid.ok()) {
        result.diagnostic = QStringLiteral("WallPocket fixture failed topology validation: %1")
            .arg(QString::fromStdString(valid.message));
        return result;
    }
    result.ok = true;
    result.diagnostic = QStringLiteral("%1 independent %2 WallPocket openings, each with a fixed %3 mm depth and 2.00 mm floor; Candidate #1-side marker present.")
        .arg(definition.candidateCount).arg(brick ? QStringLiteral("brick-depth") : QStringLiteral("plate-depth"))
        .arg(prototype.nominalAxialExtentMillimetres,0,'f',2);
    return result;
}
StudReceivingCalibrationArtifactResult StudReceivingCalibrationArtifact::generate(const FunctionalFeature&prototype,const StudReceivingCalibrationArtifactDefinition&d){StudReceivingCalibrationArtifactResult r;r.artifactIdentity=d.artifactIdentity;r.parentArtifactIdentity=d.parentArtifactIdentity;r.orientationIdentity=orientationIdentity();r.regenerationPrototype=prototype;const bool tubeWall=prototype.constructionRecipe=="stud-receiving-tube-wall-cell-v1",postWall=prototype.constructionRecipe=="stud-receiving-post-wall-cell-v1";if(prototype.family!=FunctionalInterfaceFamily::StudReceivingClutch||(!tubeWall&&!postWall)||d.artifactIdentity.isEmpty()||d.candidateCount<3||d.candidateCount>9||d.candidateCount%2==0||d.candidateSpacingMillimetres<=0){r.diagnostic="The receiving-clutch artifact definition or retained variant contract is invalid.";return r;}McutMeshBooleanService b;const int half=d.candidateCount/2;if(postWall){PrintMesh body=box(0,18*d.candidateCount,0,8,0,4.8);for(int i=0;i<d.candidateCount;++i){const double correction=d.centerDiameterCorrectionMillimetres+(i-half)*d.candidateSpacingMillimetres,cx=9+18*i;auto cut=b.subtract(body,box(cx-6.4,cx+6.4,1.6,6.4,1.6,5.0));if(!cut.ok()){r.diagnostic=QString("PostWallCell cavity %1 failed.").arg(i+1);return r;}body=std::move(cut.mesh);auto feature=prototype;feature.stableIdentity=QString("%1:candidate-%2").arg(r.artifactIdentity).arg(i+1);feature.frame.origin={cx,4,1.6};const auto post=FunctionalOperandRegenerator::regenerateReceivingPost(feature,{correction});if(!post.ok()){r.diagnostic=post.diagnostic;return r;}auto joined=b.unite(body,post.mesh);if(!joined.ok()){r.diagnostic=QString("Receiving post %1 could not be joined.").arg(i+1);return r;}body=std::move(joined.mesh);FitCalibrationCandidate c;c.index=i+1;c.diameterCorrectionMillimetres=correction;c.functionalDiameterMillimetres=3.2+correction;r.candidates.push_back(c);}auto marker=b.subtract(body,box(1,3,0,0.8,3.8,5.0));if(!marker.ok()){r.diagnostic="The Candidate #1-side marker could not be formed.";return r;}r.mesh=std::move(marker.mesh);r.analysis=analyzeSource(r.mesh);const auto valid=validatePreparedMesh(r.analysis);if(!valid.ok()){r.diagnostic=QString("PostWallCell artifact failed validation: %1").arg(QString::fromStdString(valid.message));return r;}r.ok=true;r.diagnostic=QString("%1 independent PostWallCell candidates generated with fixed wall positions, pitch, seating depth, and post height.").arg(d.candidateCount);return r;}PrintMesh body=box(0,18*d.candidateCount,0,16,0,4.8);for(int i=0;i<d.candidateCount;++i){const double correction=d.centerDiameterCorrectionMillimetres+(i-half)*d.candidateSpacingMillimetres,cx=9+18*i;auto cut=b.subtract(body,box(cx-6.4,cx+6.4,1.6,14.4,1.6,5.0));if(!cut.ok()){r.diagnostic=QString("Receiving-cell cavity %1 failed.").arg(i+1);return r;}body=std::move(cut.mesh);auto feature=prototype;feature.stableIdentity=QString("%1:candidate-%2").arg(r.artifactIdentity).arg(i+1);feature.frame.origin={cx,8,1.6};const auto tube=FunctionalOperandRegenerator::regenerateReceivingTube(feature,{correction});if(!tube.ok()){r.diagnostic=tube.diagnostic;return r;}auto joined=b.unite(body,tube.mesh);if(!joined.ok()){r.diagnostic=QString("Receiving tube %1 could not be joined.").arg(i+1);return r;}body=std::move(joined.mesh);FitCalibrationCandidate c;c.index=i+1;c.diameterCorrectionMillimetres=correction;c.functionalDiameterMillimetres=6.4+correction;r.candidates.push_back(c);}r.mesh=std::move(body);r.analysis=analyzeSource(r.mesh);const auto valid=validatePreparedMesh(r.analysis);if(!valid.ok()){r.diagnostic=QString("TubeWallCell artifact failed validation: %1").arg(QString::fromStdString(valid.message));return r;}r.ok=true;r.diagnostic=QString("%1 independent TubeWallCell candidates generated at fixed 100% scale.").arg(d.candidateCount);return r;}
FitCalibrationExperiment StudReceivingCalibrationArtifact::observationTemplate(const StudReceivingCalibrationArtifactResult&a,const StudReceivingCalibrationArtifactDefinition&d){FitCalibrationExperiment e;e.artifactIdentity=a.artifactIdentity;e.parentArtifactIdentity=d.parentArtifactIdentity;e.featureFamily="StudReceivingClutch";e.featureRole="female";e.modeledOrientationIdentity=a.orientationIdentity;e.centerDiameterCorrectionMillimetres=d.centerDiameterCorrectionMillimetres;e.candidateSpacingMillimetres=d.candidateSpacingMillimetres;e.regenerationPrototype=a.regenerationPrototype;e.hasRegenerationPrototype=true;e.candidates=a.candidates;e.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;const bool postWall=a.regenerationPrototype.constructionRecipe==QStringLiteral("stud-receiving-post-wall-cell-v1"),wallPocket=a.regenerationPrototype.constructionRecipe==QStringLiteral("stud-receiving-wall-pocket-square-v1"),antiStud=a.regenerationPrototype.constructionRecipe==QStringLiteral("stud-receiving-antistud-bore-v1");e.process.orientationNotes=antiStud?QStringLiteral("Print the flat base down with centered bores open upward. Test each bore with a genuine LEGO stud entering its center, not the tube-and-wall cell. Candidate #1 is beside the wall notch."):wallPocket?QStringLiteral("Print the flat base on the build plate with square pockets open upward; test each pocket with genuine LEGO studs. Candidate #1 is beside the wall notch. Keep brick-depth and plate-depth evidence separate."):postWall?QStringLiteral("Print the flat base on the build plate; press genuine LEGO studs into both bays of each PostWallCell. Candidate #1 is beside the wall notch."):QStringLiteral("Print the flat base on the build plate; test each receiving cell with a genuine LEGO stud in the normal tube-and-wall position, not in the center bore.");e.process.dimensionalCompensationNotes="Print at 100% scale and record all slicer dimensional compensation settings.";return e;}
}
