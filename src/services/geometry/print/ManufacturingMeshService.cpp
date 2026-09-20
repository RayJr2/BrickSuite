#include "ManufacturingMeshService.h"
#include "FunctionalOperandRegenerator.h"
#include "McutMeshBooleanService.h"
#include "PrintMeshAnalysis.h"
#include <QCryptographicHash>
#include <algorithm>

namespace PrintGeometry { namespace {
ManufacturingMeshResult fail(ManufacturingMeshError e,const QString&m){ManufacturingMeshResult r;r.error=e;r.diagnostic=m;return r;}
int order(SemanticRole r){return r==SemanticRole::PrimaryBody?0:r==SemanticRole::SubtractivePassage?1:r==SemanticRole::AdditiveAttachment?2:3;}
QString operandIdentity(const SemanticOperand&o){return o.sourceFiles.join('|');}
QString orientationName(FitPrintedOrientation o){return o==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate?"feature-axis-perpendicular-to-build-plate":o==FitPrintedOrientation::FeatureAxisParallelToBuildPlate?"feature-axis-parallel-to-build-plate":"unsupported";}
}
ManufacturingMeshService::ManufacturingMeshService(BooleanServiceFactory f,SemanticBuilderFunction b):m_factory(f?std::move(f):[]{return std::make_unique<McutMeshBooleanService>();}),m_builder(std::move(b)){}
ManufacturingMeshResult ManufacturingMeshService::generate(const LDrawGeometry::LDrawLoadResult&source,const PreparedMesh&prepared,const FitProfile&profile,FitPrintedOrientation orientation)const{
    if(!source.ok()||prepared.mesh.faces.empty()||prepared.partReference.isEmpty())return fail(ManufacturingMeshError::InvalidInput,"Source and nominal PreparedMesh are required.");
    if(prepared.partReference!="3700")return fail(ManufacturingMeshError::UnsupportedProofPart,"Phase 1D ManufacturingMesh proof is limited to Part 3700.");
    QString reason;if(!FitCalibrationLibrary::profileCompatibility(profile,&reason))return fail(ManufacturingMeshError::IncompatibleProfile,reason);
    const auto correction=std::find_if(profile.corrections.cbegin(),profile.corrections.cend(),[&](const auto&c){return c.featureFamily=="RoundTechnicPassage"&&c.featureRole=="female"&&c.printedOrientation==orientationName(orientation)&&c.units=="millimetres"&&c.semantics=="female-diameter-clearance";});
    if(correction==profile.corrections.cend())return fail(ManufacturingMeshError::MissingCorrection,"The selected profile has no compatible female RoundTechnicPassage correction for this print orientation.");
    auto semantic=m_builder?m_builder(source):LDrawSemanticOperandBuilder::build(source);if(!semantic.ok())return fail(ManufacturingMeshError::SemanticFailure,semantic.diagnostics.join(' '));
    QVector<SemanticOperand> operands=semantic.operands;std::stable_sort(operands.begin(),operands.end(),[](const auto&a,const auto&b){const int ao=order(a.role),bo=order(b.role);return ao==bo?operandIdentity(a)<operandIdentity(b):ao<bo;});
    FunctionalOperandRegenerationResult regenerated;bool replaced=false;QString featureIdentity;
    for(auto&operand:operands)for(const auto&feature:operand.functionalFeatures)if(!replaced&&feature.family==FunctionalInterfaceFamily::RoundTechnicPassage&&feature.role==FunctionalInterfaceRole::Female&&feature.evidenceContract==correction->semanticContractVersion){regenerated=FunctionalOperandRegenerator::regenerate(feature,{correction->valueMillimetres});if(!regenerated.ok())return fail(ManufacturingMeshError::RegenerationFailure,regenerated.diagnostic);operand.closedMesh=regenerated.mesh;operand.analysis=regenerated.analysis;featureIdentity=feature.stableIdentity;replaced=true;}
    if(!replaced)return fail(ManufacturingMeshError::MissingCorrection,"No recognized functional operand matches the profile semantic contract.");
    auto backend=m_factory();if(!backend||operands.isEmpty())return fail(ManufacturingMeshError::BooleanFailure,"No Boolean composition service is available.");PrintMesh accumulated=operands.front().closedMesh;
    for(int i=1;i<operands.size();++i){auto operation=operands[i].role==SemanticRole::SubtractivePassage?backend->subtract(accumulated,operands[i].closedMesh):backend->unite(accumulated,operands[i].closedMesh);if(!operation.ok())return fail(ManufacturingMeshError::BooleanFailure,QString::fromStdString(operation.message));accumulated=std::move(operation.mesh);}
    const auto analysis=analyzeSource(accumulated);if(!validatePreparedMesh(analysis).ok())return fail(ManufacturingMeshError::InvalidResult,"The composed ManufacturingMesh failed strict validation.");
    auto output=std::make_shared<ManufacturingMesh>();output->mesh=std::move(accumulated);output->analysis=analysis;output->partReference=prepared.partReference;output->fitProfileIdentity=profile.profileIdentity;output->sourceSessionIdentity=profile.sourceSessionIdentity;output->featureIdentity=featureIdentity;output->semanticContractVersion=correction->semanticContractVersion;output->correctionContractVersion=correction->correctionContractVersion;output->regeneratorAlgorithmVersion=correction->regeneratorAlgorithmVersion;output->booleanVersion=backend->versionIdentity();output->nominalDiameterMillimetres=regenerated.resultingGoverningRadiusMillimetres*2.0-correction->valueMillimetres;output->diameterCorrectionMillimetres=correction->valueMillimetres;output->manufacturingDiameterMillimetres=regenerated.resultingGoverningRadiusMillimetres*2.0;
    output->nominalPreparationIdentity=prepared.partReference+'|'+prepared.ldrawIdentity+'|'+prepared.preparationProfileVersion+'|'+prepared.mcutVersion;
    const QByteArray identity=(output->nominalPreparationIdentity+'|'+profile.profileIdentity+'|'+profile.processFingerprint+'|'+featureIdentity+'|'+QString::number(correction->valueMillimetres,'g',17)+'|'+correction->semanticContractVersion+'|'+correction->correctionContractVersion+'|'+correction->regeneratorAlgorithmVersion+'|'+output->booleanVersion).toUtf8();output->identity=QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());
    output->provenance<<QStringLiteral("Nominal PreparedMesh: %1").arg(output->nominalPreparationIdentity)<<QStringLiteral("Verified Fit Profile: %1").arg(profile.profileIdentity)<<QStringLiteral("Feature %1: diameter %2 mm + %3 mm = %4 mm").arg(featureIdentity).arg(output->nominalDiameterMillimetres,0,'f',3).arg(output->diameterCorrectionMillimetres,0,'f',3).arg(output->manufacturingDiameterMillimetres,0,'f',3);
    ManufacturingMeshResult result;result.error=ManufacturingMeshError::None;result.manufacturingMesh=output;result.diagnostic="Separate profile-driven ManufacturingMesh generated; Source and nominal PreparedMesh were not modified.";return result;
}
}
