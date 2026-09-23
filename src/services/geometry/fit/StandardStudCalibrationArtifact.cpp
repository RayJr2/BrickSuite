#include "StandardStudCalibrationArtifact.h"
#include "FitCalibrationPackage.h"
#include "FitCalibrationFixtureLabel.h"
#include "FitCalibrationLibrary.h"
#include "FitCalibrationNamingCatalog.h"

#include "../print/McutMeshBooleanService.h"
#include "../print/PrintMeshAnalysis.h"

#include <QPointF>
#include <algorithm>
#include <cmath>

namespace PrintGeometry { namespace {
PrintMesh keyedBase(int count)
{
    PrintMesh mesh;const double width=12.0*count+8.0;const QVector<QPointF> profile{{0,0},{width,0},{width,3},{3,3},{0,2}};
    for(double y:{0.0,10.0})for(const auto&p:profile)mesh.vertices.push_back({p.x(),y,p.y()});const auto n=std::uint32_t(profile.size());
    for(std::uint32_t i=1;i+1<n;++i){mesh.faces.push_back({0,i+1,i});mesh.faces.push_back({n,n+i,n+i+1});}
    for(std::uint32_t i=0;i<n;++i){const auto next=(i+1)%n;mesh.faces.push_back({i,next,n+i});mesh.faces.push_back({next,n+next,n+i});}
    if(analyzeSource(mesh).signedVolume<0)for(auto&face:mesh.faces)std::swap(face[1],face[2]);return mesh;
}
bool supported(const FunctionalFeature&f){return f.family==FunctionalInterfaceFamily::StandardStud&&f.role==FunctionalInterfaceRole::Male&&f.materialSide==FunctionalMaterialSide::MaterialInside&&f.eligibility==FunctionalEligibility::Eligible&&f.confidence==SemanticConfidence::HighConfidence&&f.operandAction==FunctionalOperandAction::Unite&&f.constructionRecipe==QStringLiteral("standard-solid-stud-v1");}
}

FunctionalFeature StandardStudCalibrationArtifact::canonicalPrototype()
{
    FunctionalFeature f;f.stableIdentity=QStringLiteral("official-standard-stud-calibration-prototype");f.family=FunctionalInterfaceFamily::StandardStud;f.role=FunctionalInterfaceRole::Male;f.materialSide=FunctionalMaterialSide::MaterialInside;f.eligibility=FunctionalEligibility::Eligible;f.confidence=SemanticConfidence::HighConfidence;f.frame={{0,0,0},{0,0,1},{1,0,0},{0,1,0},false};f.nominalRadiusMillimetres=2.4;f.nominalDiameterMillimetres=4.8;f.nominalAxialExtentMillimetres=1.6;f.nominalEngagementExtentMillimetres=1.6;f.operandAction=FunctionalOperandAction::Unite;f.governingOperandIdentity=QStringLiteral("official-standard-stud-calibration-prototype:operand");f.constructionRecipe=QStringLiteral("standard-solid-stud-v1");f.evidenceContract=QStringLiteral("official-ldraw-standard-stud-v1");f.radialProfile={{0,2.4},{1.6,2.4}};return f;
}
QString StandardStudCalibrationArtifact::diameterArtifactIdentity(){return QStringLiteral("standard-stud-male-od-perpendicular-labeled-v2");}
QString StandardStudCalibrationArtifact::heightArtifactIdentity(){return QStringLiteral("standard-stud-male-height-perpendicular-v1");}
QString StandardStudCalibrationArtifact::orientationIdentity(){return QStringLiteral("flat-base-stud-axis-perpendicular-v1");}

bool StandardStudCalibrationArtifact::heightVerificationDefinition(const FitCalibrationExperiment&provisionalHeight,double verifiedDiameterCorrectionMillimetres,StandardStudCalibrationArtifactDefinition*out,QString*error)
{
    if(!out||provisionalHeight.featureFamily!=QStringLiteral("StandardStud")||provisionalHeight.featureRole!=QStringLiteral("male")||provisionalHeight.correctionDimension!=FitCorrectionDimension::Height||provisionalHeight.artifactIdentity.isEmpty()||provisionalHeight.preferredCandidateIndex<=0||!std::isfinite(verifiedDiameterCorrectionMillimetres)){if(error)*error=QStringLiteral("A preferred Stud Height experiment and Verified Stud OD correction are required.");return false;}
    const auto preferred=std::find_if(provisionalHeight.candidates.cbegin(),provisionalHeight.candidates.cend(),[&](const auto&candidate){return candidate.index==provisionalHeight.preferredCandidateIndex;});
    if(preferred==provisionalHeight.candidates.cend()){if(error)*error=QStringLiteral("The preferred Stud Height candidate is unavailable.");return false;}
    StandardStudCalibrationArtifactDefinition definition;definition.artifactIdentity=QStringLiteral("standard-stud-male-height-direct-verification-v2");definition.parentArtifactIdentity=provisionalHeight.artifactIdentity;definition.dimension=StandardStudCalibrationDimension::Height;definition.centerCorrectionMillimetres=preferred->heightCorrectionMillimetres;definition.candidateSpacingMillimetres=provisionalHeight.candidateSpacingMillimetres>0?std::min(.05,provisionalHeight.candidateSpacingMillimetres):.05;definition.fixedDiameterCorrectionMillimetres=verifiedDiameterCorrectionMillimetres;definition.candidateCount=3;*out=definition;if(error)error->clear();return true;
}

StandardStudCalibrationArtifactResult StandardStudCalibrationArtifact::generate(const FunctionalFeature&prototype,const StandardStudCalibrationArtifactDefinition&definition)
{
    StandardStudCalibrationArtifactResult result;result.artifactIdentity=definition.artifactIdentity;result.orientationIdentity=orientationIdentity();result.dimension=definition.dimension;result.regenerationPrototype=prototype;
    if(!supported(prototype)){result.diagnostic=QStringLiteral("The prototype is not an eligible ordinary solid stud contract.");return result;}
    if(definition.artifactIdentity.isEmpty()||definition.candidateCount<3||definition.candidateCount>9||definition.candidateCount%2==0||!std::isfinite(definition.centerCorrectionMillimetres)||!std::isfinite(definition.candidateSpacingMillimetres)||definition.candidateSpacingMillimetres<=0||!std::isfinite(definition.fixedDiameterCorrectionMillimetres)){result.diagnostic=QStringLiteral("The stud calibration definition is invalid.");return result;}
    QVector<FitCandidateValue> series;
    if(!FitCandidateSeries::generate(definition.artifactIdentity,definition.centerCorrectionMillimetres,
                                     definition.candidateSpacingMillimetres,definition.candidateCount,&series,&result.diagnostic))return result;
    PrintMesh body=keyedBase(definition.candidateCount);McutMeshBooleanService booleans;
    for(int i=0;i<series.size();++i){const double varied=series[i].correctionMillimetres;MaleStudDimensionalCorrection correction;correction.diameterMillimetres=definition.dimension==StandardStudCalibrationDimension::Diameter?varied:definition.fixedDiameterCorrectionMillimetres;correction.heightMillimetres=definition.dimension==StandardStudCalibrationDimension::Height?varied:0.0;FunctionalFeature candidate=prototype;candidate.stableIdentity=series[i].identity;candidate.frame.origin={8.0+12.0*i,5.0,3.0};candidate.frame.axis={0,0,1};candidate.frame.profileU={1,0,0};candidate.frame.profileV={0,1,0};const auto regenerated=FunctionalOperandRegenerator::regenerateStud(candidate,correction);if(!regenerated.ok()){result.diagnostic=QStringLiteral("Stud candidate %1 could not be regenerated: %2").arg(i+1).arg(regenerated.diagnostic);return result;}const auto united=booleans.unite(body,regenerated.mesh);if(!united.ok()){result.diagnostic=QStringLiteral("Stud candidate %1 could not be joined to the calibration base.").arg(i+1);return result;}body=united.mesh;FitCalibrationCandidate record;record.index=series[i].index;record.diameterCorrectionMillimetres=correction.diameterMillimetres;record.functionalDiameterMillimetres=regenerated.resultingGoverningRadiusMillimetres*2.0;record.heightCorrectionMillimetres=correction.heightMillimetres;record.functionalHeightMillimetres=regenerated.resultingAxialExtentMillimetres;result.candidates.push_back(record);}
    result.mesh=std::move(body);result.analysis=analyzeSource(result.mesh);const auto validation=validatePreparedMesh(result.analysis);if(!validation.ok()){result.diagnostic=QStringLiteral("The stud calibration artifact failed validation: %1").arg(QString::fromStdString(validation.message));return result;}
    if (definition.artifactIdentity == diameterArtifactIdentity() ||
        definition.artifactIdentity == heightArtifactIdentity()) {
        const auto experiment = observationTemplate(result, definition);
        PrintMesh labeled;
        if (!FitCalibrationFixtureLabel::recess(result.mesh,
            FitCalibrationLibrary::featureDisplayName(experiment,
                FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate),
            QString::fromLatin1(FitCalibrationNamingCatalog::forKey(
                definition.dimension == StandardStudCalibrationDimension::Height
                    ? FitCalibrationNameKey::StudHeight : FitCalibrationNameKey::StudOd).abbreviated),
            {6, 86, .5, 4.5}, &labeled, nullptr, &result.diagnostic))
            return result;
        result.mesh = std::move(labeled);
        result.analysis = analyzeSource(result.mesh);
    }
    result.ok=true;result.diagnostic=QStringLiteral("Standard stud calibration artifact generated at fixed 100% physical scale.");return result;
}

FitCalibrationExperiment StandardStudCalibrationArtifact::observationTemplate(const StandardStudCalibrationArtifactResult&a,const StandardStudCalibrationArtifactDefinition&d)
{
    FitCalibrationExperiment e;e.artifactIdentity=a.artifactIdentity;e.parentArtifactIdentity=d.parentArtifactIdentity;e.featureFamily=QStringLiteral("StandardStud");e.featureRole=QStringLiteral("male");e.modeledOrientationIdentity=a.orientationIdentity;e.correctionDimension=d.dimension==StandardStudCalibrationDimension::Height?FitCorrectionDimension::Height:FitCorrectionDimension::Diameter;e.centerDiameterCorrectionMillimetres=e.correctionDimension==FitCorrectionDimension::Diameter?d.centerCorrectionMillimetres:0.0;e.centerHeightCorrectionMillimetres=e.correctionDimension==FitCorrectionDimension::Height?d.centerCorrectionMillimetres:0.0;e.fixedDiameterCorrectionMillimetres=d.fixedDiameterCorrectionMillimetres;e.candidateSpacingMillimetres=d.candidateSpacingMillimetres;e.regenerationPrototype=a.regenerationPrototype;e.hasRegenerationPrototype=true;e.candidates=a.candidates;e.process.orientationNotes=QStringLiteral("Print the flat base on the build plate so the stud axis is perpendicular; test with genuine LEGO receiving geometry.");e.process.dimensionalCompensationNotes=QStringLiteral("Print at 100% scale and record all slicer dimensional compensation settings.");return e;
}
} // namespace PrintGeometry
