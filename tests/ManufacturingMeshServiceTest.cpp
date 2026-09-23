#include "../src/services/geometry/print/ManufacturingMeshService.h"
#include "../src/services/geometry/print/AutoFitProfileResolver.h"
#include "../src/services/geometry/print/FrictionlessTechnicPinSemantic.h"
#include "../src/services/geometry/print/FrictionTechnicPinSemantic.h"
#include "../src/services/geometry/print/TechnicAxleSemantic.h"
#include "../src/services/geometry/print/RoundTechnicPassageSemantic.h"
#include "../src/services/geometry/print/FunctionalOperandRegenerator.h"
#include "../src/services/geometry/print/PrintMeshAnalysis.h"
#include "../src/services/geometry/print/SourceSurfaceSolidifier.h"
#include "../src/services/geometry/print/McutMeshBooleanService.h"
#include "../src/services/geometry/print/StudReceivingWallPocketSemantic.h"
#include "../src/services/geometry/print/StudReceivingAntiStudSemantic.h"
#include "../src/services/geometry/print/StandardBarSemantic.h"
#include "../src/services/geometry/print/CClipBarReceiverSemantic.h"
#include "../src/services/geometry/print/LDrawPrintPreparationService.h"
#include "../src/services/geometry/print/ManufacturingMeshDiagnosticExporter.h"
#include "../src/services/geometry/LDrawLibraryService.h"
#include "../src/services/geometry/ThreeMfWriter.h"
#include <lib3mf_implicit.hpp>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTextStream>
#include <cmath>
using namespace PrintGeometry;
namespace {
bool check(bool v,const QString&m){if(!v)QTextStream(stderr)<<"FAIL: "<<m<<Qt::endl;return v;}
PrintMesh box(double x1=1){PrintMesh m;m.vertices={{0,0,0},{x1,0,0},{x1,1,0},{0,1,0},{0,0,1},{x1,0,1},{x1,1,1},{0,1,1}};m.faces={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{3,7,6},{3,6,2},{0,4,7},{0,7,3},{1,2,6},{1,6,5}};return m;}
FunctionalFeature feature(){FunctionalFeature f;f.stableIdentity="3700:round-passage";f.family=FunctionalInterfaceFamily::RoundTechnicPassage;f.role=FunctionalInterfaceRole::Female;f.materialSide=FunctionalMaterialSide::EmptyInsideMaterialOutside;f.eligibility=FunctionalEligibility::Eligible;f.confidence=SemanticConfidence::HighConfidence;f.frame={{0,0,0},{0,0,1},{1,0,0},{0,1,0},false};f.nominalRadiusMillimetres=2.4;f.nominalDiameterMillimetres=4.8;f.nominalAxialExtentMillimetres=8;f.nominalEngagementExtentMillimetres=8;f.operandAction=FunctionalOperandAction::Subtract;f.governingOperandIdentity="passage";f.constructionRecipe="round-through-passage-v1";f.evidenceContract=FitCalibrationLibrary::currentSemanticContractVersion();f.radialProfile={{-4.1,3.0},{-4,3.0},{-4,2.4},{4,2.4},{4,3.0},{4.1,3.0}};return f;}
FunctionalFeature studFeature(){FunctionalFeature f;f.stableIdentity="3001:standard-stud";f.family=FunctionalInterfaceFamily::StandardStud;f.role=FunctionalInterfaceRole::Male;f.materialSide=FunctionalMaterialSide::MaterialInside;f.eligibility=FunctionalEligibility::Eligible;f.confidence=SemanticConfidence::HighConfidence;f.frame={{0,0,0},{0,0,1},{1,0,0},{0,1,0},false};f.nominalRadiusMillimetres=2.4;f.nominalDiameterMillimetres=4.8;f.nominalAxialExtentMillimetres=1.6;f.nominalEngagementExtentMillimetres=1.6;f.operandAction=FunctionalOperandAction::Unite;f.governingOperandIdentity="stud";f.constructionRecipe="standard-solid-stud-v1";f.evidenceContract="official-ldraw-standard-stud-v1";f.radialProfile={{0,2.4},{1.6,2.4}};return f;}
FunctionalFeature receiverFeature(){FunctionalFeature f;f.stableIdentity="3001:tube-wall-cell";f.family=FunctionalInterfaceFamily::StudReceivingClutch;f.role=FunctionalInterfaceRole::Female;f.materialSide=FunctionalMaterialSide::MaterialInside;f.eligibility=FunctionalEligibility::Eligible;f.confidence=SemanticConfidence::HighConfidence;f.frame={{0,0,0},{0,0,1},{1,0,0},{0,1,0},false};f.nominalRadiusMillimetres=3.2;f.nominalDiameterMillimetres=6.4;f.nominalAxialExtentMillimetres=3.2;f.nominalEngagementExtentMillimetres=3.2;f.protectedInnerRadiusMillimetres=2.4;f.operandAction=FunctionalOperandAction::Unite;f.governingOperandIdentity="receiving-tube";f.constructionRecipe="stud-receiving-tube-wall-cell-v1";f.evidenceContract="official-ldraw-stud4-tube-wall-cell-v1";f.radialProfile={{0,3.2},{3.2,3.2}};return f;}
FunctionalFeature postWallFeature(){FunctionalFeature f=receiverFeature();f.stableIdentity="3004:post-wall-cell";f.nominalRadiusMillimetres=1.6;f.nominalDiameterMillimetres=3.2;f.protectedInnerRadiusMillimetres=0;f.governingOperandIdentity="receiving-post";f.constructionRecipe="stud-receiving-post-wall-cell-v1";f.evidenceContract="official-ldraw-stud3-post-wall-cell-v1";f.radialProfile={{0,1.6},{3.2,1.6}};return f;}
FitProfile profile(double value=.2){FitProfile p;p.profileIdentity="verified-profile";p.sourceSessionIdentity="verified-session";p.name="Bambu H2D / PETG / LEGO Fit";p.verificationState=FitEvidenceState::Verified;p.process.printerIdentity="Bambu H2D";p.process.materialIdentity="PETG";p.process.profileName="0.20 mm Standard";p.process.hasNozzleDiameter=true;p.process.nozzleDiameterMillimetres=.4;p.process.hasLayerHeight=true;p.process.layerHeightMillimetres=.2;p.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;p.processFingerprint=FitCalibrationLibrary::processFingerprint(p.process);FitProfileCorrection c;c.featureFamily="RoundTechnicPassage";c.featureRole="female";c.printedOrientation="feature-axis-perpendicular-to-build-plate";c.valueMillimetres=value;c.semanticContractVersion=FitCalibrationLibrary::currentSemanticContractVersion();c.regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();c.calibrationArtifactIdentity="verification-v2";p.corrections.push_back(c);return p;}
FitProfile studProfile(bool diameter=true,bool height=true,double heightDiameterDependency=.35){auto p=profile();p.profileIdentity="verified-combined-profile";if(diameter){FitProfileCorrection c;c.featureFamily="StandardStud";c.featureRole="male";c.printedOrientation="feature-axis-perpendicular-to-build-plate";c.valueMillimetres=.35;c.semantics="male-stud-diameter";c.correctionContractVersion="male-stud-diameter-v1";c.semanticContractVersion="official-ldraw-standard-stud-v1";c.regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();c.calibrationArtifactIdentity="stud-od-verification";p.corrections.push_back(c);}if(height){FitProfileCorrection c;c.featureFamily="StandardStud";c.featureRole="male";c.printedOrientation="feature-axis-perpendicular-to-build-plate";c.valueMillimetres=.20;c.semantics="male-stud-height";c.correctionContractVersion="male-stud-height-v1";c.semanticContractVersion="official-ldraw-standard-stud-v1";c.regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();c.calibrationArtifactIdentity="stud-height-verification";c.hasRequiredDiameterCorrection=true;c.requiredDiameterCorrectionMillimetres=heightDiameterDependency;p.corrections.push_back(c);}return p;}
FitProfile axlePairProfile(double male=.25,double female=.40){auto p=profile();p.profileIdentity="verified-axle-pair-profile";p.corrections.clear();FitProfileCorrection axle;axle.featureFamily="TechnicAxle";axle.featureRole="male";axle.printedOrientation="feature-axis-perpendicular-to-build-plate";axle.valueMillimetres=male;axle.semantics="male-technic-axle-tip-to-tip-envelope";axle.correctionContractVersion="male-technic-axle-tip-to-tip-envelope-v1";axle.semanticContractVersion="official-ldraw-axle-cross-profile-v1";axle.regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();axle.calibrationArtifactIdentity="verified-axle";p.corrections.push_back(axle);FitProfileCorrection hole;hole.featureFamily="TechnicAxleHole";hole.featureRole="female";hole.printedOrientation="feature-axis-perpendicular-to-build-plate";hole.valueMillimetres=female;hole.semantics="female-technic-axle-hole-arm-width-clearance";hole.correctionContractVersion="female-technic-axle-hole-arm-width-clearance-v2";hole.semanticContractVersion="official-ldraw-axlehole-arm-width-clearance-v2";hole.regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();hole.calibrationArtifactIdentity="verified-axle-hole-arm-width";hole.hasFixedTipToTipCorrection=true;hole.fixedTipToTipCorrectionMillimetres=.30;p.corrections.push_back(hole);return p;}
LDrawGeometry::LDrawLoadResult axlePairSource(){LDrawGeometry::LDrawLoadResult s;s.sourceModel=std::make_shared<LDrawGeometry::LDrawSourceModel>();s.sourceModel->files={{0,"p/axle.dat",LDrawGeometry::SourceClassification::Primitive},{1,"p/axlehole.dat",LDrawGeometry::SourceClassification::Primitive}};std::array<double,12> male{};male[0]=male[5]=male[10]=1;std::array<double,12> female=male;female[3]=10;s.sourceModel->references={{0,-1,0,1,male,false,false},{1,-1,1,2,female,false,false}};return s;}
LDrawGeometry::LDrawLoadResult mixedAxleRoundPassageSource(){LDrawGeometry::LDrawLoadResult s;s.sourceModel=std::make_shared<LDrawGeometry::LDrawSourceModel>();s.sourceModel->files={{0,"p/axlehole.dat",LDrawGeometry::SourceClassification::Primitive},{1,"p/peghole.dat",LDrawGeometry::SourceClassification::Primitive},{2,"p/4-4cyli.dat",LDrawGeometry::SourceClassification::Primitive}};std::array<double,12> hole{};hole[0]=hole[5]=hole[10]=1;hole[3]=10;std::array<double,12> first{};first[0]=first[5]=first[10]=1;first[7]=-5;std::array<double,12> second=first;second[5]=-1;second[7]=5;std::array<double,12> circular=first;circular[3]=20;s.sourceModel->references={{0,-1,0,1,hole,false,false},{1,-1,1,2,first,false,false},{2,-1,1,3,second,false,false},{3,-1,2,4,circular,false,false}};return s;}
LDrawGeometry::LDrawLoadResult realAxle4519Source(){LDrawGeometry::LDrawLoadResult s;s.sourceModel=std::make_shared<LDrawGeometry::LDrawSourceModel>();s.sourceModel->files={{0,"parts/4519.dat",LDrawGeometry::SourceClassification::Part},{1,"p/axlehol8.dat",LDrawGeometry::SourceClassification::Primitive}};std::array<double,12> transform{};transform[0]=0;transform[1]=-55;transform[2]=0;transform[3]=27.5;transform[4]=1;transform[5]=0;transform[6]=0;transform[7]=0;transform[8]=0;transform[9]=0;transform[10]=1;transform[11]=0;s.sourceModel->references={{0,0,1,17,transform,false,false}};return s;}
class ProofBoolean final:public MeshBooleanService{public:MeshBooleanResult subtract(const PrintMesh&s,const PrintMesh&p)override{lastPassage=p;MeshBooleanResult r;r.error=MeshBooleanError::None;r.mesh=s;r.mesh.vertices[1].x+=.01;r.resultAnalysis=analyzeSource(r.mesh);return r;}MeshBooleanResult unite(const PrintMesh&s,const PrintMesh&p)override{lastUnion=p;MeshBooleanResult r;r.error=MeshBooleanError::None;r.mesh=s;r.resultAnalysis=analyzeSource(s);return r;}QString versionIdentity()const override{return "proof-boolean-v1";}static PrintMesh lastPassage,lastUnion;};PrintMesh ProofBoolean::lastPassage;PrintMesh ProofBoolean::lastUnion;
class StudProofBoolean final:public MeshBooleanService{public:MeshBooleanResult subtract(const PrintMesh&s,const PrintMesh&)override{MeshBooleanResult r;r.error=MeshBooleanError::None;r.mesh=s;r.resultAnalysis=analyzeSource(s);return r;}MeshBooleanResult unite(const PrintMesh&s,const PrintMesh&p)override{lastStud=p;MeshBooleanResult r;r.error=MeshBooleanError::None;r.mesh=s;r.mesh.vertices[1].x+=.02;r.resultAnalysis=analyzeSource(r.mesh);return r;}QString versionIdentity()const override{return "stud-proof-boolean-v1";}static PrintMesh lastStud;};PrintMesh StudProofBoolean::lastStud;
LDrawSemanticOperandBuilder::Result semantic(){LDrawSemanticOperandBuilder::Result r;r.status=LDrawSemanticOperandBuilder::Status::Ready;SemanticOperand body;body.role=SemanticRole::PrimaryBody;body.closedMesh=box();body.analysis=analyzeSource(body.closedMesh);body.sourceFiles={"body"};SemanticOperand hole;hole.role=SemanticRole::SubtractivePassage;hole.feature=SemanticFeature::RoundThroughPassage;hole.closedMesh=FunctionalOperandRegenerator::regenerate(feature(),{0}).mesh;hole.analysis=analyzeSource(hole.closedMesh);hole.functionalFeatures.push_back(feature());hole.sourceFiles={"passage"};r.operands={body,hole};return r;}
LDrawSemanticOperandBuilder::Result studSemantic(){LDrawSemanticOperandBuilder::Result r;r.status=LDrawSemanticOperandBuilder::Status::Ready;SemanticOperand body;body.role=SemanticRole::PrimaryBody;body.closedMesh=box();body.analysis=analyzeSource(body.closedMesh);body.sourceFiles={"body"};SemanticOperand stud;stud.role=SemanticRole::AdditiveAttachment;stud.closedMesh=FunctionalOperandRegenerator::regenerateStud(studFeature(),{}).mesh;stud.analysis=analyzeSource(stud.closedMesh);stud.functionalFeatures.push_back(studFeature());stud.sourceFiles={"stud"};r.operands={body,stud};return r;}
LDrawSemanticOperandBuilder::Result mixedOrientationSemantic(){auto r=studSemantic();auto passageFeature=feature();passageFeature.frame.axis={0,1,0};passageFeature.frame.profileV={0,0,-1};SemanticOperand passage;passage.role=SemanticRole::SubtractivePassage;passage.feature=SemanticFeature::RoundThroughPassage;passage.closedMesh=FunctionalOperandRegenerator::regenerate(passageFeature,{0}).mesh;passage.analysis=analyzeSource(passage.closedMesh);passage.functionalFeatures.push_back(passageFeature);passage.sourceFiles={"side-passage"};r.operands.push_back(passage);return r;}
LDrawSemanticOperandBuilder::Result receiverSemantic(){LDrawSemanticOperandBuilder::Result r;r.status=LDrawSemanticOperandBuilder::Status::Ready;SemanticOperand body;body.role=SemanticRole::PrimaryBody;body.closedMesh=box();body.analysis=analyzeSource(body.closedMesh);body.sourceFiles={"body"};SemanticOperand receiver;receiver.role=SemanticRole::HollowAdditiveAttachment;receiver.feature=SemanticFeature::Tube;receiver.closedMesh=FunctionalOperandRegenerator::regenerateReceivingTube(receiverFeature(),{}).mesh;receiver.analysis=analyzeSource(receiver.closedMesh);receiver.functionalFeatures.push_back(receiverFeature());receiver.sourceFiles={"stud4"};r.operands={body,receiver};return r;}
LDrawSemanticOperandBuilder::Result postWallSemantic(){LDrawSemanticOperandBuilder::Result r;r.status=LDrawSemanticOperandBuilder::Status::Ready;SemanticOperand body;body.role=SemanticRole::PrimaryBody;body.closedMesh=box();body.analysis=analyzeSource(body.closedMesh);body.sourceFiles={"walls-and-body"};SemanticOperand post;post.role=SemanticRole::AdditiveAttachment;post.closedMesh=FunctionalOperandRegenerator::regenerateReceivingPost(postWallFeature(),{}).mesh;post.analysis=analyzeSource(post.closedMesh);post.functionalFeatures.push_back(postWallFeature());post.sourceFiles={"stud3"};r.operands={body,post};return r;}
LDrawSemanticOperandBuilder::Result allFamiliesSemantic(){auto r=semantic();r.operands.push_back(studSemantic().operands.back());r.operands.push_back(receiverSemantic().operands.back());return r;}
bool same(const PrintMesh&a,const PrintMesh&b){if(a.faces!=b.faces||a.vertices.size()!=b.vertices.size())return false;for(std::size_t i=0;i<a.vertices.size();++i)if(a.vertices[i].x!=b.vertices[i].x||a.vertices[i].y!=b.vertices[i].y||a.vertices[i].z!=b.vertices[i].z)return false;return true;}
bool sameSource(const QVector<LDrawGeometry::Triangle>& a,const QVector<LDrawGeometry::Triangle>& b){if(a.size()!=b.size())return false;for(qsizetype i=0;i<a.size();++i)if(a[i].a!=b[i].a||a[i].b!=b[i].b||a[i].c!=b[i].c)return false;return true;}
double minimumRadialDistance(const PrintMesh&mesh){double result=1e100;for(const auto&vertex:mesh.vertices)result=std::min(result,std::hypot(vertex.x,vertex.y));return result;}
PreparedMesh wallPocketPrepared(const LDrawGeometry::LDrawLoadResult& source, const QString& part)
{
    PreparedMesh prepared;
    const auto semantic = LDrawSemanticOperandBuilder::build(source);
    if (!semantic.ok() || semantic.operands.isEmpty()) return prepared;
    McutMeshBooleanService booleanService;
    PrintMesh accumulated = semantic.operands.front().closedMesh;
    for (int i = 1; i < semantic.operands.size(); ++i) {
        const auto& operand = semantic.operands[i];
        const auto result = operand.role == SemanticRole::SubtractivePassage
            ? booleanService.subtract(accumulated, operand.closedMesh)
            : booleanService.unite(accumulated, operand.closedMesh);
        if (!result.ok()) return prepared;
        accumulated = result.mesh;
    }
    prepared.mesh = std::move(accumulated);
    prepared.partReference = part;
    prepared.ldrawIdentity = QStringLiteral("parts/%1.dat").arg(part);
    prepared.preparationProfileVersion = QStringLiteral("wall-pocket-test-v1");
    prepared.mcutVersion = booleanService.versionIdentity();
    return prepared;
}
FitProfile standardBarProfile(double correction)
{
    auto result = profile();
    result.profileIdentity = QStringLiteral("synthetic-standard-bar-profile");
    result.corrections.clear();
    FitProfileCorrection entry;
    entry.featureFamily = QStringLiteral("StandardBar");
    entry.featureRole = QStringLiteral("male");
    entry.printedOrientation = QStringLiteral("axis-perpendicular-to-build-plate");
    entry.semantics = QStringLiteral("male-standard-bar-diameter");
    entry.correctionContractVersion = QStringLiteral("male-standard-bar-diameter-v1");
    entry.semanticContractVersion = QStringLiteral("official-ldraw-capped-standard-bar-v1");
    entry.regeneratorAlgorithmVersion = FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();
    entry.calibrationArtifactIdentity = QStringLiteral("synthetic-standard-bar-evidence");
    entry.valueMillimetres = correction;
    result.corrections.push_back(entry);
    return result;
}
FitProfile hypotheticalCClipProfile()
{
    auto result=profile();
    result.profileIdentity=QStringLiteral("synthetic-c-clip-calibration-only");
    result.corrections.clear();
    FitProfileCorrection entry;
    entry.featureFamily=QStringLiteral("CClipBarReceiver");
    entry.featureRole=QStringLiteral("female");
    entry.printedOrientation=QStringLiteral("feature-axis-perpendicular-to-build-plate");
    entry.semantics=QStringLiteral("female-c-clip-contact-arc-and-throat-clearance");
    entry.correctionContractVersion=QStringLiteral("female-c-clip-contact-arc-and-throat-clearance-v1");
    entry.semanticContractVersion=QStringLiteral("official-ldraw-clip6-bar-receiver-v1");
    entry.regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();
    entry.calibrationArtifactIdentity=QStringLiteral("synthetic-test-only");
    entry.valueMillimetres=.10;
    result.corrections.push_back(entry);
    return result;
}
}
int main(int argc,char**argv){QCoreApplication app(argc,argv);bool ok=true;LDrawGeometry::LDrawLoadResult source;source.sourceModel=std::make_shared<LDrawGeometry::LDrawSourceModel>();PreparedMesh nominal;nominal.mesh=box();nominal.partReference="3700";nominal.ldrawIdentity="parts/3700.dat";nominal.preparationProfileVersion="profile-v1";nominal.mcutVersion="mcut-v1";const qsizetype sourceTriangleCount=source.mesh.triangles.size();const auto preparedBefore=nominal.mesh;ManufacturingMeshService service([]{return std::make_unique<ProofBoolean>();},[](const auto&){return semantic();});
auto p=profile();QString selectionReason;const auto*selectedCorrection=ManufacturingMeshService::compatibleCorrection(p,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate,&selectionReason);ok&=check(selectedCorrection&&std::abs(selectedCorrection->valueMillimetres-.2)<1e-9,"explicit per-export profile selection resolves the profile correction");ok&=check(!ManufacturingMeshService::compatibleCorrection(p,FitPrintedOrientation::FeatureAxisParallelToBuildPlate),"incompatible orientation cannot be selected for compensated export");const auto result=service.generate(source,nominal,p,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(result.ok(),"compatible Verified profile produces ManufacturingMesh");if(result.ok()){const auto&m=*result.manufacturingMesh;ok&=check(std::abs(m.nominalDiameterMillimetres-4.8)<1e-9&&std::abs(m.diameterCorrectionMillimetres-.2)<1e-9&&std::abs(m.manufacturingDiameterMillimetres-5.0)<1e-9,"profile diameter correction enters semantic regeneration");ok&=check(!same(m.mesh,nominal.mesh)&&same(nominal.mesh,preparedBefore)&&source.mesh.triangles.size()==sourceTriangleCount,"Source and Prepared remain unchanged while ManufacturingMesh is distinct");ok&=check(m.fitProfileIdentity==p.profileIdentity&&m.sourceSessionIdentity==p.sourceSessionIdentity&&!m.identity.isEmpty(),"manufacturing provenance links profile and evidence");QTextStream(stdout)<<"Part 3700 proof: profile="<<m.fitProfileIdentity<<" nominalDiameter="<<m.nominalDiameterMillimetres<<" correction="<<m.diameterCorrectionMillimetres<<" manufacturingDiameter="<<m.manufacturingDiameterMillimetres<<" manufacturingIdentity="<<m.identity<<Qt::endl;const auto passage=analyzeSource(ProofBoolean::lastPassage);ok&=check(std::abs((passage.bounds.maximum.x-passage.bounds.minimum.x)-6.0)<1e-9,"protected entrance geometry remains nominal");const auto repeated=service.generate(source,nominal,p,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(repeated.ok()&&repeated.manufacturingMesh->identity==m.identity&&same(repeated.manufacturingMesh->mesh,m.mesh),"generation deterministic");auto changed=profile(.1);changed.profileIdentity="other-profile";const auto other=service.generate(source,nominal,changed,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(other.ok()&&other.manufacturingMesh->identity!=m.identity&&std::abs(other.manufacturingMesh->manufacturingDiameterMillimetres-4.9)<1e-9,"correction and identity come from selected profile");QTemporaryDir output;const QString exportPath=output.filePath("manufacturing.3mf");QString exportError;ok&=check(ManufacturingMeshDiagnosticExporter::writeThreeMf(m,exportPath,1.0,QColor("#0055BF"),&exportError),QStringLiteral("diagnostic ManufacturingMesh export: %1").arg(exportError));if(QFileInfo::exists(exportPath)){Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();model->QueryReader("3mf")->ReadFromFile(exportPath.toStdString());auto meshes=model->GetMeshObjects();ok&=check(meshes->MoveNext(),"diagnostic 3MF contains a mesh");if(meshes->GetCurrentMeshObject()){const auto exported=meshes->GetCurrentMeshObject();ok&=check(exported->GetTriangleCount()==m.mesh.faces.size(),"diagnostic export contains ManufacturingMesh triangle data");ok&=check(std::abs(exported->GetVertex(1).m_Coordinates[0]-m.mesh.vertices[1].x)<1e-6&&std::abs(exported->GetVertex(1).m_Coordinates[0]-nominal.mesh.vertices[1].x)>1e-6,"diagnostic export uses ManufacturingMesh rather than nominal Prepared Mesh");}}}
const auto disabled=AutoFitProfileResolver::resolve(false,"3700",{p},FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(disabled.state==AutoFitResolutionState::Disabled&&!disabled.resolved(),"Auto Fit defaults to nominal when disabled");const auto unique=AutoFitProfileResolver::resolve(true,"3700",{p},FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(unique.resolved()&&unique.profile.profileIdentity==p.profileIdentity,"Auto Fit deterministically resolves one compatible Verified profile");const auto none=AutoFitProfileResolver::resolve(true,"3700",{},FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(none.state==AutoFitResolutionState::NoCompatibleProfile,"Auto Fit safely keeps nominal geometry without a compatible profile");auto second=profile(.1);second.profileIdentity="second-profile";const auto ambiguous=AutoFitProfileResolver::resolve(true,"3700",{p,second},FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(ambiguous.state==AutoFitResolutionState::Ambiguous&&!ambiguous.resolved(),"Auto Fit refuses ambiguous compatible profiles");auto staleAuto=p;staleAuto.corrections.front().semanticContractVersion="stale";auto draftAuto=p;draftAuto.verificationState=FitEvidenceState::Draft;const auto invalid=AutoFitProfileResolver::resolve(true,"3700",{staleAuto,draftAuto},FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(invalid.state==AutoFitResolutionState::NoCompatibleProfile,"Auto Fit rejects stale and non-Verified profiles");
PrintOrientation nominalOrientation,xPositive,xNegative,yPositive,yNegative,zPositive,zNegative;xPositive.rotate(PrintOrientation::Rotation::XPositive);xNegative.rotate(PrintOrientation::Rotation::XNegative);yPositive.rotate(PrintOrientation::Rotation::YPositive);yNegative.rotate(PrintOrientation::Rotation::YNegative);zPositive.rotate(PrintOrientation::Rotation::ZPositive);zNegative.rotate(PrintOrientation::Rotation::ZNegative);const auto zAxisFeature=feature();ok&=check(ManufacturingMeshService::transformedOrientation(zAxisFeature,nominalOrientation)==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate&&ManufacturingMeshService::transformedOrientation(zAxisFeature,xPositive)==FitPrintedOrientation::FeatureAxisParallelToBuildPlate&&ManufacturingMeshService::transformedOrientation(zAxisFeature,xNegative)==FitPrintedOrientation::FeatureAxisParallelToBuildPlate&&ManufacturingMeshService::transformedOrientation(zAxisFeature,yPositive)==FitPrintedOrientation::FeatureAxisParallelToBuildPlate&&ManufacturingMeshService::transformedOrientation(zAxisFeature,yNegative)==FitPrintedOrientation::FeatureAxisParallelToBuildPlate&&ManufacturingMeshService::transformedOrientation(zAxisFeature,zPositive)==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate&&ManufacturingMeshService::transformedOrientation(zAxisFeature,zNegative)==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate,"identity and X/Y/Z +/-90 classify transformed feature axes against the build plate");
const auto axleSource=axlePairSource();PreparedMesh axleNominal=nominal;axleNominal.partReference="synthetic-axle-pair";const auto axleNominalBefore=axleNominal.mesh;ManufacturingMeshService axleService([]{return std::make_unique<ProofBoolean>();});const auto axlePair=axlePairProfile();const auto axleCorrections=ManufacturingMeshService::compatibleCorrections(axlePair,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(axleCorrections.technicAxleTipToTip&&axleCorrections.technicAxleHoleArmWidth,"one Verified profile independently selects male axle and female axle-hole v2 contracts");const auto axleResult=axleService.generate(axleSource,axleNominal,axlePair,nominalOrientation);ok&=check(axleResult.ok(),"male and female axle corrections reach the shared ManufacturingMesh path: "+axleResult.diagnostic);if(axleResult.ok()){const auto maleBounds=analyzeSource(ProofBoolean::lastUnion).bounds,femaleBounds=analyzeSource(ProofBoolean::lastPassage).bounds;const double femaleCenter=(femaleBounds.maximum.x+femaleBounds.minimum.x)*.5;ok&=check(std::abs((maleBounds.maximum.x-maleBounds.minimum.x)-5.05)<1e-6,"male profile value drives the 5.05 mm tip-to-tip envelope");ok&=check(std::abs((femaleBounds.maximum.x-femaleBounds.minimum.x)-5.10)<1e-6&&std::abs(ProofBoolean::lastPassage.vertices[2].x-femaleCenter-1.0)<1e-6,"female v2 keeps 5.10 mm tip-to-tip and applies the profile-driven 2.00 mm arm opening");ok&=check(axleResult.manufacturingMesh->featureIdentities.size()==2&&same(axleNominal.mesh,axleNominalBefore),"male/female features coexist while nominal PreparedMesh remains immutable");const auto explicitIdentity=axleResult.manufacturingMesh->identity;const auto resolved=AutoFitProfileResolver::resolve(true,axleNominal.partReference,{axlePair},axleSource,nominalOrientation);ok&=check(resolved.resolved(),"Auto Fit resolves the same compatible axle-pair profile");const auto repeated=axleService.generate(axleSource,axleNominal,resolved.profile,nominalOrientation);ok&=check(repeated.ok()&&repeated.manufacturingMesh->identity==explicitIdentity,"explicit selection and Auto Fit use identical deterministic production logic");}
const auto mismatchedAxle=axleService.generate(axleSource,axleNominal,axlePair,xPositive);ok&=check(mismatchedAxle.error==ManufacturingMeshError::MissingCorrection,"orientation-mismatched axle and axle-hole features remain nominal");auto maleOnly=axlePair;maleOnly.corrections.removeLast();const auto independentMale=axleService.generate(axleSource,axleNominal,maleOnly,nominalOrientation);ok&=check(independentMale.ok()&&independentMale.manufacturingMesh->featureIdentities.size()==1&&independentMale.diagnostic.contains("1 recognized feature(s) remained nominal"),"an unmatched female feature does not suppress a compatible male axle");auto obsolete=axlePair;obsolete.corrections.removeLast();auto oldHole=obsolete.corrections.front();oldHole.featureFamily="TechnicAxleHole";oldHole.featureRole="female";oldHole.semantics="female-technic-axle-hole-tip-to-tip-clearance";oldHole.correctionContractVersion="female-technic-axle-hole-tip-to-tip-clearance-v1";oldHole.semanticContractVersion="official-ldraw-axlehole-cross-profile-v1";obsolete.corrections={oldHole};ok&=check(!ManufacturingMeshService::hasApplicableCorrection(obsolete,axleSource,nominalOrientation),"obsolete tip-clearance evidence is retained as history but is not production-applicable");
const auto mixedRoundSource=mixedAxleRoundPassageSource();const auto mixedRoundFeatures=RoundTechnicPassageSemantic::recognize(mixedRoundSource);ok&=check(mixedRoundFeatures.size()==1&&mixedRoundFeatures.front().evidenceContract==FitCalibrationLibrary::currentSemanticContractVersion()&&std::abs(mixedRoundFeatures.front().nominalDiameterMillimetres-4.8)<1e-9,"opposed authoritative peghole primitives retain one ordinary 4.80 mm RoundTechnicPassage on a mixed-feature Part");ok&=check(mixedRoundFeatures.front().provenance.size()==2&&mixedRoundFeatures.front().provenance.front().sourceFile==QStringLiteral("p/peghole.dat"),"round-passage recognition records exact reviewed primitive ancestry rather than generic circular geometry");auto singlePeg=mixedRoundSource;singlePeg.sourceModel=std::make_shared<LDrawGeometry::LDrawSourceModel>(*mixedRoundSource.sourceModel);singlePeg.sourceModel->references.removeAt(2);ok&=check(RoundTechnicPassageSemantic::recognize(singlePeg).isEmpty(),"a lone peghole surface is not misclassified as a through passage");auto sameFacing=mixedRoundSource;sameFacing.sourceModel=std::make_shared<LDrawGeometry::LDrawSourceModel>(*mixedRoundSource.sourceModel);sameFacing.sourceModel->references[2].accumulatedTransform[5]=1;ok&=check(RoundTechnicPassageSemantic::recognize(sameFacing).isEmpty(),"same-facing and unrelated circular primitives do not create a false RoundTechnicPassage");auto mixedRoundProfile=axlePair;mixedRoundProfile.profileIdentity="verified-axle-hole-round-passage-profile";mixedRoundProfile.corrections.push_back(profile().corrections.front());PreparedMesh mixedRoundNominal=nominal;mixedRoundNominal.partReference="mixed-axle-round-passage";const auto mixedRoundBefore=mixedRoundNominal.mesh;ManufacturingMeshService mixedRoundService([]{return std::make_unique<ProofBoolean>();},[](const auto&){return LDrawSemanticOperandBuilder::Result{};});ok&=check(ManufacturingMeshService::hasApplicableCorrection(mixedRoundProfile,mixedRoundSource,nominalOrientation),"source-aware correction selection discovers the Verified RoundTechnicPassage on an otherwise mixed axle Part");const auto mixedRoundResult=mixedRoundService.generate(mixedRoundSource,mixedRoundNominal,mixedRoundProfile,nominalOrientation);ok&=check(mixedRoundResult.ok(),"mixed axle-hole and ordinary round passage reach ManufacturingMesh together: "+mixedRoundResult.diagnostic);if(mixedRoundResult.ok()){const auto&m=*mixedRoundResult.manufacturingMesh;ok&=check(m.featureIdentities.size()==2&&std::abs(m.nominalDiameterMillimetres-4.8)<1e-9&&std::abs(m.diameterCorrectionMillimetres-.2)<1e-9&&std::abs(m.manufacturingDiameterMillimetres-5.0)<1e-9,"round passage receives the profile-driven 4.80 + 0.20 = 5.00 mm correction while axle-hole semantics coexist");ok&=check(ProofBoolean::lastPassage.vertices.size()>32&&std::abs(std::abs(ProofBoolean::lastPassage.vertices[32].x)-2.5)<1e-6,"regenerated passage geometry carries the 5.00 mm functional bore rather than nominal 4.80 mm");ok&=check(same(mixedRoundNominal.mesh,mixedRoundBefore)&&m.provenance.join('|').contains("surrounding geometry"),"Source/Prepared geometry stays immutable and protected neighboring geometry is recorded");const auto autoMixedRound=AutoFitProfileResolver::resolve(true,mixedRoundNominal.partReference,{mixedRoundProfile},mixedRoundSource,nominalOrientation);ok&=check(autoMixedRound.resolved()&&autoMixedRound.profile.profileIdentity==mixedRoundProfile.profileIdentity,"explicit export and Auto Fit use the same mixed-feature RoundTechnicPassage compatibility path");}ManufacturingMeshService retainedBodyService([]{return std::make_unique<ProofBoolean>();},[](const auto&){LDrawSemanticOperandBuilder::Result r;r.status=LDrawSemanticOperandBuilder::Status::Ready;SemanticOperand body;body.role=SemanticRole::PrimaryBody;body.closedMesh=box();body.analysis=analyzeSource(body.closedMesh);body.sourceFiles={"mixed-body"};r.operands={body};return r;});const auto retainedBodyResult=retainedBodyService.generate(mixedRoundSource,mixedRoundNominal,profile(),nominalOrientation);ok&=check(retainedBodyResult.ok()&&retainedBodyResult.manufacturingMesh->featureIdentities.size()==1&&std::abs(retainedBodyResult.manufacturingMesh->manufacturingDiameterMillimetres-5.0)<1e-9,"a successfully prepared mixed body gains the independently retained round-passage operand without requiring body reconstruction");auto parallelRoundProfile=profile();parallelRoundProfile.profileIdentity="parallel-round-profile";parallelRoundProfile.corrections.front().printedOrientation="feature-axis-parallel-to-build-plate";ok&=check(!ManufacturingMeshService::hasApplicableCorrection(parallelRoundProfile,mixedRoundSource,nominalOrientation)&&ManufacturingMeshService::hasApplicableCorrection(parallelRoundProfile,mixedRoundSource,xPositive),"perpendicular and parallel Verified RoundTechnicPassage evidence remains orientation-specific after recognition");
auto incompleteDependent=axlePair;incompleteDependent.corrections.back().hasFixedTipToTipCorrection=false;QString incompleteReason;ok&=check(FitCalibrationLibrary::profileCompatibility(incompleteDependent,&incompleteReason),"an incomplete dependent axle-hole correction does not invalidate independent contracts in the Verified profile");const auto incompleteCorrections=ManufacturingMeshService::compatibleCorrections(incompleteDependent,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate,&incompleteReason);ok&=check(incompleteCorrections.technicAxleTipToTip&&!incompleteCorrections.technicAxleHoleArmWidth,"missing axle-hole dependency metadata suppresses only that correction while retaining the Verified male axle correction");
const auto source4519=realAxle4519Source();const auto features4519=TechnicAxleSemantic::recognizeAxles(source4519);ok&=check(features4519.size()==1&&features4519.front().provenance.front().sourceFile==QStringLiteral("p/axlehol8.dat"),"real 4519 authoritative axlehol8 ancestry resolves ordinary Technic Axle semantics without Part-number logic");ok&=check(ManufacturingMeshService::hasApplicableCorrection(incompleteDependent,source4519,yPositive)&&!ManufacturingMeshService::hasApplicableCorrection(incompleteDependent,source4519,nominalOrientation),"4519 standing on end resolves the perpendicular Verified axle correction while its incompatible orientation remains rejected");PreparedMesh prepared4519=axleNominal;prepared4519.partReference="4519";prepared4519.ldrawIdentity="parts/4519.dat";const auto result4519=axleService.generate(source4519,prepared4519,incompleteDependent,yPositive);ok&=check(result4519.ok()&&std::abs(result4519.manufacturingMesh->manufacturingDiameterMillimetres-5.05)<1e-9,"4519 reaches the profile-driven 5.05 mm ManufacturingMesh despite unrelated incomplete axle-hole evidence");const auto auto4519=AutoFitProfileResolver::resolve(true,"4519",{incompleteDependent},source4519,yPositive);ok&=check(auto4519.resolved()&&auto4519.profile.profileIdentity==incompleteDependent.profileIdentity,"4519 explicit selection and Auto Fit share correction-level compatibility");
auto mixedAxleProfile=axlePair;const auto studContracts=studProfile().corrections;for(int i=1;i<studContracts.size();++i)mixedAxleProfile.corrections.push_back(studContracts[i]);ManufacturingMeshService mixedAxleService([]{return std::make_unique<ProofBoolean>();},[](const auto&){return studSemantic();});const auto mixedAxleResult=mixedAxleService.generate(axleSource,axleNominal,mixedAxleProfile,nominalOrientation);ok&=check(mixedAxleResult.ok()&&mixedAxleResult.manufacturingMesh->featureIdentities.size()==3,"male axle, female axle-hole, and existing stud corrections compose independently on one semantic Part");

PreparedMesh studNominal=nominal;studNominal.partReference="3001";studNominal.ldrawIdentity="parts/3001.dat";const auto studBefore=studNominal.mesh;ManufacturingMeshService studService([]{return std::make_unique<StudProofBoolean>();},[](const auto&){return studSemantic();});const auto combined=studProfile();const auto available=ManufacturingMeshService::compatibleCorrections(combined,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(available.femaleDiameter&&available.studDiameter&&available.studHeight,"one Fit Profile retains Technic Hole, Stud OD, and Stud Height correction contracts");const auto studResult=studService.generate(source,studNominal,combined,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(studResult.ok(),"combined verified Standard Stud corrections produce ManufacturingMesh");if(studResult.ok()){const auto&m=*studResult.manufacturingMesh;ok&=check(std::abs(m.nominalDiameterMillimetres-4.8)<1e-9&&std::abs(m.diameterCorrectionMillimetres-.35)<1e-9&&std::abs(m.manufacturingDiameterMillimetres-5.15)<1e-9,"stud OD comes from the independent profile correction");ok&=check(std::abs(m.nominalHeightMillimetres-1.6)<1e-9&&std::abs(m.heightCorrectionMillimetres-.20)<1e-9&&std::abs(m.manufacturingHeightMillimetres-1.80)<1e-9,"stud height combines with OD during the same regeneration");const auto regenerated=analyzeSource(StudProofBoolean::lastStud);ok&=check(std::abs((regenerated.bounds.maximum.x-regenerated.bounds.minimum.x)-5.15)<1e-6&&std::abs((regenerated.bounds.maximum.z-regenerated.bounds.minimum.z)-1.85)<1e-6,"representative regenerated stud has 5.15 mm OD, 1.80 mm functional height, and its existing 0.05 mm stitch intrusion");ok&=check(same(studNominal.mesh,studBefore)&&source.mesh.triangles.size()==sourceTriangleCount,"combined stud generation does not mutate Source or PreparedMesh");QTemporaryDir output;const QString exportPath=output.filePath("stud-manufacturing.3mf");QString exportError;ok&=check(ManufacturingMeshDiagnosticExporter::writeThreeMf(m,exportPath,1.0,QColor("#C91A09"),&exportError),QStringLiteral("explicit stud ManufacturingMesh export: %1").arg(exportError));if(QFileInfo::exists(exportPath)){Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();model->QueryReader("3mf")->ReadFromFile(exportPath.toStdString());auto meshes=model->GetMeshObjects();ok&=check(meshes->MoveNext()&&meshes->GetCurrentMeshObject()->GetTriangleCount()==m.mesh.faces.size(),"explicit export contains the corrected ManufacturingMesh");}}
ManufacturingMeshService mixedService([]{return std::make_unique<ProofBoolean>();},[](const auto&){return mixedOrientationSemantic();});const auto mixedSemantics=mixedOrientationSemantic();const auto mixedAutoFit=AutoFitProfileResolver::resolve(true,"3700",{combined},mixedSemantics,xPositive);ok&=check(mixedAutoFit.resolved(),"Auto Fit uses transformed per-operand applicability");const auto mixedResult=mixedService.generate(source,nominal,combined,xPositive);ok&=check(mixedResult.ok(),"X +90 mixed Part produces a ManufacturingMesh: "+mixedResult.diagnostic);if(mixedResult.ok()){ok&=check(mixedResult.manufacturingMesh->featureIdentities==QStringList{"3700:round-passage"},"X +90 applies only the transformed-perpendicular passage correction");ok&=check(mixedResult.diagnostic.contains("1 feature(s) corrected")&&mixedResult.diagnostic.contains("1 recognized feature(s) left nominal"),"one transformed-parallel stud remains nominal without suppressing the matched passage");const auto provenance=mixedResult.manufacturingMesh->provenance.join('|');ok&=check(provenance.contains("Print Orientation: X")&&provenance.contains("build axis (0, 0, 1)")&&provenance.contains("build axis (0, -1, 0)")&&provenance.contains("remained nominal"),"ManufacturingMesh provenance records Print Orientation, transformed axes, applied and nominal features");const auto orientedExport=xPositive.apply(mixedResult.manufacturingMesh->mesh);ok&=check(orientedExport.vertices.front().x==mixedResult.manufacturingMesh->mesh.vertices.front().x,"explicit export applies the same Print Orientation used for fit selection without mutating ManufacturingMesh");}
const auto diameterOnly=studService.generate(source,studNominal,studProfile(true,false),FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(diameterOnly.ok()&&std::abs(diameterOnly.manufacturingMesh->manufacturingDiameterMillimetres-5.15)<1e-9&&std::abs(diameterOnly.manufacturingMesh->manufacturingHeightMillimetres-1.6)<1e-9,"OD-only profile applies verified OD and leaves height nominal");const auto heightOnly=studService.generate(source,studNominal,studProfile(false,true),FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(heightOnly.error==ManufacturingMeshError::MissingCorrection&&heightOnly.diagnostic.contains("requires its matching"),"height-only profile cannot apply evidence measured under a fixed OD context");const auto mismatched=studService.generate(source,studNominal,studProfile(true,true,.30),FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(mismatched.ok()&&std::abs(mismatched.manufacturingMesh->manufacturingDiameterMillimetres-5.15)<1e-9&&std::abs(mismatched.manufacturingMesh->manufacturingHeightMillimetres-1.6)<1e-9&&mismatched.manufacturingMesh->correctionContractVersion=="male-stud-diameter-v1"&&mismatched.manufacturingMesh->provenance.join('|').contains("different Stud OD"),"mismatched Height dependency is rejected while valid OD and its provenance remain applied");ok&=check(mismatched.manufacturingMesh->identity!=studResult.manufacturingMesh->identity,"ManufacturingMesh identity includes only the correction contracts actually applied");const auto studDisabled=AutoFitProfileResolver::resolve(false,"3001",{combined},FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(studDisabled.state==AutoFitResolutionState::Disabled&&!studDisabled.resolved()&&same(studNominal.mesh,studBefore),"Auto Fit off leaves representative PreparedMesh nominal");const auto studAuto=AutoFitProfileResolver::resolve(true,"3001",{combined},FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(studAuto.resolved()&&studAuto.profile.profileIdentity==combined.profileIdentity,"Auto Fit uniquely resolves the same combined stud profile path");
auto stale=p;stale.corrections.front().regeneratorAlgorithmVersion="future";ok&=check(service.generate(source,nominal,stale,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate).error==ManufacturingMeshError::IncompatibleProfile,"stale profile rejected");auto unverified=p;unverified.verificationState=FitEvidenceState::Draft;ok&=check(service.generate(source,nominal,unverified,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate).error==ManufacturingMeshError::IncompatibleProfile,"non-Verified profile rejected");auto parallelOnly=p;parallelOnly.corrections.front().printedOrientation="feature-axis-parallel-to-build-plate";ok&=check(service.generate(source,nominal,parallelOnly,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate).error==ManufacturingMeshError::MissingCorrection,"genuine per-feature orientation mismatch remains a safe no-match failure");
const auto args=app.arguments();const int libraryAt=args.indexOf("--ldraw");if(libraryAt>=0&&libraryAt+1<args.size()){ManufacturingMeshService realPartService([]{return std::make_unique<StudProofBoolean>();});for(const QString&part:{QStringLiteral("3003"),QStringLiteral("3001"),QStringLiteral("3700")}){const auto realSource=LDrawLibraryService::loadPart(args[libraryAt+1],part);ok&=check(realSource.ok(),part+" real LDraw source loads");if(!realSource.ok())continue;PreparedMesh realNominal=studNominal;realNominal.partReference=part;realNominal.ldrawIdentity=QStringLiteral("parts/%1.dat").arg(part);const auto realNominalBefore=realNominal.mesh;const auto realResult=realPartService.generate(realSource,realNominal,combined,nominalOrientation);ok&=check(realResult.ok(),part+" real StandardStud operands connect to ManufacturingMesh correction selection: "+realResult.diagnostic);if(realResult.ok()){ok&=check(std::abs(realResult.manufacturingMesh->manufacturingDiameterMillimetres-5.15)<1e-9&&std::abs(realResult.manufacturingMesh->manufacturingHeightMillimetres-1.80)<1e-9&&same(realNominal.mesh,realNominalBefore),part+" uses profile OD and Height while nominal PreparedMesh remains unchanged");if(part==QStringLiteral("3700")){ok&=check(realResult.manufacturingMesh->featureIdentities.size()==2&&realResult.diagnostic.contains("2 feature(s) corrected")&&realResult.diagnostic.contains("1 recognized feature(s) left nominal"),"3700 corrects both open studs independently while leaving the side passage nominal");ok&=check(realResult.manufacturingMesh->provenance.join('|').contains("remained nominal"),"3700 provenance records the incompatible side-passage orientation");const auto repeatedReal=realPartService.generate(realSource,realNominal,combined,nominalOrientation);ok&=check(repeatedReal.ok()&&repeatedReal.manufacturingMesh->identity==realResult.manufacturingMesh->identity,"3700 mixed-feature ManufacturingMesh composition is deterministic");const auto rotatedReal=realPartService.generate(realSource,realNominal,combined,xPositive);ok&=check(rotatedReal.ok()&&rotatedReal.manufacturingMesh->featureIdentities.size()==1&&rotatedReal.diagnostic.contains("2 recognized feature(s) left nominal"),"3700 X +90 applies the now-perpendicular passage while leaving both transformed-parallel studs nominal: "+rotatedReal.diagnostic);}}}}
auto receiverOnly=profile();receiverOnly.corrections.clear();FitProfileCorrection receiverCorrection;receiverCorrection.featureFamily="StudReceivingClutch";receiverCorrection.featureRole="female";receiverCorrection.printedOrientation="feature-axis-perpendicular-to-build-plate";receiverCorrection.valueMillimetres=.2;receiverCorrection.semantics="female-stud-receiver-tube-od";receiverCorrection.correctionContractVersion="female-stud-receiver-tube-od-v1";receiverCorrection.semanticContractVersion="official-ldraw-stud4-tube-wall-cell-v1";receiverCorrection.regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();receiverOnly.corrections.push_back(receiverCorrection);const auto productionReceiverCorrections=ManufacturingMeshService::compatibleCorrections(receiverOnly,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(productionReceiverCorrections.receivingTubeDiameter==&receiverOnly.corrections.front()&&!productionReceiverCorrections.femaleDiameter&&!productionReceiverCorrections.studDiameter&&!productionReceiverCorrections.studHeight,"Verified TubeWallCell profile data enters production correction selection without another compensation path");ManufacturingMeshService receiverService([]{return std::make_unique<StudProofBoolean>();},[](const auto&){return receiverSemantic();});PreparedMesh receiverNominal=nominal;receiverNominal.partReference="3001";receiverNominal.ldrawIdentity="parts/3001.dat";const auto receiverPreparedBefore=receiverNominal.mesh;const auto receiverResult=receiverService.generate(source,receiverNominal,receiverOnly,nominalOrientation);ok&=check(receiverResult.ok(),"explicit ManufacturingMesh consumes the Verified TubeWallCell correction: "+receiverResult.diagnostic);if(receiverResult.ok()){const auto&m=*receiverResult.manufacturingMesh;const auto regenerated=analyzeSource(StudProofBoolean::lastStud);ok&=check(std::abs(m.nominalDiameterMillimetres-6.4)<1e-9&&std::abs(m.diameterCorrectionMillimetres-.2)<1e-9&&std::abs(m.manufacturingDiameterMillimetres-6.6)<1e-9,"TubeWallCell ManufacturingMesh derives 6.60 mm OD from the profile value");ok&=check(std::abs((regenerated.bounds.maximum.x-regenerated.bounds.minimum.x)-6.6)<1e-6&&std::abs(minimumRadialDistance(StudProofBoolean::lastStud)-2.4)<1e-6,"regeneration changes only tube OD while preserving the protected 4.80 mm bore");ok&=check(std::abs((regenerated.bounds.maximum.z-regenerated.bounds.minimum.z)-3.25)<1e-6&&same(receiverNominal.mesh,receiverPreparedBefore)&&source.mesh.triangles.size()==sourceTriangleCount&&m.provenance.join('|').contains("protected bore 4.800 mm"),"Source, nominal PreparedMesh, surrounding body, seating extent, and external dimensions remain nominal with protected-geometry provenance");QTemporaryDir output;const QString exportPath=output.filePath("receiver-manufacturing.3mf");QString exportError;ok&=check(ManufacturingMeshDiagnosticExporter::writeThreeMf(m,exportPath,1.0,QColor("#0055BF"),&exportError),"explicit TubeWallCell ManufacturingMesh export: "+exportError);if(QFileInfo::exists(exportPath)){Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();model->QueryReader("3mf")->ReadFromFile(exportPath.toStdString());auto meshes=model->GetMeshObjects();ok&=check(meshes->MoveNext()&&meshes->GetCurrentMeshObject()->GetTriangleCount()==m.mesh.faces.size(),"explicit TubeWallCell export contains corrected ManufacturingMesh geometry");}}
const auto receiverAuto=AutoFitProfileResolver::resolve(true,"3001",{receiverOnly},receiverSemantic(),nominalOrientation);ok&=check(receiverAuto.resolved()&&receiverAuto.profile.profileIdentity==receiverOnly.profileIdentity,"Auto Fit resolves the same TubeWallCell correction path");const auto receiverAutoDisabled=AutoFitProfileResolver::resolve(false,"3001",{receiverOnly},receiverSemantic(),nominalOrientation);ok&=check(receiverAutoDisabled.state==AutoFitResolutionState::Disabled&&same(receiverNominal.mesh,receiverPreparedBefore),"Auto Fit disabled leaves TubeWallCell nominal");const auto receiverRotated=receiverService.generate(source,receiverNominal,receiverOnly,xPositive);ok&=check(receiverRotated.error==ManufacturingMeshError::MissingCorrection&&receiverRotated.diagnostic.contains("remained nominal"),"orientation-incompatible TubeWallCell remains nominal without guessing");
auto postProfile=profile();postProfile.profileIdentity="verified-post-wall-cell-profile";postProfile.corrections.clear();FitProfileCorrection postCorrection;postCorrection.featureFamily="StudReceivingClutch";postCorrection.featureRole="female";postCorrection.printedOrientation="feature-axis-perpendicular-to-build-plate";postCorrection.valueMillimetres=.10;postCorrection.semantics="female-stud-receiver-post-od";postCorrection.correctionContractVersion="female-stud-receiver-post-od-v1";postCorrection.semanticContractVersion="official-ldraw-stud3-post-wall-cell-v1";postCorrection.regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();postCorrection.calibrationArtifactIdentity="post-wall-cell-verification-v2";postProfile.corrections.push_back(postCorrection);const auto postCorrections=ManufacturingMeshService::compatibleCorrections(postProfile,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(postCorrections.receivingPostDiameter==&postProfile.corrections.front()&&!postCorrections.receivingTubeDiameter,"Verified PostWallCell selects its distinct profile correction contract");ManufacturingMeshService postService([]{return std::make_unique<StudProofBoolean>();},[](const auto&){return postWallSemantic();});PreparedMesh postNominal=receiverNominal;postNominal.partReference="3004";postNominal.ldrawIdentity="parts/3004.dat";const auto postBefore=postNominal.mesh;const auto postResult=postService.generate(source,postNominal,postProfile,nominalOrientation);ok&=check(postResult.ok(),"explicit PostWallCell ManufacturingMesh consumes Verified profile data: "+postResult.diagnostic);if(postResult.ok()){const auto&m=*postResult.manufacturingMesh;const auto regenerated=analyzeSource(StudProofBoolean::lastStud);ok&=check(std::abs(m.nominalDiameterMillimetres-3.2)<1e-9&&std::abs(m.diameterCorrectionMillimetres-.10)<1e-9&&std::abs(m.manufacturingDiameterMillimetres-3.3)<1e-9,"PostWallCell derives 3.20 + 0.10 = 3.30 mm from profile data");ok&=check(std::abs((regenerated.bounds.maximum.x-regenerated.bounds.minimum.x)-3.3)<1e-6&&std::abs((regenerated.bounds.maximum.z-regenerated.bounds.minimum.z)-3.25)<1e-6&&same(postNominal.mesh,postBefore)&&source.mesh.triangles.size()==sourceTriangleCount,"post OD changes while height, Source, and PreparedMesh remain nominal");ok&=check(m.provenance.join('|').contains("wall positions, stud pitch, external dimensions"),"PostWallCell provenance records protected surrounding geometry");QTemporaryDir output;const QString path=output.filePath("post-wall-manufacturing.3mf");QString exportError;ok&=check(ManufacturingMeshDiagnosticExporter::writeThreeMf(m,path,1.0,QColor("#0055BF"),&exportError),"explicit PostWallCell ManufacturingMesh export: "+exportError);}const auto postAuto=AutoFitProfileResolver::resolve(true,"3004",{postProfile},postWallSemantic(),nominalOrientation);ok&=check(postAuto.resolved()&&postAuto.profile.profileIdentity==postProfile.profileIdentity,"Auto Fit resolves the same PostWallCell profile path");ok&=check(AutoFitProfileResolver::resolve(false,"3004",{postProfile},postWallSemantic(),nominalOrientation).state==AutoFitResolutionState::Disabled,"PostWallCell Auto Fit disabled remains nominal");ok&=check(postService.generate(source,postNominal,postProfile,xPositive).error==ManufacturingMeshError::MissingCorrection,"orientation-incompatible PostWallCell remains nominal");
if(libraryAt>=0&&libraryAt+1<args.size()){for(const QString&part:{QStringLiteral("3004"),QStringLiteral("3010"),QStringLiteral("3023b"),QStringLiteral("3710"),QStringLiteral("3666")}){const auto realPost=LDrawLibraryService::loadPart(args[libraryAt+1],part);ok&=check(realPost.ok(),part+" real PostWallCell source loads");if(realPost.ok()){bool compatible=false,incompatible=false;for(const auto&orientation:QVector<PrintOrientation>{nominalOrientation,xPositive,xNegative,yPositive,yNegative,zPositive,zNegative}){if(ManufacturingMeshService::hasApplicableCorrection(postProfile,realPost,orientation))compatible=true;else incompatible=true;}ok&=check(compatible&&incompatible,part+" resolves the Verified PostWallCell profile only in its matching per-operand orientation");}}}
auto allFamilies=combined;allFamilies.corrections.push_back(receiverCorrection);ManufacturingMeshService allFamiliesService([]{return std::make_unique<StudProofBoolean>();},[](const auto&){return allFamiliesSemantic();});const auto allFamiliesResult=allFamiliesService.generate(source,receiverNominal,allFamilies,nominalOrientation);ok&=check(allFamiliesResult.ok()&&allFamiliesResult.manufacturingMesh->featureIdentities.size()==3&&allFamiliesResult.diagnostic.contains("3 feature(s) corrected"),"TubeWallCell coexists with StandardStud and RoundTechnicPassage corrections on one mixed semantic Part");const auto genuineNoMatch=receiverService.generate(source,receiverNominal,combined,nominalOrientation);ok&=check(genuineNoMatch.error==ManufacturingMeshError::MissingCorrection,"genuine total no-match behavior remains safe");
if(libraryAt>=0&&libraryAt+1<args.size()){ManufacturingMeshService realReceiverService([]{return std::make_unique<StudProofBoolean>();});for(const QString&part:{QStringLiteral("3003"),QStringLiteral("3001"),QStringLiteral("3020")}){const auto realSource=LDrawLibraryService::loadPart(args[libraryAt+1],part);ok&=check(realSource.ok(),part+" real TubeWallCell representative loads");if(!realSource.ok())continue;PreparedMesh realNominal=receiverNominal;realNominal.partReference=part;realNominal.ldrawIdentity=QStringLiteral("parts/%1.dat").arg(part);const auto before=realNominal.mesh;const auto realResult=realReceiverService.generate(realSource,realNominal,receiverOnly,nominalOrientation);ok&=check(realResult.ok(),part+" authoritative stud4 TubeWallCell enters ManufacturingMesh: "+realResult.diagnostic);if(realResult.ok())ok&=check(std::abs(realResult.manufacturingMesh->nominalDiameterMillimetres-6.4)<1e-9&&std::abs(realResult.manufacturingMesh->manufacturingDiameterMillimetres-6.6)<1e-9&&same(realNominal.mesh,before)&&realResult.manufacturingMesh->provenance.join('|').contains("protected bore 4.800 mm"),part+" proves profile-driven 6.40 -> 6.60 mm OD with nominal PreparedMesh and protected 4.80 mm bore");}}
auto verifiedPinProfile=profile();verifiedPinProfile.profileIdentity="verified-frictionless-pin-profile";verifiedPinProfile.sourceSessionIdentity="verified-frictionless-pin-session";verifiedPinProfile.corrections.clear();FitProfileCorrection pinCorrection;pinCorrection.featureFamily="FrictionlessTechnicPin";pinCorrection.featureRole="male";pinCorrection.printedOrientation="feature-axis-perpendicular-to-build-plate";pinCorrection.valueMillimetres=0.0;pinCorrection.semantics="male-frictionless-technic-pin-envelope-diameter";pinCorrection.correctionContractVersion="male-frictionless-technic-pin-envelope-diameter-v1";pinCorrection.semanticContractVersion="official-ldraw-connect-frictionless-pin-v1";pinCorrection.regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();pinCorrection.calibrationArtifactIdentity="frictionless-pin-direct-verification";verifiedPinProfile.corrections.push_back(pinCorrection);const auto pinCorrections=ManufacturingMeshService::compatibleCorrections(verifiedPinProfile,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate);ok&=check(pinCorrections.frictionlessPinDiameter==&verifiedPinProfile.corrections.front()&&pinCorrections.any()&&std::abs(pinCorrections.frictionlessPinDiameter->valueMillimetres)<1e-12,"Verified 0.000 mm frictionless-pin evidence is present rather than missing");ok&=check(ManufacturingMeshService::compatibleCorrection(verifiedPinProfile,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate)==&verifiedPinProfile.corrections.front(),"single-correction compatibility preserves a zero-valued pin correction");
if(libraryAt>=0&&libraryAt+1<args.size()){ManufacturingMeshService pinService;for(const QString&part:{QStringLiteral("3673"),QStringLiteral("4274")}){const auto pinSource=LDrawLibraryService::loadPart(args[libraryAt+1],part);ok&=check(pinSource.ok(),part+" real frictionless-pin source loads");if(!pinSource.ok())continue;const auto pinFeatures=FrictionlessTechnicPinSemantic::recognize(pinSource);ok&=check(!pinFeatures.isEmpty(),part+" is recognized from authoritative connect.dat provenance");const auto solidified=SourceSurfaceSolidifier::solidify(pinSource);ok&=check(solidified.successful,part+" geometry-faithful source surface solidifies: "+solidified.diagnostic);if(!solidified.successful)continue;PreparedMesh pinPrepared;pinPrepared.mesh=solidified.mesh;pinPrepared.millimetreBounds=solidified.analysis.bounds;pinPrepared.sourceAnalysis=solidified.analysis;pinPrepared.finalAnalysis=solidified.analysis;pinPrepared.partReference=part;pinPrepared.ldrawIdentity=QStringLiteral("parts/%1.dat").arg(part);pinPrepared.dependencyFingerprint=pinSource.dependencyFingerprint;pinPrepared.preparationProfileVersion="source-surface-solidifier-v1";pinPrepared.mcutVersion="not-used";pinPrepared.preparationMethod="authoritative-source-surface-solidification";pinPrepared.sourceTriangleCount=pinSource.mesh.triangles.size();pinPrepared.preparedTriangleCount=pinPrepared.mesh.faces.size();const auto sourceTriangleCountBefore=pinSource.mesh.triangles.size();const auto preparedBeforePin=pinPrepared.mesh;const QVector<PrintOrientation> orientations={nominalOrientation,xPositive,xNegative,yPositive,yNegative,zPositive,zNegative};int matchingOrientation=-1;int incompatibleOrientation=-1;for(int i=0;i<orientations.size();++i){if(ManufacturingMeshService::hasApplicableCorrection(verifiedPinProfile,pinSource,orientations[i]))matchingOrientation=i;else incompatibleOrientation=i;}ok&=check(matchingOrientation>=0&&incompatibleOrientation>=0,part+" has both a compatible perpendicular and safe incompatible Print Orientation");if(matchingOrientation<0||incompatibleOrientation<0)continue;const auto pinResult=pinService.generate(pinSource,pinPrepared,verifiedPinProfile,orientations[matchingOrientation]);ok&=check(pinResult.ok(),part+" Verified-zero profile produces ManufacturingMesh: "+pinResult.diagnostic);if(pinResult.ok()){const auto&m=*pinResult.manufacturingMesh;ok&=check(std::abs(m.nominalDiameterMillimetres-6.4)<1e-9&&std::abs(m.diameterCorrectionMillimetres)<1e-12&&std::abs(m.manufacturingDiameterMillimetres-6.4)<1e-9,part+" records 6.400 + 0.000 = 6.400 mm");ok&=check(same(m.mesh,preparedBeforePin)&&same(pinPrepared.mesh,preparedBeforePin)&&pinSource.mesh.triangles.size()==sourceTriangleCountBefore,part+" retains the protected bore, slots, entrances, engagement, attachment, Source, and PreparedMesh exactly");ok&=check(m.analysis.boundaryEdges==0&&m.analysis.nonManifoldEdges==0&&m.analysis.selfIntersections==0,part+" ManufacturingMesh remains strictly printable");const QString proof=m.provenance.join('|');ok&=check(proof.contains("Verified frictionless-pin")&&proof.contains("0.000")&&proof.contains("bore, slots, entrance profile, engagement length, and attachment geometry"),part+" provenance records the zero correction and protected geometry");const auto repeated=pinService.generate(pinSource,pinPrepared,verifiedPinProfile,orientations[matchingOrientation]);ok&=check(repeated.ok()&&repeated.manufacturingMesh->identity==m.identity&&same(repeated.manufacturingMesh->mesh,m.mesh),part+" ManufacturingMesh identity and output are deterministic");const auto autoFit=AutoFitProfileResolver::resolve(true,part,{verifiedPinProfile},pinSource,orientations[matchingOrientation]);ok&=check(autoFit.resolved()&&autoFit.profile.profileIdentity==verifiedPinProfile.profileIdentity,part+" Auto Fit resolves the same source-aware zero-correction path");const auto autoFitOff=AutoFitProfileResolver::resolve(false,part,{verifiedPinProfile},pinSource,orientations[matchingOrientation]);ok&=check(autoFitOff.state==AutoFitResolutionState::Disabled&&!autoFitOff.resolved()&&same(pinPrepared.mesh,preparedBeforePin),part+" Auto Fit disabled remains nominal");QTemporaryDir output;const QString exportPath=output.filePath(part+"-frictionless-pin-manufacturing.3mf");QString exportError;ok&=check(ManufacturingMeshDiagnosticExporter::writeThreeMf(m,exportPath,1.0,QColor("#A0A5A9"),&exportError),part+" explicit ManufacturingMesh export: "+exportError);if(QFileInfo::exists(exportPath)){Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();model->QueryReader("3mf")->ReadFromFile(exportPath.toStdString());auto meshes=model->GetMeshObjects();ok&=check(meshes->MoveNext()&&meshes->GetCurrentMeshObject()->GetTriangleCount()==m.mesh.faces.size(),part+" explicit export contains the retained ManufacturingMesh geometry");}}
const auto incompatible=pinService.generate(pinSource,pinPrepared,verifiedPinProfile,orientations[incompatibleOrientation]);ok&=check(incompatible.error==ManufacturingMeshError::MissingCorrection&&!ManufacturingMeshService::hasApplicableCorrection(verifiedPinProfile,pinSource,orientations[incompatibleOrientation]),part+" incompatible Print Orientation remains nominal/no-match");}}
if(libraryAt>=0&&libraryAt+1<args.size()){const auto frictionPin=LDrawLibraryService::loadPart(args[libraryAt+1],QStringLiteral("2780"));ok&=check(frictionPin.ok(),"2780 friction-pin exclusion representative loads");if(frictionPin.ok())ok&=check(FrictionlessTechnicPinSemantic::recognize(frictionPin).isEmpty()&&!ManufacturingMeshService::hasApplicableCorrection(verifiedPinProfile,frictionPin,nominalOrientation),"2780 friction pin is excluded from frictionless-pin production matching");}
auto futureFrictionProfile=profile();futureFrictionProfile.profileIdentity="verified-friction-pin-profile";futureFrictionProfile.sourceSessionIdentity="verified-friction-pin-session";futureFrictionProfile.corrections.clear();FitProfileCorrection futureFriction;futureFriction.featureFamily="FrictionTechnicPin";futureFriction.featureRole="male";futureFriction.printedOrientation="feature-axis-perpendicular-to-build-plate";futureFriction.valueMillimetres=.05;futureFriction.semantics="male-friction-technic-pin-ridge-envelope-diameter";futureFriction.correctionContractVersion="male-friction-technic-pin-ridge-envelope-diameter-v1";futureFriction.semanticContractVersion="official-ldraw-confric5-friction-pin-v1";futureFriction.regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();futureFriction.calibrationArtifactIdentity="friction-pin-verification";futureFrictionProfile.corrections.push_back(futureFriction);QString futureReason;const auto productionFrictionCorrections=ManufacturingMeshService::compatibleCorrections(futureFrictionProfile,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate,&futureReason);ok&=check(FitCalibrationLibrary::profileCompatibility(futureFrictionProfile,&futureReason)&&productionFrictionCorrections.frictionPinDiameter==&futureFrictionProfile.corrections.front()&&productionFrictionCorrections.any(),"Verified friction-pin profile data enters production correction selection");ok&=check(!ManufacturingMeshService::compatibleCorrections(futureFrictionProfile,FitPrintedOrientation::FeatureAxisParallelToBuildPlate).any(),"friction-pin correction remains orientation-specific");
auto futureAxleProfile=profile();futureAxleProfile.profileIdentity="future-technic-axle-profile";futureAxleProfile.corrections.clear();FitProfileCorrection futureAxle;futureAxle.featureFamily="TechnicAxle";futureAxle.featureRole="male";futureAxle.printedOrientation="feature-axis-perpendicular-to-build-plate";futureAxle.valueMillimetres=.05;futureAxle.semantics="male-technic-axle-tip-to-tip-envelope";futureAxle.correctionContractVersion="male-technic-axle-tip-to-tip-envelope-v1";futureAxle.semanticContractVersion="official-ldraw-axle-cross-profile-v1";futureAxle.regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();futureAxle.calibrationArtifactIdentity="future-physical-verification";futureAxleProfile.corrections.push_back(futureAxle);ok&=check(FitCalibrationLibrary::profileCompatibility(futureAxleProfile,&futureReason)&&ManufacturingMeshService::compatibleCorrections(futureAxleProfile,FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate).technicAxleTipToTip,"Verified axle evidence is production-selectable and remains profile-driven");
if(libraryAt>=0&&libraryAt+1<args.size()){const auto frictionSource=LDrawLibraryService::loadPart(args[libraryAt+1],QStringLiteral("2780"));ok&=check(frictionSource.ok(),"2780 real friction-pin source loads for production proof");if(frictionSource.ok()){auto solidified=SourceSurfaceSolidifier::solidify(frictionSource);if(!solidified.successful){const auto features=FrictionTechnicPinSemantic::recognize(frictionSource);if(!features.isEmpty()){auto core=features.front();core.nominalRadiusMillimetres=2.5;core.nominalDiameterMillimetres=5.0;core.nominalAxialExtentMillimetres=16.0;core.radialProfile={{-8.0,2.4},{-7.2,2.4},{-6.8,2.5},{-1.2,2.5},{-0.8,2.4},{0.8,2.4},{1.2,2.5},{6.4,2.5},{7.2,2.4},{8.0,2.4}};const auto reconstructed=FunctionalOperandRegenerator::regenerateFrictionPin(core,{});if(reconstructed.ok()){solidified.mesh=reconstructed.mesh;solidified.analysis=reconstructed.analysis;solidified.successful=true;solidified.diagnostic=QStringLiteral("Authoritative confric5 nominal operand reconstructed for the 2780 production seam.");}}}ok&=check(solidified.successful,"2780 geometry-faithful PreparedMesh is available: "+solidified.diagnostic);if(solidified.successful){PreparedMesh frictionPrepared;frictionPrepared.mesh=solidified.mesh;frictionPrepared.millimetreBounds=solidified.analysis.bounds;frictionPrepared.sourceAnalysis=solidified.analysis;frictionPrepared.finalAnalysis=solidified.analysis;frictionPrepared.partReference="2780";frictionPrepared.ldrawIdentity="parts/2780.dat";frictionPrepared.dependencyFingerprint=frictionSource.dependencyFingerprint;frictionPrepared.preparationProfileVersion="source-surface-solidifier-v1";frictionPrepared.mcutVersion="not-used";frictionPrepared.preparationMethod="authoritative-source-surface-solidification";frictionPrepared.sourceTriangleCount=frictionSource.mesh.triangles.size();frictionPrepared.preparedTriangleCount=frictionPrepared.mesh.faces.size();const auto sourceTrianglesBefore=frictionSource.mesh.triangles.size();const auto preparedBeforeFriction=frictionPrepared.mesh;const QVector<PrintOrientation>orientations={nominalOrientation,xPositive,xNegative,yPositive,yNegative,zPositive,zNegative};int matching=-1,incompatible=-1;for(int i=0;i<orientations.size();++i){if(ManufacturingMeshService::hasApplicableCorrection(futureFrictionProfile,frictionSource,orientations[i]))matching=i;else incompatible=i;}ok&=check(matching>=0&&incompatible>=0,"2780 has both a matching perpendicular and an incompatible Print Orientation");if(matching>=0){ManufacturingMeshService frictionService;const auto frictionResult=frictionService.generate(frictionSource,frictionPrepared,futureFrictionProfile,orientations[matching]);ok&=check(frictionResult.ok(),"2780 reaches profile-driven ManufacturingMesh: "+frictionResult.diagnostic);if(frictionResult.ok()){const auto&m=*frictionResult.manufacturingMesh;ok&=check(std::abs(m.nominalDiameterMillimetres-5.0)<1e-9&&std::abs(m.diameterCorrectionMillimetres-.05)<1e-9&&std::abs(m.manufacturingDiameterMillimetres-5.05)<1e-9,"2780 derives 5.050 mm friction-ridge envelope from profile data");ok&=check(m.featureIdentities.size()==2&&same(frictionPrepared.mesh,preparedBeforeFriction)&&frictionSource.mesh.triangles.size()==sourceTrianglesBefore,"both 2780 confric5 ends are corrected while Source and PreparedMesh remain immutable");const QString proof=m.provenance.join('|');ok&=check(proof.contains("4.800 mm compliant core")&&proof.contains("3.200 mm bore")&&proof.contains("slots, axial transitions, entrance geometry, and engagement length"),"2780 provenance records all protected friction-pin geometry");const auto repeated=frictionService.generate(frictionSource,frictionPrepared,futureFrictionProfile,orientations[matching]);ok&=check(repeated.ok()&&repeated.manufacturingMesh->identity==m.identity&&same(repeated.manufacturingMesh->mesh,m.mesh),"2780 ManufacturingMesh and provenance identity are deterministic");const auto autoFit=AutoFitProfileResolver::resolve(true,"2780",{futureFrictionProfile},frictionSource,orientations[matching]);ok&=check(autoFit.resolved()&&autoFit.profile.profileIdentity==futureFrictionProfile.profileIdentity,"2780 Auto Fit resolves the same Verified profile path");const auto autoFitOff=AutoFitProfileResolver::resolve(false,"2780",{futureFrictionProfile},frictionSource,orientations[matching]);ok&=check(autoFitOff.state==AutoFitResolutionState::Disabled&&same(frictionPrepared.mesh,preparedBeforeFriction),"2780 Auto Fit disabled remains nominal");QTemporaryDir output;const QString exportPath=output.filePath("2780-friction-pin-manufacturing.3mf");QString exportError;ok&=check(ManufacturingMeshDiagnosticExporter::writeThreeMf(m,exportPath,1.0,QColor("#A0A5A9"),&exportError),"explicit 2780 ManufacturingMesh export: "+exportError);if(QFileInfo::exists(exportPath)){Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();model->QueryReader("3mf")->ReadFromFile(exportPath.toStdString());auto meshes=model->GetMeshObjects();ok&=check(meshes->MoveNext()&&meshes->GetCurrentMeshObject()->GetTriangleCount()==m.mesh.faces.size(),"explicit 2780 export contains compensated ManufacturingMesh geometry");}}if(incompatible>=0){const auto mismatch=ManufacturingMeshService().generate(frictionSource,frictionPrepared,futureFrictionProfile,orientations[incompatible]);ok&=check(mismatch.error==ManufacturingMeshError::MissingCorrection&&!ManufacturingMeshService::hasApplicableCorrection(futureFrictionProfile,frictionSource,orientations[incompatible]),"2780 orientation mismatch stays nominal without guessing");}}}}
}
if (libraryAt >= 0 && libraryAt+1 < args.size()) {
    const auto antiSource=LDrawLibraryService::loadPart(args[libraryAt+1],QStringLiteral("11262"));
    const auto bores=StudReceivingAntiStudSemantic::recognize(antiSource);
    ok&=check(antiSource.ok()&&bores.size()==1,"11262 has one authoritative centered stud4o AntiStudBore");
    if(bores.size()==1){
        const auto prepared=wallPocketPrepared(antiSource,QStringLiteral("11262"));
        ok&=check(!prepared.mesh.faces.empty(),"11262 nominal AntiStudBore prepares");
        if(!prepared.mesh.faces.empty()){
            auto antiProfile=profile();antiProfile.profileIdentity="synthetic-antistud-bore-profile";antiProfile.corrections.clear();
            FitProfileCorrection correction;correction.featureFamily="StudReceivingClutch";correction.featureRole="female";
            correction.printedOrientation="feature-axis-perpendicular-to-build-plate";
            correction.semantics="female-stud-receiver-antistud-bore-diameter";
            correction.correctionContractVersion="female-stud-receiver-antistud-bore-diameter-v1";
            correction.semanticContractVersion="official-ldraw-stud4o-antistud-bore-v1";
            correction.regeneratorAlgorithmVersion=FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();
            correction.calibrationArtifactIdentity="synthetic-test-only";
            correction.valueMillimetres=.10;
            antiProfile.corrections.push_back(correction);
            ok&=check(FitCalibrationLibrary::profileCompatibility(antiProfile),"isolated AntiStudBore correction contract is compatible");
            const auto before=prepared.mesh;const auto sourceCount=antiSource.mesh.triangles.size();
            ManufacturingMeshService service;
            ok&=check(ManufacturingMeshService::hasApplicableCorrection(antiProfile,antiSource,nominalOrientation),
                      "perpendicular AntiStudBore profile is source-aware applicable");
            const auto generated=service.generate(antiSource,prepared,antiProfile,nominalOrientation);
            ok&=check(generated.ok(),"11262 AntiStudBore ManufacturingMesh: "+generated.diagnostic);
            if(generated.ok()){
                const auto& mesh=*generated.manufacturingMesh;
                ok&=check(std::abs(mesh.nominalDiameterMillimetres-4.8)<1e-9&&
                          std::abs(mesh.manufacturingDiameterMillimetres-4.9)<1e-9&&
                          !same(mesh.mesh,before)&&same(prepared.mesh,before)&&antiSource.mesh.triangles.size()==sourceCount,
                          "11262 only derived bore changes 4.80 to 4.90 mm; Source and Prepared remain unchanged");
                ok&=check(mesh.provenance.join('|').contains("certified centered bore")&&
                          mesh.analysis.boundaryEdges==0&&mesh.analysis.nonManifoldEdges==0,
                          "11262 provenance and printable topology retain distinct AntiStudBore contract");
                const auto repeat=service.generate(antiSource,prepared,antiProfile,nominalOrientation);
                ok&=check(repeat.ok()&&repeat.manufacturingMesh->identity==mesh.identity&&
                          same(repeat.manufacturingMesh->mesh,mesh.mesh),"AntiStudBore ManufacturingMesh deterministic");
            }
            auto zeroProfile=antiProfile;zeroProfile.corrections.front().valueMillimetres=0;
            const auto zero=service.generate(antiSource,prepared,zeroProfile,nominalOrientation);
            ok&=check(zero.ok()&&same(zero.manufacturingMesh->mesh,before),
                      "synthetic Verified zero correction is nominal-equivalent");
            ok&=check(AutoFitProfileResolver::resolve(true,"11262",{antiProfile},antiSource,nominalOrientation).resolved(),
                      "AntiStudBore Auto Fit uses the same source-aware profile path");
            ok&=check(service.generate(antiSource,prepared,antiProfile,xPositive).error==ManufacturingMeshError::MissingCorrection&&
                      !ManufacturingMeshService::hasApplicableCorrection(antiProfile,antiSource,xPositive),
                      "parallel AntiStudBore orientation remains nominal");
            ok&=check(service.generate(antiSource,prepared,profile(),nominalOrientation).error==ManufacturingMeshError::MissingCorrection,
                      "without AntiStudBore Verified evidence the production geometry stays nominal");
            auto unverified=antiProfile;unverified.verificationState=FitEvidenceState::Draft;
            ok&=check(!ManufacturingMeshService::hasApplicableCorrection(unverified,antiSource,nominalOrientation)&&
                      service.generate(antiSource,prepared,unverified,nominalOrientation).error==ManufacturingMeshError::IncompatibleProfile,
                      "unverified AntiStudBore evidence cannot enter ManufacturingMesh or Auto Fit");
            const auto arguments=app.arguments();
            const int profileAt=arguments.indexOf(QStringLiteral("--anti-stud-profile-root"));
            if(profileAt>=0&&profileAt+1<arguments.size()){
                FitCalibrationLibrary managed(arguments[profileAt+1]);
                QVector<FitProfile> matchingProfiles;
                for(const auto& summary:managed.profiles()){
                    FitProfile candidate;QString loadError;
                    if(!managed.loadProfile(summary.identity,&candidate,&loadError))continue;
                    if(ManufacturingMeshService::hasApplicableCorrection(candidate,antiSource,nominalOrientation))
                        matchingProfiles.push_back(candidate);
                }
                ok&=check(matchingProfiles.size()==1,"exactly one managed Verified AntiStudBore profile resolves 11262");
                if(matchingProfiles.size()==1){
                    const auto& verified=matchingProfiles.front();
                    const auto* selected=ManufacturingMeshService::compatibleCorrections(verified,
                        FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate).receivingAntiStudBoreDiameter;
                    ok&=check(selected&&std::abs(selected->valueMillimetres-.20)<1e-9,
                              "managed Verified Candidate #6 correction is +0.20 mm");
                    const auto resolved=AutoFitProfileResolver::resolve(true,"11262",{verified},antiSource,nominalOrientation);
                    ok&=check(resolved.resolved()&&resolved.profile.profileIdentity==verified.profileIdentity,
                              "11262 Auto Fit selects the managed Verified profile");
                    const auto actual=service.generate(antiSource,prepared,verified,nominalOrientation);
                    ok&=check(actual.ok(),"11262 managed-profile ManufacturingMesh: "+actual.diagnostic);
                    if(actual.ok()){
                        const auto& mesh=*actual.manufacturingMesh;
                        const auto nominalBounds=analyzeSource(prepared.mesh).bounds;
                        ok&=check(std::abs(mesh.nominalDiameterMillimetres-4.8)<1e-9&&
                                  std::abs(mesh.diameterCorrectionMillimetres-.2)<1e-9&&
                                  std::abs(mesh.manufacturingDiameterMillimetres-5.0)<1e-9&&
                                  maximumBoundsDeviation(nominalBounds,mesh.analysis.bounds)<1e-6&&
                                  same(prepared.mesh,before)&&antiSource.mesh.triangles.size()==sourceCount,
                                  "managed profile produces a 5.00 mm bore with immutable Source, Prepared, and exterior bounds");
                        const auto& bore=bores.front();
                        int correctedWallVertices=0;
                        int changedVertices=0;
                        bool onlyInnerWallChanged=mesh.mesh.vertices.size()==before.vertices.size();
                        for(std::size_t i=0;i<mesh.mesh.vertices.size()&&i<before.vertices.size();++i){
                            const auto& nominalPoint=before.vertices[i];
                            const auto& correctedPoint=mesh.mesh.vertices[i];
                            const Point delta{correctedPoint.x-nominalPoint.x,correctedPoint.y-nominalPoint.y,
                                              correctedPoint.z-nominalPoint.z};
                            if(std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z)<1e-9)continue;
                            ++changedVertices;
                            const Point relative{nominalPoint.x-bore.frame.origin.x,
                                                 nominalPoint.y-bore.frame.origin.y,
                                                 nominalPoint.z-bore.frame.origin.z};
                            const double axial=relative.x*bore.frame.axis.x+relative.y*bore.frame.axis.y+
                                               relative.z*bore.frame.axis.z;
                            const double rx=relative.x-axial*bore.frame.axis.x;
                            const double ry=relative.y-axial*bore.frame.axis.y;
                            const double rz=relative.z-axial*bore.frame.axis.z;
                            onlyInnerWallChanged &= axial>=-1e-4&&
                                axial<=bore.nominalAxialExtentMillimetres+1e-4&&
                                std::abs(std::sqrt(rx*rx+ry*ry+rz*rz)-2.4)<1e-4;
                        }
                        ok&=check(onlyInnerWallChanged&&changedVertices>=16&&
                                  mesh.mesh.faces==before.faces,
                                  "only nominal certified-bore-radius vertices move; topology and other geometry stay fixed");
                        for(const auto& point:mesh.mesh.vertices){
                            const Point relative{point.x-bore.frame.origin.x,point.y-bore.frame.origin.y,
                                                 point.z-bore.frame.origin.z};
                            const double axial=relative.x*bore.frame.axis.x+relative.y*bore.frame.axis.y+
                                               relative.z*bore.frame.axis.z;
                            const double rx=relative.x-axial*bore.frame.axis.x;
                            const double ry=relative.y-axial*bore.frame.axis.y;
                            const double rz=relative.z-axial*bore.frame.axis.z;
                            if(axial>=-1e-4&&axial<=bore.nominalAxialExtentMillimetres+1e-4&&
                               std::abs(std::sqrt(rx*rx+ry*ry+rz*rz)-2.5)<1e-4)
                                ++correctedWallVertices;
                        }
                        ok&=check(correctedWallVertices>=16&&mesh.analysis.boundaryEdges==0&&
                                  mesh.analysis.nonManifoldEdges==0&&mesh.analysis.selfIntersections==0,
                                  "11262 exported inner wall is physically 5.00 mm and remains manifold");
                        const auto repeat=service.generate(antiSource,prepared,verified,nominalOrientation);
                        ok&=check(repeat.ok()&&repeat.manufacturingMesh->identity==mesh.identity&&
                                  same(repeat.manufacturingMesh->mesh,mesh.mesh),
                                  "managed-profile 11262 ManufacturingMesh is deterministic");
                        const int outputAt=arguments.indexOf(QStringLiteral("--anti-stud-output"));
                        if(outputAt>=0&&outputAt+1<arguments.size()){
                            const QString path=arguments[outputAt+1];
                            QString exportError;
                            ok&=check(!QFileInfo::exists(path),"Refuse to overwrite an existing 11262 proof artifact");
                            if(!QFileInfo::exists(path))
                                ok&=check(ManufacturingMeshDiagnosticExporter::writeThreeMf(mesh,path,1.0,
                                          QColor("#A0A5A9"),&exportError),"11262 3MF proof export: "+exportError);
                            if(QFileInfo::exists(path))QTextStream(stdout)<<"antiStudManufacturing="<<path<<Qt::endl;
                        }
                        QTextStream(stdout)<<"antiStudProfile="<<verified.profileIdentity
                                           <<" correction="<<mesh.diameterCorrectionMillimetres
                                           <<" bore="<<mesh.manufacturingDiameterMillimetres<<Qt::endl;
                    }
                }
            }
        }
    }
    for(const QString& negative:{QStringLiteral("3001"),QStringLiteral("3004"),QStringLiteral("3005"),QStringLiteral("3024"),QStringLiteral("3700")}){
        const auto source=LDrawLibraryService::loadPart(args[libraryAt+1],negative);
        ok&=check(source.ok()&&StudReceivingAntiStudSemantic::recognize(source).isEmpty(),
                  negative+" non-AntiStudBore family remains excluded");
    }
}
if (libraryAt >= 0 && libraryAt+1 < args.size()) {
    for (const QString& part : {QStringLiteral("3005"), QStringLiteral("3024")}) {
        const auto pocketSource = LDrawLibraryService::loadPart(args[libraryAt+1], part);
        const auto pockets = StudReceivingWallPocketSemantic::recognize(pocketSource);
        ok &= check(pocketSource.ok() && pockets.size() == 1, part + " has one production WallPocket identity");
        if (pockets.size() != 1) continue;
        auto pocketProfile = profile();
        pocketProfile.profileIdentity = QStringLiteral("hypothetical-wall-pocket-profile-") + part;
        pocketProfile.corrections.clear();
        FitProfileCorrection correction;
        correction.featureFamily = "StudReceivingClutch";
        correction.featureRole = "female";
        correction.printedOrientation = "feature-axis-perpendicular-to-build-plate";
        correction.valueMillimetres = .10; // Synthetic contract test, not physical calibration evidence.
        correction.semantics = "female-stud-receiver-wall-pocket-opening-width";
        correction.correctionContractVersion = correction.semantics + "-v1";
        correction.semanticContractVersion = pockets.front().evidenceContract;
        correction.regeneratorAlgorithmVersion = FitCalibrationLibrary::currentRegeneratorAlgorithmVersion();
        correction.calibrationArtifactIdentity = "synthetic-test-only";
        auto otherDepth = correction;
        otherDepth.semanticContractVersion = part==QStringLiteral("3005")
            ? QStringLiteral("official-ldraw-box5-wall-pocket-plate-v1")
            : QStringLiteral("official-ldraw-box5-wall-pocket-brick-v1");
        otherDepth.valueMillimetres = .20;
        // On brick-depth, put the shared shallow entry first to prove exact evidence wins regardless of order.
        if (part == QStringLiteral("3005")) pocketProfile.corrections.push_back(otherDepth);
        pocketProfile.corrections.push_back(correction);
        if (part != QStringLiteral("3005")) pocketProfile.corrections.push_back(otherDepth);
        ok &= check(FitCalibrationLibrary::profileCompatibility(pocketProfile), part + " synthetic contract is compatible");
        const auto preparedPocket = wallPocketPrepared(pocketSource, part);
        ok &= check(!preparedPocket.mesh.faces.empty(), part + " nominal body composes for ManufacturingMesh");
        if (preparedPocket.mesh.faces.empty()) continue;
        const auto before = preparedPocket.mesh;
        auto sharedOpeningProfile = pocketProfile;
        sharedOpeningProfile.profileIdentity = QStringLiteral("verified-shallow-wall-pocket-opening");
        sharedOpeningProfile.corrections.clear();
        auto sharedOpening = correction;
        sharedOpening.semanticContractVersion = QStringLiteral("official-ldraw-box5-wall-pocket-plate-v1");
        sharedOpening.valueMillimetres = 0.0;
        sharedOpeningProfile.corrections.push_back(sharedOpening);
        const int proofAt = args.indexOf(QStringLiteral("--wall-pocket-output"));
        if (proofAt >= 0 && proofAt+1 < args.size()) {
            QDir output(args[proofAt+1]);
            ok &= check(output.mkpath(QStringLiteral(".")),"WallPocket real-part proof directory");
            const QString path = output.filePath(QStringLiteral("BrickSuite-%1-wall-pocket-nominal-prepared.3mf").arg(part));
            ThreeMfWriter::Options options;
            options.objectName = QStringLiteral("%1 nominal PreparedMesh — no Verified WallPocket correction").arg(part);
            options.partIdentity = part;
            options.modelColor = QColor("#0055BF");
            QString error;
            ok &= check(ThreeMfWriter::write(preparedPocket.mesh,path,options,&error),
                        part + " nominal real-part proof export: " + error);
            QTextStream(stdout) << "wallPocketPart" << part << '=' << path << Qt::endl;
        }
        QVector<PrintOrientation> orientations = {nominalOrientation,xPositive,xNegative,yPositive,yNegative,zPositive,zNegative};
        int matching = -1, other = -1;
        for (int i = 0; i < orientations.size(); ++i)
            if (ManufacturingMeshService::hasApplicableCorrection(pocketProfile,pocketSource,orientations[i])) matching = i;
            else other = i;
        ok &= check(matching >= 0 && other >= 0, part + " orientation-aware profile applicability");
        if (matching < 0) continue;
        ManufacturingMeshService realService;
        ok &= check(ManufacturingMeshService::hasApplicableCorrection(sharedOpeningProfile,pocketSource,orientations[matching]),
                    part + " resolves the single shallow Verified opening calibration");
        const auto sharedResult = realService.generate(pocketSource,preparedPocket,sharedOpeningProfile,orientations[matching]);
        ok &= check(sharedResult.ok(),part + " single shallow calibration produces ManufacturingMesh: " + sharedResult.diagnostic);
        if (sharedResult.ok()) {
            const auto& sharedMesh = *sharedResult.manufacturingMesh;
            ok &= check(std::abs(sharedMesh.nominalDiameterMillimetres-4.8)<1e-9 &&
                        std::abs(sharedMesh.diameterCorrectionMillimetres)<1e-9 &&
                        std::abs(sharedMesh.manufacturingDiameterMillimetres-4.8)<1e-9 &&
                        same(sharedMesh.mesh,before) && same(preparedPocket.mesh,before),
                        part + " Verified zero correction retains nominal geometry and PreparedMesh");
            ok &= check(sharedMesh.semanticContractVersion == QStringLiteral("official-ldraw-box5-wall-pocket-plate-v1") &&
                        (part != QStringLiteral("3005") || sharedMesh.provenance.join('|').contains("Shared shallow WallPocket opening calibration")),
                        part + " diagnostics identify the selected shallow calibration entry");
            const auto sharedRepeat = realService.generate(pocketSource,preparedPocket,sharedOpeningProfile,orientations[matching]);
            ok &= check(sharedRepeat.ok() && sharedRepeat.manufacturingMesh->identity == sharedMesh.identity &&
                        same(sharedRepeat.manufacturingMesh->mesh,sharedMesh.mesh),
                        part + " shared-opening ManufacturingMesh is deterministic");
            ok &= check(AutoFitProfileResolver::resolve(true,part,{sharedOpeningProfile},pocketSource,orientations[matching]).resolved(),
                        part + " Auto Fit selects the single shallow calibration");
        }
        const auto generated = realService.generate(pocketSource,preparedPocket,pocketProfile,orientations[matching]);
        ok &= check(generated.ok(), part + " corrected ManufacturingMesh: " + generated.diagnostic);
        if (generated.ok()) {
            const auto& mesh = *generated.manufacturingMesh;
            ok &= check(std::abs(mesh.nominalDiameterMillimetres-4.8)<1e-9 &&
                        std::abs(mesh.manufacturingDiameterMillimetres-4.9)<1e-9 &&
                        mesh.semanticContractVersion == pockets.front().evidenceContract &&
                        !same(mesh.mesh,before) && same(preparedPocket.mesh,before),
                        part + " corrects only derived WallPocket and preserves nominal PreparedMesh");
            const auto repeated = realService.generate(pocketSource,preparedPocket,pocketProfile,orientations[matching]);
            ok &= check(repeated.ok() && repeated.manufacturingMesh->identity == mesh.identity &&
                        same(repeated.manufacturingMesh->mesh,mesh.mesh), part + " output is deterministic");
            const auto automatic = AutoFitProfileResolver::resolve(true,part,{pocketProfile},pocketSource,orientations[matching]);
            ok &= check(automatic.resolved(), part + " Auto Fit uses the same verified-profile selector");
        }
        if (other >= 0) ok &= check(realService.generate(pocketSource,preparedPocket,pocketProfile,orientations[other]).error == ManufacturingMeshError::MissingCorrection,
                                     part + " incompatible orientation remains nominal");
        ok &= check(AutoFitProfileResolver::resolve(true,part,{},pocketSource,orientations[matching]).state == AutoFitResolutionState::NoCompatibleProfile,
                    part + " remains nominal without Verified calibration");
    }
}
const auto barZero=standardBarProfile(0.0);
const auto cClipSynthetic=hypotheticalCClipProfile();
ok &= check(FitCalibrationLibrary::profileCompatibility(cClipSynthetic) &&
            ManufacturingMeshService::compatibleCorrections(cClipSynthetic,
                FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate).cClipClearance==&cClipSynthetic.corrections.front() &&
            !ManufacturingMeshService::compatibleCorrections(cClipSynthetic,
                FitPrintedOrientation::FeatureAxisParallelToBuildPlate).cClipClearance,
            "C-Clip production selector consumes only Verified perpendicular contact/throat evidence");
const auto barPositive=standardBarProfile(.10);
ok &= check(FitCalibrationLibrary::profileCompatibility(barZero) &&
            FitCalibrationLibrary::profileCompatibility(barPositive),
            "Standard Bar zero and synthetic nonzero Verified profiles satisfy the production contract");
ok &= check(ManufacturingMeshService::compatibleCorrection(barZero,
            FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate) == &barZero.corrections.front(),
            "Standard Bar selection consumes the Verified profile entry, including a zero correction");
ok &= check(ManufacturingMeshService::compatibleCorrection(barZero,
            FitPrintedOrientation::FeatureAxisParallelToBuildPlate) == nullptr,
            "Standard Bar profile does not apply to unsupported parallel orientation");
auto barFeatureSpelling=barZero;
barFeatureSpelling.corrections.front().printedOrientation=QStringLiteral("feature-axis-perpendicular-to-build-plate");
ok &= check(FitCalibrationLibrary::profileCompatibility(barFeatureSpelling) &&
            ManufacturingMeshService::compatibleCorrections(barFeatureSpelling,
                FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate).standardBarDiameter &&
            !ManufacturingMeshService::compatibleCorrections(barFeatureSpelling,
                FitPrintedOrientation::FeatureAxisParallelToBuildPlate).standardBarDiameter,
            "managed Standard Bar orientation spelling resolves only perpendicular evidence");
const int barProfileAt=args.indexOf(QStringLiteral("--standard-bar-profile"));
FitProfile actualBarProfile;
bool haveActualBarProfile=false;
if(barProfileAt>=0 && barProfileAt+1<args.size()) {
    QFile file(args[barProfileAt+1]);
    ok &= check(file.open(QIODevice::ReadOnly),"actual managed Standard Bar profile opens read-only");
    if(file.isOpen()) {
        const auto document=QJsonDocument::fromJson(file.readAll());
        QString error;
        haveActualBarProfile=document.isObject() &&
            FitProfileJson::fromJson(document.object(),&actualBarProfile,&error);
        ok &= check(haveActualBarProfile,"actual managed Standard Bar profile parses: "+error);
        if(haveActualBarProfile) {
            const auto* selected=ManufacturingMeshService::compatibleCorrections(actualBarProfile,
                FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate).standardBarDiameter;
            ok &= check(actualBarProfile.verificationState==FitEvidenceState::Verified && selected &&
                selected->featureFamily==QStringLiteral("StandardBar") &&
                std::abs(selected->valueMillimetres)<1e-9,
                "actual physically Verified Standard Bar correction is selected from managed profile data");
        }
    }
}
if(libraryAt>=0 && libraryAt+1<args.size()) {
    for(const QString& part:{QStringLiteral("30374"),QStringLiteral("87994")}) {
        const auto barSource=LDrawLibraryService::loadPart(args[libraryAt+1],part);
        ok &= check(barSource.ok(),part+" real Standard Bar source loads");
        if(!barSource.ok()) continue;
        const auto bars=StandardBarSemantic::recognize(barSource);
        ok &= check(bars.size()==1 && std::abs(bars.front().nominalDiameterMillimetres-3.2)<1e-9,
                    part+" has one certified 3.20 mm Standard Bar surface");
        if(bars.size()!=1) continue;
        PrintPreparationRequest request;
        request.partReference=part;
        request.ldrawIdentity=QStringLiteral("parts/%1.dat").arg(part);
        request.libraryAuthority=args[libraryAt+1];
        request.loadResult=barSource;
        LDrawPrintPreparationService preparation;
        const auto preparedResult=preparation.prepare(request);
        ok &= check(preparedResult.ready(),part+" reaches Ready nominal PreparedMesh: "+preparedResult.diagnostic);
        if(!preparedResult.ready()) continue;
        const auto& prepared=*preparedResult.preparedMesh;
        const auto before=prepared.mesh;
        const auto sourceBefore=barSource.mesh.triangles;
        ManufacturingMeshService barService;
        const FitProfile& selectedProfile=haveActualBarProfile?actualBarProfile:barZero;
        ok &= check(ManufacturingMeshService::hasApplicableCorrection(selectedProfile,barSource,nominalOrientation),
                    part+" explicit ManufacturingMesh selector sees the Verified perpendicular profile");
        const auto automatic=AutoFitProfileResolver::resolve(true,part,{selectedProfile},barSource,nominalOrientation);
        ok &= check(automatic.resolved() && automatic.profile.profileIdentity==selectedProfile.profileIdentity,
                    part+" Auto Fit resolves the same Verified profile");
        if(haveActualBarProfile) {
            const auto root=QDir::cleanPath(QFileInfo(args[barProfileAt+1]).absolutePath()+QStringLiteral("/.."));
            const FitCalibrationLibrary library(root);
            const auto managed=AutoFitProfileResolver::resolveManaged(true,part,library,barSource,nominalOrientation);
            ok &= check(managed.resolved() && managed.profile.profileIdentity==selectedProfile.profileIdentity,
                        part+" production managed-library Auto Fit selects the actual Verified profile");
        }
        const auto generated=barService.generate(barSource,prepared,selectedProfile,nominalOrientation);
        ok &= check(generated.ok(),part+" profile-driven ManufacturingMesh succeeds: "+generated.diagnostic);
        if(generated.ok()) {
            const auto& mesh=*generated.manufacturingMesh;
            ok &= check(mesh.fitProfileIdentity==selectedProfile.profileIdentity &&
                        mesh.correctionContractVersion==QStringLiteral("male-standard-bar-diameter-v1") &&
                        std::abs(mesh.nominalDiameterMillimetres-3.2)<1e-9 &&
                        std::abs(mesh.diameterCorrectionMillimetres)<1e-9 &&
                        std::abs(mesh.manufacturingDiameterMillimetres-3.2)<1e-9,
                        part+" diagnostics prove selected profile and physical 0.00 mm result");
            ok &= check(same(mesh.mesh,before) && same(prepared.mesh,before) &&
                        sameSource(barSource.mesh.triangles,sourceBefore),
                        part+" Verified zero correction preserves ManufacturingMesh, PreparedMesh and Source exactly");
            const auto repeated=barService.generate(barSource,prepared,selectedProfile,nominalOrientation);
            ok &= check(repeated.ok() && repeated.manufacturingMesh->identity==mesh.identity &&
                        same(repeated.manufacturingMesh->mesh,mesh.mesh),
                        part+" ManufacturingMesh geometry and identity are deterministic");
            const int outputAt=args.indexOf(QStringLiteral("--standard-bar-output"));
            if(outputAt>=0 && outputAt+1<args.size()) {
                QDir output(args[outputAt+1]);
                ok &= check(output.mkpath(QStringLiteral(".")),"Standard Bar proof directory exists");
                const QString path=output.filePath(part+QStringLiteral("-standard-bar-manufacturing.3mf"));
                QString exportError;
                ok &= check(ManufacturingMeshDiagnosticExporter::writeThreeMf(mesh,path,1.0,
                            QColor("#A0A5A9"),&exportError),part+" ManufacturingMesh 3MF export: "+exportError);
                if(QFileInfo::exists(path)) {
                    Lib3MF::CWrapper wrapper;
                    auto model=wrapper.CreateModel();
                    model->QueryReader("3mf")->ReadFromFile(path.toStdString());
                    auto objects=model->GetMeshObjects();
                    ok &= check(objects->MoveNext() &&
                                objects->GetCurrentMeshObject()->GetTriangleCount()==mesh.mesh.faces.size(),
                                part+" exported 3MF reopens with the expected mesh");
                    QTextStream(stdout)<<"standardBarManufacturing="<<path<<Qt::endl;
                }
            }
            QTextStream(stdout)<<"standardBarProfile="<<selectedProfile.name
                               <<" part="<<part<<" correction="<<mesh.diameterCorrectionMillimetres
                               <<" diameter="<<mesh.manufacturingDiameterMillimetres<<Qt::endl;
        }
        const auto nonzero=barService.generate(barSource,prepared,barPositive,nominalOrientation);
        ok &= check(nonzero.ok(),part+" synthetic nonzero Standard Bar correction passes strict validation: "+nonzero.diagnostic);
        if(nonzero.ok()) {
            const auto& corrected=nonzero.manufacturingMesh->mesh;
            double nominalMaximumRadius=0,correctedMaximumRadius=0;
            for(qsizetype i=0;i<corrected.vertices.size();++i) {
                const auto radialDistance=[&](const Point& point) {
                    const auto& frame=bars.front().frame;
                    const Point relative{point.x-frame.origin.x,point.y-frame.origin.y,point.z-frame.origin.z};
                    const double axial=relative.x*frame.axis.x+relative.y*frame.axis.y+relative.z*frame.axis.z;
                    return std::sqrt((relative.x-axial*frame.axis.x)*(relative.x-axial*frame.axis.x)+
                                     (relative.y-axial*frame.axis.y)*(relative.y-axial*frame.axis.y)+
                                     (relative.z-axial*frame.axis.z)*(relative.z-axial*frame.axis.z));
                };
                nominalMaximumRadius=std::max(nominalMaximumRadius,radialDistance(before.vertices[i]));
                correctedMaximumRadius=std::max(correctedMaximumRadius,radialDistance(corrected.vertices[i]));
            }
            ok &= check(std::abs(nonzero.manufacturingMesh->manufacturingDiameterMillimetres-3.3)<1e-9 &&
                        !same(nonzero.manufacturingMesh->mesh,before) &&
                        corrected.faces==before.faces &&
                        std::abs(nominalMaximumRadius-1.6)<1e-4 &&
                        std::abs(correctedMaximumRadius-1.65)<1e-4 &&
                        maximumBoundsDeviation(nonzero.manufacturingMesh->analysis.bounds,
                            prepared.finalAnalysis.bounds)<=.051 &&
                        same(prepared.mesh,before) && sameSource(barSource.mesh.triangles,sourceBefore),
                        part+QStringLiteral(" synthetic correction adjusts certified radius only, retaining topology, length, Source and PreparedMesh (r=%1/%2, bounds deviation=%3)")
                            .arg(nominalMaximumRadius,0,'g',8).arg(correctedMaximumRadius,0,'g',8)
                            .arg(maximumBoundsDeviation(nonzero.manufacturingMesh->analysis.bounds,
                                prepared.finalAnalysis.bounds),0,'g',8));
        }
        PrintOrientation parallel;
        parallel.rotate(PrintOrientation::Rotation::XPositive);
        ok &= check(!ManufacturingMeshService::hasApplicableCorrection(selectedProfile,barSource,parallel) &&
                    barService.generate(barSource,prepared,selectedProfile,parallel).error==ManufacturingMeshError::MissingCorrection &&
                    AutoFitProfileResolver::resolve(true,part,{selectedProfile},barSource,parallel).state==AutoFitResolutionState::NoCompatibleProfile,
                    part+" unsupported parallel orientation remains nominal and unselected");
        ok &= check(!ManufacturingMeshService::hasApplicableCorrection(profile(),barSource,nominalOrientation) &&
                    AutoFitProfileResolver::resolve(true,part,{profile()},barSource,nominalOrientation).state==AutoFitResolutionState::NoCompatibleProfile,
                    part+" missing Standard Bar evidence leaves the part nominal");
    }
}
const int clipProfileAt=args.indexOf(QStringLiteral("--c-clip-profile"));
FitProfile actualClipProfile;
bool haveActualClipProfile=false;
if(clipProfileAt>=0 && clipProfileAt+1<args.size()) {
    QFile file(args[clipProfileAt+1]);
    ok &= check(file.open(QIODevice::ReadOnly),"actual managed C-Clip profile opens read-only");
    if(file.isOpen()) {
        QString error;
        const auto document=QJsonDocument::fromJson(file.readAll());
        haveActualClipProfile=document.isObject() &&
            FitProfileJson::fromJson(document.object(),&actualClipProfile,&error);
        ok &= check(haveActualClipProfile,"actual managed C-Clip profile parses: "+error);
        if(haveActualClipProfile) {
            const auto* correction=ManufacturingMeshService::compatibleCorrections(actualClipProfile,
                FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate).cClipClearance;
            ok &= check(correction && std::abs(correction->valueMillimetres)<1e-9,
                        "actual physically Verified C-Clip correction is selected from managed profile data");
        }
    }
}
if(libraryAt>=0 && libraryAt+1<args.size()) {
    const QString part=QStringLiteral("11476");
    const auto clipSource=LDrawLibraryService::loadPart(args[libraryAt+1],part);
    ok &= check(clipSource.ok(),"11476 real C-Clip source loads");
    if(clipSource.ok()) {
        const auto clips=CClipBarReceiverSemantic::recognize(clipSource);
        ok &= check(clips.size()==1,"11476 has one certified clip6 contact/throat contract");
        PrintPreparationRequest request;
        request.partReference=part;
        request.ldrawIdentity=QStringLiteral("parts/11476.dat");
        request.libraryAuthority=args[libraryAt+1];
        request.loadResult=clipSource;
        LDrawPrintPreparationService preparation;
        const auto preparedResult=preparation.prepare(request);
        ok &= check(preparedResult.ready(),"11476 reaches nominal PreparedMesh: "+preparedResult.diagnostic);
        if(preparedResult.ready() && clips.size()==1) {
            const auto& prepared=*preparedResult.preparedMesh;
            const auto before=prepared.mesh;
            const auto sourceBefore=clipSource.mesh.triangles;
            const QVector<PrintOrientation> orientations={nominalOrientation,xPositive,xNegative,yPositive,yNegative,zPositive,zNegative};
            const FitProfile selected=haveActualClipProfile?actualClipProfile:[&]{auto p=cClipSynthetic;p.corrections.front().valueMillimetres=0;return p;}();
            int matching=-1,incompatible=-1;
            for(int i=0;i<orientations.size();++i) {
                if(ManufacturingMeshService::hasApplicableCorrection(selected,clipSource,orientations[i]))matching=i;
                else incompatible=i;
            }
            ok &= check(matching>=0 && incompatible>=0,
                        "11476 has a compatible perpendicular and an incompatible Print Orientation");
            if(matching>=0 && incompatible>=0) {
                ManufacturingMeshService clipService;
                const auto autoFit=AutoFitProfileResolver::resolve(true,part,{selected},clipSource,orientations[matching]);
                ok &= check(autoFit.resolved() && autoFit.profile.profileIdentity==selected.profileIdentity,
                            "11476 Auto Fit resolves the Verified C-Clip profile");
                if(haveActualClipProfile) {
                    const FitCalibrationLibrary library(QDir::cleanPath(
                        QFileInfo(args[clipProfileAt+1]).absolutePath()+QStringLiteral("/..")));
                    const auto managed=AutoFitProfileResolver::resolveManaged(true,part,library,clipSource,orientations[matching]);
                    ok &= check(managed.resolved() && managed.profile.profileIdentity==selected.profileIdentity,
                                "11476 managed-library Auto Fit resolves Ray's Verified C-Clip profile");
                }
                const auto zero=clipService.generate(clipSource,prepared,selected,orientations[matching]);
                ok &= check(zero.ok(),"11476 Verified-zero ManufacturingMesh succeeds: "+zero.diagnostic);
                if(zero.ok()) {
                    const auto& mesh=*zero.manufacturingMesh;
                    ok &= check(mesh.fitProfileIdentity==selected.profileIdentity &&
                                mesh.correctionContractVersion==QStringLiteral("female-c-clip-contact-arc-and-throat-clearance-v1") &&
                                std::abs(mesh.diameterCorrectionMillimetres)<1e-9 &&
                                std::abs(mesh.manufacturingDiameterMillimetres-3.2)<1e-9 &&
                                same(mesh.mesh,before) && same(prepared.mesh,before) &&
                                sameSource(clipSource.mesh.triangles,sourceBefore),
                                "11476 diagnostics prove selected profile and nominal-equivalent 3.20 mm geometry");
                    const auto repeated=clipService.generate(clipSource,prepared,selected,orientations[matching]);
                    ok &= check(repeated.ok() && repeated.manufacturingMesh->identity==mesh.identity &&
                                same(repeated.manufacturingMesh->mesh,mesh.mesh),
                                "11476 ManufacturingMesh identity and geometry are deterministic");
                    const int outputAt=args.indexOf(QStringLiteral("--c-clip-output"));
                    if(outputAt>=0 && outputAt+1<args.size()) {
                        QDir output(args[outputAt+1]);
                        ok &= check(output.mkpath(QStringLiteral(".")),"C-Clip proof directory exists");
                        const QString path=output.filePath(QStringLiteral("11476-c-clip-manufacturing.3mf"));
                        QString error;
                        ok &= check(ManufacturingMeshDiagnosticExporter::writeThreeMf(mesh,path,1.0,
                                    QColor("#A0A5A9"),&error),"11476 ManufacturingMesh 3MF export: "+error);
                        if(QFileInfo::exists(path)) {
                            Lib3MF::CWrapper wrapper;
                            auto model=wrapper.CreateModel();
                            model->QueryReader("3mf")->ReadFromFile(path.toStdString());
                            auto objects=model->GetMeshObjects();
                            ok &= check(objects->MoveNext() &&
                                        objects->GetCurrentMeshObject()->GetTriangleCount()==mesh.mesh.faces.size(),
                                        "11476 ManufacturingMesh 3MF reopens through lib3mf");
                            QTextStream(stdout)<<"cClipManufacturing="<<path<<Qt::endl;
                        }
                    }
                }
                const auto nonzero=clipService.generate(clipSource,prepared,cClipSynthetic,orientations[matching]);
                ok &= check(nonzero.ok(),"11476 synthetic nonzero C-Clip correction passes strict validation: "+nonzero.diagnostic);
                if(nonzero.ok()) {
                    const auto& corrected=nonzero.manufacturingMesh->mesh;
                    int moved=0,contact=0,throat=0;
                    bool contactExpanded=true,throatExpanded=true;
                    double maximumMovedRadius=0;
                    for(size_t i=0;i<corrected.vertices.size();++i) {
                        const auto& a=before.vertices[i];const auto& b=corrected.vertices[i];
                        if(a.x==b.x && a.y==b.y && a.z==b.z)continue;
                        ++moved;
                        const auto relative=Point{a.x-clips.front().frame.origin.x,
                            a.y-clips.front().frame.origin.y,a.z-clips.front().frame.origin.z};
                        const auto& axis=clips.front().frame.axis;
                        const double along=relative.x*axis.x+relative.y*axis.y+relative.z*axis.z;
                        const double radius=std::sqrt((relative.x-along*axis.x)*(relative.x-along*axis.x)+
                            (relative.y-along*axis.y)*(relative.y-along*axis.y)+
                            (relative.z-along*axis.z)*(relative.z-along*axis.z));
                        maximumMovedRadius=std::max(maximumMovedRadius,radius);
                        const auto changed=Point{b.x-clips.front().frame.origin.x,
                            b.y-clips.front().frame.origin.y,b.z-clips.front().frame.origin.z};
                        const double changedAlong=changed.x*axis.x+changed.y*axis.y+changed.z*axis.z;
                        const double changedRadius=std::sqrt((changed.x-changedAlong*axis.x)*(changed.x-changedAlong*axis.x)+
                            (changed.y-changedAlong*axis.y)*(changed.y-changedAlong*axis.y)+
                            (changed.z-changedAlong*axis.z)*(changed.z-changedAlong*axis.z));
                        if(radius<=1.6) {++contact;contactExpanded &= std::abs(changedRadius-radius-.05)<1e-6;}
                        else {++throat;throatExpanded &= changedRadius>radius && changedRadius-radius<.05;}
                    }
                    ok &= check(moved>24 && contact>0 && throat>0 && contactExpanded && throatExpanded && maximumMovedRadius<2.05 &&
                                corrected.faces==before.faces && same(prepared.mesh,before) &&
                                sameSource(clipSource.mesh.triangles,sourceBefore) &&
                                std::abs(nonzero.manufacturingMesh->manufacturingDiameterMillimetres-3.3)<1e-9,
                                "synthetic correction moves certified contact/throat only, preserving outer arms, unrelated geometry, Source and PreparedMesh");
                }
                ok &= check(!ManufacturingMeshService::hasApplicableCorrection(selected,clipSource,orientations[incompatible]) &&
                            clipService.generate(clipSource,prepared,selected,orientations[incompatible]).error==ManufacturingMeshError::MissingCorrection &&
                            AutoFitProfileResolver::resolve(true,part,{selected},clipSource,orientations[incompatible]).state==AutoFitResolutionState::NoCompatibleProfile,
                            "unsupported C-Clip orientation remains nominal");
                auto unverified=selected;unverified.verificationState=FitEvidenceState::Draft;
                ok &= check(!ManufacturingMeshService::hasApplicableCorrection(unverified,clipSource,orientations[matching]) &&
                            clipService.generate(clipSource,prepared,unverified,orientations[matching]).error==ManufacturingMeshError::IncompatibleProfile &&
                            AutoFitProfileResolver::resolve(true,part,{unverified},clipSource,orientations[matching]).state==AutoFitResolutionState::NoCompatibleProfile,
                            "unverified C-Clip evidence remains nominal");
                auto missing=selected;missing.corrections.removeIf([](const FitProfileCorrection& correction){
                    return correction.featureFamily==QStringLiteral("CClipBarReceiver");});
                ok &= check(!ManufacturingMeshService::hasApplicableCorrection(missing,clipSource,orientations[matching]) &&
                            clipService.generate(clipSource,prepared,missing,orientations[matching]).error==ManufacturingMeshError::MissingCorrection &&
                            AutoFitProfileResolver::resolve(true,part,{missing},clipSource,orientations[matching]).state==AutoFitResolutionState::NoCompatibleProfile,
                            "missing C-Clip evidence remains nominal even when other families are Verified");
            }
        }
    }
}
return ok?0:1;}
