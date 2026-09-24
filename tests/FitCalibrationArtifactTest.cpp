#include "../src/services/geometry/fit/RoundTechnicCalibrationArtifact.h"
#include "../src/services/geometry/fit/StandardStudCalibrationArtifact.h"
#include "../src/services/geometry/fit/StudReceivingCalibrationArtifact.h"
#include "../src/services/geometry/fit/StandardBarCalibrationArtifact.h"
#include "../src/services/geometry/fit/CClipBarReceiverCalibrationArtifact.h"
#include "../src/services/geometry/fit/BallJointCalibrationArtifact.h"
#include "../src/services/geometry/fit/BallSocketCalibrationArtifact.h"
#include "../src/services/geometry/print/BallSocketSemantic.h"
#include "../src/services/geometry/fit/FitCalibrationFixtureLabel.h"
#include "../src/services/geometry/fit/FitCalibrationNamingCatalog.h"
#include "../src/services/geometry/fit/FitCalibrationArtifactLocation.h"
#include "../src/services/geometry/fit/FrictionlessTechnicPinCalibrationArtifact.h"
#include "../src/services/geometry/fit/FrictionTechnicPinCalibrationArtifact.h"
#include "../src/services/geometry/fit/TechnicAxleCalibrationArtifact.h"
#include "../src/services/geometry/fit/TechnicAxleHoleCalibrationArtifact.h"
#include "../src/services/geometry/fit/FitCalibrationLibrary.h"
#include "../src/services/geometry/ThreeMfWriter.h"
#include "../src/services/geometry/LDrawLibraryService.h"
#include "../src/services/geometry/print/LDrawPrintGeometryBuilder.h"
#include "../src/services/geometry/print/PrintMeshAnalysis.h"
#include <lib3mf_implicit.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <cmath>

using namespace PrintGeometry;
namespace {
bool require(bool value,const QString&message){if(!value)QTextStream(stderr)<<"FAIL: "<<message<<Qt::endl;return value;}
FunctionalFeature fixture(){FunctionalFeature f;f.stableIdentity="fixture";f.family=FunctionalInterfaceFamily::RoundTechnicPassage;f.role=FunctionalInterfaceRole::Female;f.materialSide=FunctionalMaterialSide::EmptyInsideMaterialOutside;f.eligibility=FunctionalEligibility::Eligible;f.confidence=SemanticConfidence::HighConfidence;f.frame={{0,0,0},{0,1,0},{1,0,0},{0,0,-1},false};f.nominalRadiusMillimetres=2.40006;f.nominalDiameterMillimetres=4.80012;f.nominalAxialExtentMillimetres=8;f.nominalEngagementExtentMillimetres=6.4;f.radialProfile={{-4.1,3.20008},{-3.2,3.20008},{-3.2,2.40006},{3.2,2.40006},{3.2,3.20008},{4.1,3.20008}};f.operandAction=FunctionalOperandAction::Subtract;f.constructionRecipe="round-through-passage-v1";f.governingOperandIdentity="fixture:operand";f.evidenceContract="official-ldraw-peghole-pair-v1";return f;}
bool sameMesh(const PrintMesh&a,const PrintMesh&b){if(a.faces!=b.faces||a.vertices.size()!=b.vertices.size())return false;for(std::size_t i=0;i<a.vertices.size();++i)if(a.vertices[i].x!=b.vertices[i].x||a.vertices[i].y!=b.vertices[i].y||a.vertices[i].z!=b.vertices[i].z)return false;return true;}
QString wallPocketSessionName(bool brickDepth)
{
    const auto key = brickDepth ? FitCalibrationNameKey::ClutchWallPocketBrick :
                                  FitCalibrationNameKey::ClutchWallPocketPlate;
    const auto modelName = FitCalibrationArtifactLocation::fixtureFileName(key,QStringLiteral("coarse"),1);
    return QFileInfo(modelName).completeBaseName() + QStringLiteral("-session.json");
}
bool validateWallPocketSession(const QString& path, bool brickDepth)
{
    bool ok = true;
    QString error;
    QFile file(path);
    ok &= require(file.open(QIODevice::ReadOnly),QStringLiteral("Open WallPocket session %1").arg(path));
    if (!ok) return false;
    const auto document = QJsonDocument::fromJson(file.readAll());
    FitCalibrationSession decoded;
    const QString contract = brickDepth ? QStringLiteral("official-ldraw-box5-wall-pocket-brick-v1") :
                                          QStringLiteral("official-ldraw-box5-wall-pocket-plate-v1");
    ok &= require(document.isObject() && FitCalibrationSessionJson::fromJson(document.object(),&decoded,&error),
                  QStringLiteral("Parse WallPocket session through BrickSuite schema: %1").arg(error));
    if (!ok) return false;
    ok &= require(!decoded.sessionIdentity.isEmpty() && decoded.hasCoarseExperiment &&
                  !decoded.hasFineExperiment && decoded.history.isEmpty() &&
                  decoded.coarseExperiment.parentArtifactIdentity.isEmpty() &&
                  decoded.coarseExperiment.artifactIdentity ==
                      StudReceivingCalibrationArtifact::wallPocketArtifactIdentity(brickDepth) &&
                  decoded.coarseExperiment.regenerationPrototype.evidenceContract == contract &&
                  decoded.process.actualPrintedOrientation == FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate &&
                  decoded.coarseExperiment.candidates.size()==7 &&
                  decoded.coarseExperiment.preferredCandidateIndex==0 &&
                  decoded.coarseExperiment.state!=FitEvidenceState::Verified,
                  QStringLiteral("Depth, lineage, stage, orientation and unverified state: %1").arg(path));
    for (int i=0; i<decoded.coarseExperiment.candidates.size(); ++i) {
        const auto& candidate = decoded.coarseExperiment.candidates[i];
        ok &= require(candidate.index==i+1 && candidate.observations.isEmpty() &&
                      std::abs(candidate.diameterCorrectionMillimetres-(-.3+i*.1))<1e-9 &&
                      std::abs(candidate.functionalDiameterMillimetres-(4.5+i*.1))<1e-9,
                      QStringLiteral("Unobserved WallPocket candidate %1").arg(i+1));
    }
    QTemporaryDir temporary;
    FitCalibrationLibrary library(temporary.path());
    FitCalibrationWorkspace selected;
    selected.process.printerIdentity = QStringLiteral("Test printer");
    selected.process.materialIdentity = QStringLiteral("Test material");
    selected.process.profileName = QStringLiteral("Test process");
    selected.process.hasNozzleDiameter = true;
    selected.process.nozzleDiameterMillimetres = .4;
    selected.process.hasLayerHeight = true;
    selected.process.layerHeightMillimetres = .2;
    selected.process.dimensionalCompensationNotes = QStringLiteral("None");
    FitCalibrationSession anchor;
    anchor.hasCoarseExperiment = true;
    anchor.coarseExperiment = decoded.coarseExperiment;
    selected.featureSessions.push_back(anchor);
    FitCalibrationSession imported;
    ok &= require(library.importSessionIntoWorkspace(path,&selected,&imported,&error) &&
                  imported.sessionIdentity==decoded.sessionIdentity &&
                  imported.process.printerIdentity==selected.process.printerIdentity &&
                  imported.coarseExperiment.candidates.size()==7 &&
                  imported.coarseExperiment.candidates[3].observations.isEmpty(),
                  QStringLiteral("Import WallPocket session into selected manufacturing workspace: %1").arg(error));
    FitCalibrationSession resumed;
    ok &= require(library.loadSession(decoded.sessionIdentity,&resumed,&error) &&
                  resumed.coarseExperiment.regenerationPrototype.evidenceContract==contract &&
                  resumed.coarseExperiment.state!=FitEvidenceState::Verified,
                  QStringLiteral("Resume imported WallPocket session: %1").arg(error));
    return ok;
}
bool testWallPocket(const QStringList& args)
{
    bool ok = true;
    QString error;
    const int outputAt = args.indexOf(QStringLiteral("--wall-pocket-output"));
    for (bool brickDepth : {true,false}) {
        const auto fixture = StudReceivingCalibrationArtifact::generateWallPocket(brickDepth);
        const QString depth = brickDepth ? QStringLiteral("brick") : QStringLiteral("plate");
        ok &= require(fixture.ok && fixture.candidates.size()==7,
                      depth + " WallPocket seven-candidate fixture: " + fixture.diagnostic);
        if (!fixture.ok) continue;
        for (int i=0; i<7; ++i)
            ok &= require(std::abs(fixture.candidates[i].diameterCorrectionMillimetres-(-.3+i*.1))<1e-9 &&
                          std::abs(fixture.candidates[i].functionalDiameterMillimetres-(4.5+i*.1))<1e-9,
                          depth + " WallPocket candidate opening and correction");
        const auto bounds = fixture.analysis.bounds;
        ok &= require(fixture.analysis.connectedComponents==1 && fixture.analysis.boundaryEdges==0 &&
                      fixture.analysis.nonManifoldEdges==0 && fixture.analysis.selfIntersections==0 &&
                      std::abs(bounds.maximum.x-bounds.minimum.x-84.0)<1e-6 &&
                      std::abs(bounds.maximum.y-bounds.minimum.y-12.0)<1e-6 &&
                      std::abs(bounds.maximum.z-bounds.minimum.z-(brickDepth?10.0:3.6))<1e-6,
                      depth + " WallPocket is a compact, connected, manifold print mesh");
        const auto repeated = StudReceivingCalibrationArtifact::generateWallPocket(brickDepth);
        ok &= require(repeated.ok && sameMesh(fixture.mesh,repeated.mesh),
                      depth + " WallPocket fixture is deterministic");
        const auto experiment = StudReceivingCalibrationArtifact::observationTemplate(fixture);
        FitCalibrationExperiment restored;
        ok &= require(FitCalibrationExperimentJson::fromJson(FitCalibrationExperimentJson::toJson(experiment),
                      &restored,&error) && restored.regenerationPrototype.evidenceContract ==
                      fixture.regenerationPrototype.evidenceContract &&
                      restored.regenerationPrototype.materialSide==FunctionalMaterialSide::EmptyInsideMaterialOutside &&
                      restored.regenerationPrototype.operandAction==FunctionalOperandAction::Subtract &&
                      restored.state!=FitEvidenceState::Verified &&
                      restored.candidates[3].observations.isEmpty() &&
                      restored.process.orientationNotes.contains(QStringLiteral("open upward")),
                      depth + " WallPocket session retains depth, candidate, orientation and unverified state: " + error);
        StudReceivingCalibrationArtifactDefinition next;
        next.artifactIdentity = fixture.artifactIdentity + QStringLiteral("-fine-search-v2");
        next.parentArtifactIdentity = fixture.artifactIdentity;
        next.centerDiameterCorrectionMillimetres = .10;
        next.candidateSpacingMillimetres = .025;
        next.candidateCount = 5;
        const auto fine = StudReceivingCalibrationArtifact::generateWallPocket(fixture.regenerationPrototype,next);
        ok &= require(fine.ok && fine.candidates.size()==5 &&
                      std::abs(fine.candidates.front().functionalDiameterMillimetres-4.85)<1e-9 &&
                      std::abs(fine.candidates.back().functionalDiameterMillimetres-4.95)<1e-9 &&
                      StudReceivingCalibrationArtifact::observationTemplate(fine,next).parentArtifactIdentity==fixture.artifactIdentity,
                      depth + " WallPocket fine-search continuation keeps its own depth contract and lineage");
        if (outputAt >= 0 && outputAt+1 < args.size()) {
            QDir output(args[outputAt+1]);
            ok &= require(output.mkpath(QStringLiteral(".")),"WallPocket proof output directory");
            const auto key = brickDepth ? FitCalibrationNameKey::ClutchWallPocketBrick :
                                          FitCalibrationNameKey::ClutchWallPocketPlate;
            PrintMesh labeled;
            ok &= require(FitCalibrationFixtureLabel::recessStandalone(fixture.mesh,key,&labeled,nullptr,&error),
                          depth + " WallPocket fixture labeling: " + error);
            if (!labeled.faces.empty()) {
                const QString path = output.filePath(QStringLiteral("BrickSuite-wall-pocket-%1-perpendicular-coarse-v1.3mf").arg(depth));
                ThreeMfWriter::Options options;
                options.objectName=fixture.artifactIdentity;
                options.partIdentity=fixture.artifactIdentity;
                options.modelColor=QColor("#0055BF");
                ok &= require(ThreeMfWriter::write(labeled,path,options,&error),
                              depth + " WallPocket proof 3MF: " + error);
                QTextStream(stdout) << "wallPocket" << depth << "Fixture=" << path << Qt::endl;
            }
            const QString sessionPath = output.filePath(wallPocketSessionName(brickDepth));
            if (QFileInfo::exists(sessionPath)) {
                ok &= require(false,QStringLiteral("Refusing to overwrite WallPocket session: %1").arg(sessionPath));
            } else {
                QTemporaryDir managedRoot;
                FitCalibrationLibrary managed(managedRoot.path());
                FitCalibrationSession session;
                session.sessionIdentity = FitCalibrationLibrary::newStableIdentity();
                session.process.actualPrintedOrientation = experiment.process.actualPrintedOrientation;
                session.process.orientationNotes = experiment.process.orientationNotes;
                session.hasCoarseExperiment = true;
                session.coarseExperiment = experiment;
                session.coarseExperiment.process = session.process;
                ok &= require(managed.saveSession(&session,&error) &&
                              managed.exportSession(session.sessionIdentity,sessionPath,&error),
                              depth + " WallPocket managed-session export: " + error);
                if (QFileInfo::exists(sessionPath)) {
                    ok &= validateWallPocketSession(sessionPath,brickDepth);
                    QTextStream(stdout) << "wallPocket" << depth << "Session=" << sessionPath << Qt::endl;
                }
            }
        }
    }
    const int validateAt = args.indexOf(QStringLiteral("--wall-pocket-validate-directory"));
    if (validateAt >= 0 && validateAt+1 < args.size()) {
        const QDir directory(args[validateAt+1]);
        for (bool brickDepth : {true,false})
            ok &= validateWallPocketSession(directory.filePath(wallPocketSessionName(brickDepth)),brickDepth);
    }
    return ok;
}
bool validateAntiStudBoreSession(const QString& path)
{
    QString error;
    QFile file(path);
    if (!require(file.open(QIODevice::ReadOnly),"Open managed AntiStudBore session: "+path)) return false;
    FitCalibrationSession decoded;
    if (!require(FitCalibrationSessionJson::fromJson(QJsonDocument::fromJson(file.readAll()).object(),&decoded,&error),
                 "Decode managed AntiStudBore session: "+error)) return false;
    bool ok=require(!decoded.sessionIdentity.isEmpty()&&decoded.hasCoarseExperiment&&
                    !decoded.hasFineExperiment&&decoded.history.isEmpty()&&
                    decoded.coarseExperiment.parentArtifactIdentity.isEmpty()&&
                    decoded.coarseExperiment.artifactIdentity==StudReceivingCalibrationArtifact::antiStudBoreArtifactIdentity()&&
                    decoded.coarseExperiment.regenerationPrototype.evidenceContract==QStringLiteral("official-ldraw-stud4o-antistud-bore-v1")&&
                    decoded.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate&&
                    decoded.coarseExperiment.candidates.size()==7&&
                    decoded.coarseExperiment.preferredCandidateIndex==0&&
                    decoded.coarseExperiment.state!=FitEvidenceState::Verified,
                    "AntiStudBore managed identity, lineage, orientation, and unverified stage");
    for(int i=0;i<decoded.coarseExperiment.candidates.size();++i){
        const auto& candidate=decoded.coarseExperiment.candidates[i];
        ok&=require(candidate.index==i+1&&candidate.observations.isEmpty()&&
                    std::abs(candidate.diameterCorrectionMillimetres-(-.3+i*.1))<1e-9&&
                    std::abs(candidate.functionalDiameterMillimetres-(4.5+i*.1))<1e-9,
                    QStringLiteral("Unobserved AntiStudBore candidate %1").arg(i+1));
    }
    QTemporaryDir root;
    FitCalibrationLibrary library(root.path());
    FitCalibrationWorkspace selected;
    selected.process.printerIdentity=QStringLiteral("Test printer");
    selected.process.materialIdentity=QStringLiteral("Test material");
    selected.process.profileName=QStringLiteral("Test process");
    selected.process.hasNozzleDiameter=true;selected.process.nozzleDiameterMillimetres=.4;
    selected.process.hasLayerHeight=true;selected.process.layerHeightMillimetres=.2;
    selected.process.dimensionalCompensationNotes=QStringLiteral("None");
    FitCalibrationSession anchor;anchor.hasCoarseExperiment=true;anchor.coarseExperiment=decoded.coarseExperiment;
    selected.featureSessions.push_back(anchor);
    FitCalibrationSession imported;
    ok&=require(library.importSessionIntoWorkspace(path,&selected,&imported,&error)&&
                imported.sessionIdentity==decoded.sessionIdentity&&
                imported.coarseExperiment.candidates[3].observations.isEmpty(),
                "Import AntiStudBore into selected workspace: "+error);
    FitCalibrationSession resumed;
    ok&=require(library.loadSession(decoded.sessionIdentity,&resumed,&error)&&
                resumed.coarseExperiment.regenerationPrototype.evidenceContract==
                    QStringLiteral("official-ldraw-stud4o-antistud-bore-v1")&&
                resumed.coarseExperiment.state!=FitEvidenceState::Verified,
                "Resume managed AntiStudBore session: "+error);
    return ok;
}
bool testStandardBar(const QStringList& args)
{
    const auto artifact=StandardBarCalibrationArtifact::generate();
    bool ok=require(artifact.ok,"Standard Bar fixture generation: "+artifact.diagnostic);
    if(!ok) return false;
    ok&=require(artifact.candidates.size()==7 &&
                std::abs(artifact.candidates[0].functionalDiameterMillimetres-3.05)<1e-9 &&
                std::abs(artifact.candidates[3].functionalDiameterMillimetres-3.2)<1e-9 &&
                std::abs(artifact.candidates[6].functionalDiameterMillimetres-3.35)<1e-9,
                "Standard Bar seven candidates agree with 3.05-3.35 mm fixture");
    ok&=require(artifact.analysis.connectedComponents==1 && artifact.analysis.boundaryEdges==0 &&
                artifact.analysis.nonManifoldEdges==0 && artifact.analysis.selfIntersections==0,
                "Standard Bar fixture is manifold");
    const auto repeated=StandardBarCalibrationArtifact::generate();
    ok&=require(repeated.ok&&sameMesh(repeated.mesh,artifact.mesh),"Standard Bar fixture is deterministic");
    const auto experiment=StandardBarCalibrationArtifact::observationTemplate(artifact);
    FitCalibrationExperiment restored;
    QString error;
    ok&=require(FitCalibrationExperimentJson::fromJson(FitCalibrationExperimentJson::toJson(experiment),&restored,&error) &&
                restored.featureFamily==QStringLiteral("StandardBar") &&
                restored.regenerationPrototype.family==FunctionalInterfaceFamily::StandardBar &&
                restored.regenerationPrototype.evidenceContract==QStringLiteral("official-ldraw-capped-standard-bar-v1") &&
                restored.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate &&
                restored.preferredCandidateIndex==0 && restored.state!=FitEvidenceState::Verified,
                "Standard Bar managed schema retains contract, orientation, and unverified state: "+error);
    const auto validateSession=[&](const QString& path) {
        QFile file(path);
        if(!require(file.open(QIODevice::ReadOnly),"Open Standard Bar session: "+path)) return false;
        FitCalibrationSession decoded;
        if(!require(FitCalibrationSessionJson::fromJson(QJsonDocument::fromJson(file.readAll()).object(),&decoded,&error),
                    "Decode Standard Bar managed session: "+error)) return false;
        bool valid=require(!decoded.sessionIdentity.isEmpty() && decoded.hasCoarseExperiment &&
                           !decoded.hasFineExperiment && decoded.history.isEmpty() &&
                           decoded.coarseExperiment.parentArtifactIdentity.isEmpty() &&
                           decoded.coarseExperiment.artifactIdentity==StandardBarCalibrationArtifact::artifactIdentity() &&
                           decoded.coarseExperiment.regenerationPrototype.family==FunctionalInterfaceFamily::StandardBar &&
                           decoded.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate &&
                           decoded.coarseExperiment.candidates.size()==7 &&
                           decoded.coarseExperiment.preferredCandidateIndex==0 &&
                           decoded.coarseExperiment.state!=FitEvidenceState::Verified,
                           "Standard Bar session identity, lineage, orientation and unverified stage");
        for(int i=0;i<decoded.coarseExperiment.candidates.size();++i) {
            const auto& candidate=decoded.coarseExperiment.candidates[i];
            valid&=require(candidate.index==i+1 && candidate.observations.isEmpty() &&
                           std::abs(candidate.diameterCorrectionMillimetres-(-.15+i*.05))<1e-9 &&
                           std::abs(candidate.functionalDiameterMillimetres-(3.05+i*.05))<1e-9,
                           QStringLiteral("Standard Bar candidate %1 matches physical fixture").arg(i+1));
        }
        QTemporaryDir root;
        FitCalibrationLibrary library(root.path());
        FitCalibrationWorkspace workspace;
        workspace.process.printerIdentity=QStringLiteral("Test printer");
        workspace.process.materialIdentity=QStringLiteral("Test material");
        workspace.process.profileName=QStringLiteral("Test process");
        workspace.process.hasNozzleDiameter=true;
        workspace.process.nozzleDiameterMillimetres=.4;
        workspace.process.hasLayerHeight=true;
        workspace.process.layerHeightMillimetres=.2;
        workspace.process.dimensionalCompensationNotes=QStringLiteral("None");
        FitCalibrationSession imported;
        valid&=require(library.importSessionIntoWorkspace(path,&workspace,&imported,&error) &&
                       imported.sessionIdentity==decoded.sessionIdentity &&
                       imported.coarseExperiment.candidates[3].observations.isEmpty(),
                       "Import Standard Bar session into selected workspace: "+error);
        FitCalibrationSession resumed;
        valid&=require(library.loadSession(decoded.sessionIdentity,&resumed,&error) &&
                       resumed.coarseExperiment.regenerationPrototype.family==FunctionalInterfaceFamily::StandardBar &&
                       resumed.coarseExperiment.state!=FitEvidenceState::Verified,
                       "Resume managed Standard Bar session: "+error);
        return valid;
    };
    const int outputAt=args.indexOf(QStringLiteral("--standard-bar-output"));
    if(outputAt>=0 && outputAt+1<args.size()) {
        QDir output(args[outputAt+1]);
        ok&=require(output.mkpath(QStringLiteral(".")),"Standard Bar output directory");
        const QString fixturePath=output.filePath(FitCalibrationArtifactLocation::fixtureFileName(
            FitCalibrationNameKey::BarDiameter,QStringLiteral("coarse"),1));
        const QString sessionPath=FitCalibrationArtifactLocation::companionPath(fixturePath);
        if(QFileInfo::exists(fixturePath)||QFileInfo::exists(sessionPath))
            ok&=require(false,"Refusing to overwrite existing Standard Bar calibration artifact");
        else {
            PrintMesh labeled;
            ok&=require(FitCalibrationFixtureLabel::recessStandalone(artifact.mesh,
                FitCalibrationNameKey::BarDiameter,&labeled,nullptr,&error),"Standard Bar label: "+error);
            if(!labeled.faces.empty()) {
                ThreeMfWriter::Options options;
                options.objectName=artifact.artifactIdentity;
                options.partIdentity=artifact.artifactIdentity;
                options.modelColor=QColor("#A0A5A9");
                ok&=require(ThreeMfWriter::write(labeled,fixturePath,options,&error),
                            "Standard Bar 3MF export: "+error);
            }
            QTemporaryDir managedRoot;
            FitCalibrationLibrary managed(managedRoot.path());
            FitCalibrationSession session;
            session.sessionIdentity=FitCalibrationLibrary::newStableIdentity();
            session.process.actualPrintedOrientation=experiment.process.actualPrintedOrientation;
            session.process.orientationNotes=experiment.process.orientationNotes;
            session.hasCoarseExperiment=true;
            session.coarseExperiment=experiment;
            session.coarseExperiment.process=session.process;
            ok&=require(managed.saveSession(&session,&error) &&
                        managed.exportSession(session.sessionIdentity,sessionPath,&error),
                        "Standard Bar managed-session export: "+error);
            if(QFileInfo::exists(sessionPath)) ok&=validateSession(sessionPath);
            QTextStream(stdout)<<"standardBarFixture="<<fixturePath<<Qt::endl
                               <<"standardBarSession="<<sessionPath<<Qt::endl;
        }
    }
    const int validateAt=args.indexOf(QStringLiteral("--standard-bar-validate-session"));
    if(validateAt>=0&&validateAt+1<args.size()) ok&=validateSession(args[validateAt+1]);
    return ok;
}
bool testBallJoint(const QStringList& args)
{
    const auto artifact=BallJointCalibrationArtifact::generate();
    bool ok=require(artifact.ok,"Ball Joint fixture generation: "+artifact.diagnostic);
    if(!ok)return false;
    ok&=require(artifact.candidates.size()==7 &&
                std::abs(artifact.candidates.front().functionalDiameterMillimetres-6.10)<1e-9 &&
                std::abs(artifact.candidates[3].functionalDiameterMillimetres-6.40)<1e-9 &&
                std::abs(artifact.candidates.back().functionalDiameterMillimetres-6.70)<1e-9 &&
                artifact.analysis.connectedComponents==1 && artifact.analysis.boundaryEdges==0 &&
                artifact.analysis.nonManifoldEdges==0 && artifact.analysis.selfIntersections==0,
                "Ball Joint seven-candidate spherical fixture is manifold and includes nominal #4");
    const auto repeated=BallJointCalibrationArtifact::generate();
    ok&=require(repeated.ok&&sameMesh(repeated.mesh,artifact.mesh),"Ball Joint fixture is deterministic");
    const auto experiment=BallJointCalibrationArtifact::observationTemplate(artifact);
    FitCalibrationExperiment restored;
    QString error;
    ok&=require(FitCalibrationExperimentJson::fromJson(
                    FitCalibrationExperimentJson::toJson(experiment),&restored,&error) &&
                restored.featureFamily==QStringLiteral("BallJoint") &&
                restored.regenerationPrototype.family==FunctionalInterfaceFamily::BallJoint &&
                restored.regenerationPrototype.evidenceContract==QStringLiteral("official-ldraw-joint8ball-sphere-v1") &&
                restored.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate &&
                restored.preferredCandidateIndex==0 && restored.state!=FitEvidenceState::Verified,
                "Ball Joint experiment round-trips as unobserved perpendicular evidence: "+error);
    const auto validateSession=[&](const QString& path,const BallJointCalibrationResult& expected,
                                   FitPrintedOrientation orientation,double firstCorrection,double spacing) {
        QFile file(path);
        if(!require(file.open(QIODevice::ReadOnly),"Open Ball Joint session: "+path))return false;
        FitCalibrationSession decoded;
        if(!require(FitCalibrationSessionJson::fromJson(QJsonDocument::fromJson(file.readAll()).object(),
                    &decoded,&error),"Decode Ball Joint session: "+error))return false;
        bool valid=require(!decoded.sessionIdentity.isEmpty() && decoded.hasCoarseExperiment &&
                           !decoded.hasFineExperiment && decoded.history.isEmpty() &&
                           decoded.coarseExperiment.artifactIdentity==expected.artifactIdentity &&
                           decoded.coarseExperiment.parentArtifactIdentity.isEmpty() &&
                           decoded.coarseExperiment.candidates.size()==7 &&
                           decoded.coarseExperiment.preferredCandidateIndex==0 &&
                           decoded.coarseExperiment.state!=FitEvidenceState::Verified &&
                           decoded.process.actualPrintedOrientation==orientation,
                           "Ball Joint session identity, lineage and unverified orientation");
        for(int i=0;i<decoded.coarseExperiment.candidates.size();++i) {
            const auto& candidate=decoded.coarseExperiment.candidates[i];
            valid&=require(candidate.index==i+1 && candidate.observations.isEmpty() &&
                           std::abs(candidate.diameterCorrectionMillimetres-(firstCorrection+i*spacing))<1e-9 &&
                           std::abs(candidate.functionalDiameterMillimetres-(6.4+firstCorrection+i*spacing))<1e-9,
                           QStringLiteral("Ball Joint candidate %1 matches printed fixture").arg(i+1));
        }
        QTemporaryDir root;
        FitCalibrationLibrary library(root.path());
        FitCalibrationWorkspace workspace;
        workspace.process.printerIdentity=QStringLiteral("Test printer");
        workspace.process.materialIdentity=QStringLiteral("Test material");
        workspace.process.profileName=QStringLiteral("Test process");
        workspace.process.hasNozzleDiameter=true;
        workspace.process.nozzleDiameterMillimetres=.4;
        workspace.process.hasLayerHeight=true;
        workspace.process.layerHeightMillimetres=.2;
        workspace.process.dimensionalCompensationNotes=QStringLiteral("None");
        FitCalibrationSession imported,resumed;
        valid&=require(library.importSessionIntoWorkspace(path,&workspace,&imported,&error) &&
                       imported.sessionIdentity==decoded.sessionIdentity &&
                       library.loadSession(decoded.sessionIdentity,&resumed,&error) &&
                       resumed.coarseExperiment.regenerationPrototype.family==FunctionalInterfaceFamily::BallJoint &&
                       resumed.coarseExperiment.candidates[3].observations.isEmpty(),
                       "Ball Joint managed session imports and resumes without evidence: "+error);
        return valid;
    };
    const int outputAt=args.indexOf(QStringLiteral("--ball-joint-output"));
    if(outputAt>=0&&outputAt+1<args.size()) {
        QDir output(args[outputAt+1]);
        ok&=require(output.mkpath(QStringLiteral(".")),"Ball Joint output directory");
        const QString fixturePath=output.filePath(FitCalibrationArtifactLocation::fixtureFileName(
            FitCalibrationNameKey::BallJointDiameter,QStringLiteral("coarse"),1));
        const QString sessionPath=FitCalibrationArtifactLocation::companionPath(fixturePath);
        if(QFileInfo::exists(fixturePath)||QFileInfo::exists(sessionPath))
            ok&=require(false,"Refusing to overwrite existing Ball Joint calibration artifacts");
        else {
            PrintMesh labeled;
            ok&=require(FitCalibrationFixtureLabel::recessStandalone(artifact.mesh,
                FitCalibrationNameKey::BallJointDiameter,&labeled,nullptr,&error),"Ball Joint label: "+error);
            ok&=require(validatePreparedMesh(analyzeSource(labeled)).ok(),
                        "labeled Ball Joint fixture remains one strict manifold");
            if(!labeled.faces.empty()) {
                ThreeMfWriter::Options options;
                options.objectName=artifact.artifactIdentity;
                options.partIdentity=artifact.artifactIdentity;
                options.modelColor=QColor("#A0A5A9");
                ok&=require(ThreeMfWriter::write(labeled,fixturePath,options,&error),
                            "Ball Joint 3MF export: "+error);
                if(QFileInfo::exists(fixturePath)) {
                    Lib3MF::CWrapper wrapper;
                    auto model=wrapper.CreateModel();
                    model->QueryReader("3mf")->ReadFromFile(fixturePath.toStdString());
                    auto meshes=model->GetMeshObjects();
                    ok&=require(meshes->MoveNext() &&
                                meshes->GetCurrentMeshObject()->GetTriangleCount()==labeled.faces.size(),
                                "Ball Joint 3MF independently reopens with the labeled fixture mesh");
                }
            }
            QTemporaryDir managedRoot;
            FitCalibrationLibrary managed(managedRoot.path());
            FitCalibrationSession session;
            session.sessionIdentity=FitCalibrationLibrary::newStableIdentity();
            session.process.actualPrintedOrientation=experiment.process.actualPrintedOrientation;
            session.process.orientationNotes=experiment.process.orientationNotes;
            session.hasCoarseExperiment=true;
            session.coarseExperiment=experiment;
            session.coarseExperiment.process=session.process;
            ok&=require(managed.saveSession(&session,&error) &&
                        managed.exportSession(session.sessionIdentity,sessionPath,&error),
                        "Ball Joint managed-session export: "+error);
            if(QFileInfo::exists(sessionPath))ok&=validateSession(sessionPath,artifact,
                FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate,-.30,.10);
            QTextStream(stdout)<<"ballJointFixture="<<fixturePath<<Qt::endl
                               <<"ballJointSession="<<sessionPath<<Qt::endl;
        }
    }
    const int validateAt=args.indexOf(QStringLiteral("--ball-joint-validate-session"));
    if(validateAt>=0&&validateAt+1<args.size())ok&=validateSession(args[validateAt+1],artifact,
        FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate,-.30,.10);
    BallJointCalibrationDefinition parallelDefinition;
    parallelDefinition.orientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;
    parallelDefinition.centerCorrectionMillimetres=-.15;
    parallelDefinition.spacingMillimetres=.15;
    const auto parallel=BallJointCalibrationArtifact::generate(parallelDefinition);
    ok&=require(parallel.ok,"parallel Ball Joint fixture generation: "+parallel.diagnostic);
    if(!parallel.ok)return false;
    ok&=require(parallel.artifactIdentity!=artifact.artifactIdentity &&
                parallel.candidates.size()==7 &&
                std::abs(parallel.candidates.front().functionalDiameterMillimetres-5.80)<1e-9 &&
                std::abs(parallel.candidates[1].functionalDiameterMillimetres-5.95)<1e-9 &&
                std::abs(parallel.candidates[4].functionalDiameterMillimetres-6.40)<1e-9 &&
                std::abs(parallel.candidates.back().functionalDiameterMillimetres-6.70)<1e-9 &&
                parallel.analysis.connectedComponents==1 && parallel.analysis.boundaryEdges==0 &&
                parallel.analysis.nonManifoldEdges==0 && parallel.analysis.selfIntersections==0,
                "parallel Ball Joint has distinct manifold seven-candidate range with nominal #5");
    const auto parallelExperiment=BallJointCalibrationArtifact::observationTemplate(parallel,parallelDefinition);
    ok&=require(parallelExperiment.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate &&
                parallelExperiment.preferredCandidateIndex==0 && parallelExperiment.state!=FitEvidenceState::Verified &&
                FitCalibrationNamingCatalog::keyFor(parallelExperiment,parallelExperiment.process.actualPrintedOrientation)==
                    FitCalibrationNameKey::BallJointDiameterParallel,
                "parallel Ball Joint session has distinct identity and no physical evidence");
    const int parallelOutputAt=args.indexOf(QStringLiteral("--ball-joint-parallel-output"));
    if(parallelOutputAt>=0 && parallelOutputAt+1<args.size()) {
        QDir output(args[parallelOutputAt+1]);
        ok&=require(output.mkpath(QStringLiteral(".")),"parallel Ball Joint output directory");
        const QString fixturePath=output.filePath(FitCalibrationArtifactLocation::fixtureFileName(
            FitCalibrationNameKey::BallJointDiameterParallel,QStringLiteral("coarse"),1));
        const QString sessionPath=FitCalibrationArtifactLocation::companionPath(fixturePath);
        if(QFileInfo::exists(fixturePath)||QFileInfo::exists(sessionPath))
            ok&=require(false,"Refusing to overwrite existing parallel Ball Joint artifacts");
        else {
            PrintMesh labeled;
            ok&=require(FitCalibrationFixtureLabel::recessStandalone(parallel.mesh,
                FitCalibrationNameKey::BallJointDiameterParallel,&labeled,nullptr,&error),
                "parallel Ball Joint label: "+error);
            ok&=require(validatePreparedMesh(analyzeSource(labeled)).ok(),
                        "labeled parallel Ball Joint fixture remains strict manifold");
            if(!labeled.faces.empty()) {
                ThreeMfWriter::Options options;
                options.objectName=parallel.artifactIdentity;
                options.partIdentity=parallel.artifactIdentity;
                options.modelColor=QColor("#A0A5A9");
                ok&=require(ThreeMfWriter::write(labeled,fixturePath,options,&error),
                            "parallel Ball Joint 3MF export: "+error);
                if(QFileInfo::exists(fixturePath)) {
                    Lib3MF::CWrapper wrapper;
                    auto model=wrapper.CreateModel();
                    model->QueryReader("3mf")->ReadFromFile(fixturePath.toStdString());
                    auto meshes=model->GetMeshObjects();
                    ok&=require(meshes->MoveNext() &&
                                meshes->GetCurrentMeshObject()->GetTriangleCount()==labeled.faces.size(),
                                "parallel Ball Joint 3MF independently reopens");
                }
            }
            QTemporaryDir managedRoot;
            FitCalibrationLibrary managed(managedRoot.path());
            FitCalibrationSession session;
            session.sessionIdentity=FitCalibrationLibrary::newStableIdentity();
            session.process.actualPrintedOrientation=parallelExperiment.process.actualPrintedOrientation;
            session.process.orientationNotes=parallelExperiment.process.orientationNotes;
            session.hasCoarseExperiment=true;
            session.coarseExperiment=parallelExperiment;
            session.coarseExperiment.process=session.process;
            ok&=require(managed.saveSession(&session,&error) &&
                        managed.exportSession(session.sessionIdentity,sessionPath,&error),
                        "parallel Ball Joint managed-session export: "+error);
            if(QFileInfo::exists(sessionPath))ok&=validateSession(sessionPath,parallel,
                FitPrintedOrientation::FeatureAxisParallelToBuildPlate,-.60,.15);
            QTextStream(stdout)<<"ballJointParallelFixture="<<fixturePath<<Qt::endl
                               <<"ballJointParallelSession="<<sessionPath<<Qt::endl;
        }
    }
    const int parallelValidateAt=args.indexOf(QStringLiteral("--ball-joint-parallel-validate-session"));
    if(parallelValidateAt>=0 && parallelValidateAt+1<args.size())ok&=validateSession(args[parallelValidateAt+1],parallel,
        FitPrintedOrientation::FeatureAxisParallelToBuildPlate,-.60,.15);
    return ok;
}
bool testCClipBarReceiver(const QStringList& args)
{
    const auto artifact=CClipBarReceiverCalibrationArtifact::generate();
    bool ok=require(artifact.ok,"C-Clip fixture generation: "+artifact.diagnostic);
    if(!ok)return false;
    ok&=require(artifact.candidates.size()==7 &&
                std::abs(artifact.candidates[0].functionalDiameterMillimetres-3.05)<1e-9 &&
                std::abs(artifact.candidates[3].functionalDiameterMillimetres-3.20)<1e-9 &&
                std::abs(artifact.candidates[6].functionalDiameterMillimetres-3.35)<1e-9 &&
                artifact.analysis.connectedComponents==1 && artifact.analysis.boundaryEdges==0 &&
                artifact.analysis.nonManifoldEdges==0 && artifact.analysis.selfIntersections==0,
                "C-Clip seven-candidate contact arc/throat fixture is manifold and includes nominal #4");
    const auto repeated=CClipBarReceiverCalibrationArtifact::generate();
    ok&=require(repeated.ok && sameMesh(repeated.mesh,artifact.mesh),"C-Clip fixture is deterministic");
    const auto experiment=CClipBarReceiverCalibrationArtifact::observationTemplate(artifact);
    FitCalibrationExperiment restored;
    QString error;
    ok&=require(FitCalibrationExperimentJson::fromJson(
                    FitCalibrationExperimentJson::toJson(experiment),&restored,&error) &&
                restored.featureFamily==QStringLiteral("CClipBarReceiver") &&
                restored.regenerationPrototype.family==FunctionalInterfaceFamily::CClipBarReceiver &&
                restored.regenerationPrototype.evidenceContract==QStringLiteral("official-ldraw-clip6-bar-receiver-v1") &&
                restored.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate &&
                restored.preferredCandidateIndex==0 && restored.state!=FitEvidenceState::Verified,
                "C-Clip managed experiment round-trips as unobserved perpendicular evidence: "+error);
    const auto validateSession=[&](const QString& path,FitPrintedOrientation orientation,
                                   const QString& identity) {
        QFile file(path);
        if(!require(file.open(QIODevice::ReadOnly),"Open C-Clip session: "+path))return false;
        FitCalibrationSession decoded;
        if(!require(FitCalibrationSessionJson::fromJson(QJsonDocument::fromJson(file.readAll()).object(),
                    &decoded,&error),"Decode C-Clip session: "+error))return false;
        bool valid=require(!decoded.sessionIdentity.isEmpty() && decoded.hasCoarseExperiment &&
                           !decoded.hasFineExperiment && decoded.history.isEmpty() &&
                           decoded.coarseExperiment.artifactIdentity==identity &&
                           decoded.coarseExperiment.parentArtifactIdentity.isEmpty() &&
                           decoded.coarseExperiment.candidates.size()==7 &&
                           decoded.coarseExperiment.preferredCandidateIndex==0 &&
                           decoded.coarseExperiment.state!=FitEvidenceState::Verified &&
                           decoded.process.actualPrintedOrientation==orientation,
                           "C-Clip session identity, lineage and unverified orientation");
        for(int i=0;i<decoded.coarseExperiment.candidates.size();++i) {
            const auto& candidate=decoded.coarseExperiment.candidates[i];
            valid&=require(candidate.index==i+1 && candidate.observations.isEmpty() &&
                           std::abs(candidate.diameterCorrectionMillimetres-(-.15+i*.05))<1e-9 &&
                           std::abs(candidate.functionalDiameterMillimetres-(3.05+i*.05))<1e-9,
                           QStringLiteral("C-Clip candidate %1 matches printed fixture").arg(i+1));
        }
        QTemporaryDir root;
        FitCalibrationLibrary library(root.path());
        FitCalibrationWorkspace workspace;
        workspace.process.printerIdentity=QStringLiteral("Test printer");
        workspace.process.materialIdentity=QStringLiteral("Test material");
        workspace.process.profileName=QStringLiteral("Test process");
        workspace.process.hasNozzleDiameter=true;
        workspace.process.nozzleDiameterMillimetres=.4;
        workspace.process.hasLayerHeight=true;
        workspace.process.layerHeightMillimetres=.2;
        workspace.process.dimensionalCompensationNotes=QStringLiteral("None");
        FitCalibrationSession imported,resumed;
        valid&=require(library.importSessionIntoWorkspace(path,&workspace,&imported,&error) &&
                       imported.sessionIdentity==decoded.sessionIdentity &&
                       library.loadSession(decoded.sessionIdentity,&resumed,&error) &&
                       resumed.coarseExperiment.regenerationPrototype.family==FunctionalInterfaceFamily::CClipBarReceiver &&
                       resumed.coarseExperiment.candidates[3].observations.isEmpty(),
                       "C-Clip managed session imports and resumes without evidence: "+error);
        return valid;
    };
    const int outputAt=args.indexOf(QStringLiteral("--c-clip-output"));
    if(outputAt>=0 && outputAt+1<args.size()) {
        QDir output(args[outputAt+1]);
        ok&=require(output.mkpath(QStringLiteral(".")),"C-Clip output directory");
        const QString fixturePath=output.filePath(FitCalibrationArtifactLocation::fixtureFileName(
            FitCalibrationNameKey::CClipBarReceiverClearance,QStringLiteral("coarse"),1));
        const QString sessionPath=FitCalibrationArtifactLocation::companionPath(fixturePath);
        if(QFileInfo::exists(fixturePath)||QFileInfo::exists(sessionPath))
            ok&=require(false,"Refusing to overwrite existing C-Clip calibration artifacts");
        else {
            PrintMesh labeled;
            ok&=require(FitCalibrationFixtureLabel::recessStandalone(artifact.mesh,
                FitCalibrationNameKey::CClipBarReceiverClearance,&labeled,nullptr,&error),"C-Clip label: "+error);
            ok&=require(validatePreparedMesh(analyzeSource(labeled)).ok(),
                        "labeled C-Clip fixture remains one strict manifold");
            if(!labeled.faces.empty()) {
                ThreeMfWriter::Options options;
                options.objectName=artifact.artifactIdentity;
                options.partIdentity=artifact.artifactIdentity;
                options.modelColor=QColor("#A0A5A9");
                ok&=require(ThreeMfWriter::write(labeled,fixturePath,options,&error),
                            "C-Clip 3MF export: "+error);
                if(QFileInfo::exists(fixturePath)) {
                    Lib3MF::CWrapper wrapper;
                    auto model=wrapper.CreateModel();
                    model->QueryReader("3mf")->ReadFromFile(fixturePath.toStdString());
                    auto meshes=model->GetMeshObjects();
                    ok&=require(meshes->MoveNext() &&
                                meshes->GetCurrentMeshObject()->GetTriangleCount()==labeled.faces.size(),
                                "C-Clip 3MF independently reopens with the validated labeled mesh");
                }
            }
            QTemporaryDir managedRoot;
            FitCalibrationLibrary managed(managedRoot.path());
            FitCalibrationSession session;
            session.sessionIdentity=FitCalibrationLibrary::newStableIdentity();
            session.process.actualPrintedOrientation=experiment.process.actualPrintedOrientation;
            session.process.orientationNotes=experiment.process.orientationNotes;
            session.hasCoarseExperiment=true;
            session.coarseExperiment=experiment;
            session.coarseExperiment.process=session.process;
            ok&=require(managed.saveSession(&session,&error) &&
                        managed.exportSession(session.sessionIdentity,sessionPath,&error),
                        "C-Clip managed-session export: "+error);
            if(QFileInfo::exists(sessionPath))ok&=validateSession(sessionPath,
                FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate,artifact.artifactIdentity);
            QTextStream(stdout)<<"cClipFixture="<<fixturePath<<Qt::endl
                               <<"cClipSession="<<sessionPath<<Qt::endl;
        }
    }
    const int validateAt=args.indexOf(QStringLiteral("--c-clip-validate-session"));
    if(validateAt>=0 && validateAt+1<args.size())ok&=validateSession(args[validateAt+1],
        FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate,artifact.artifactIdentity);
    CClipBarReceiverCalibrationDefinition parallelDefinition;
    parallelDefinition.orientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;
    const auto parallel=CClipBarReceiverCalibrationArtifact::generate(parallelDefinition);
    ok&=require(parallel.ok,"parallel C-Clip fixture generation: "+parallel.diagnostic);
    if(!parallel.ok)return false;
    ok&=require(parallel.artifactIdentity!=artifact.artifactIdentity &&
                parallel.candidates.size()==7 && parallel.candidates[3].functionalDiameterMillimetres==3.2 &&
                parallel.analysis.connectedComponents==1 && parallel.analysis.boundaryEdges==0 &&
                parallel.analysis.nonManifoldEdges==0 && parallel.analysis.selfIntersections==0,
                "parallel C-Clip is a separate manifold seven-candidate fixture");
    const auto parallelExperiment=CClipBarReceiverCalibrationArtifact::observationTemplate(parallel,parallelDefinition);
    ok&=require(parallelExperiment.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate &&
                parallelExperiment.preferredCandidateIndex==0 && parallelExperiment.state!=FitEvidenceState::Verified &&
                FitCalibrationNamingCatalog::keyFor(parallelExperiment,parallelExperiment.process.actualPrintedOrientation)==
                    FitCalibrationNameKey::CClipBarReceiverClearanceParallel,
                "parallel C-Clip session has separate identity and no physical evidence");
    const int parallelOutputAt=args.indexOf(QStringLiteral("--c-clip-parallel-output"));
    if(parallelOutputAt>=0 && parallelOutputAt+1<args.size()) {
        QDir output(args[parallelOutputAt+1]);
        ok&=require(output.mkpath(QStringLiteral(".")),"parallel C-Clip output directory");
        const QString fixturePath=output.filePath(FitCalibrationArtifactLocation::fixtureFileName(
            FitCalibrationNameKey::CClipBarReceiverClearanceParallel,QStringLiteral("coarse"),1));
        const QString sessionPath=FitCalibrationArtifactLocation::companionPath(fixturePath);
        if(QFileInfo::exists(fixturePath)||QFileInfo::exists(sessionPath))
            ok&=require(false,"Refusing to overwrite existing parallel C-Clip artifacts");
        else {
            PrintMesh labeled;
            ok&=require(FitCalibrationFixtureLabel::recessStandalone(parallel.mesh,
                FitCalibrationNameKey::CClipBarReceiverClearanceParallel,&labeled,nullptr,&error),
                "parallel C-Clip label: "+error);
            ok&=require(validatePreparedMesh(analyzeSource(labeled)).ok(),
                        "labeled parallel C-Clip fixture remains strict manifold");
            if(!labeled.faces.empty()) {
                ThreeMfWriter::Options options;
                options.objectName=parallel.artifactIdentity;
                options.partIdentity=parallel.artifactIdentity;
                options.modelColor=QColor("#A0A5A9");
                ok&=require(ThreeMfWriter::write(labeled,fixturePath,options,&error),
                            "parallel C-Clip 3MF export: "+error);
                if(QFileInfo::exists(fixturePath)) {
                    Lib3MF::CWrapper wrapper;
                    auto model=wrapper.CreateModel();
                    model->QueryReader("3mf")->ReadFromFile(fixturePath.toStdString());
                    auto meshes=model->GetMeshObjects();
                    ok&=require(meshes->MoveNext() &&
                                meshes->GetCurrentMeshObject()->GetTriangleCount()==labeled.faces.size(),
                                "parallel C-Clip 3MF independently reopens");
                }
            }
            QTemporaryDir managedRoot;
            FitCalibrationLibrary managed(managedRoot.path());
            FitCalibrationSession session;
            session.sessionIdentity=FitCalibrationLibrary::newStableIdentity();
            session.process.actualPrintedOrientation=parallelExperiment.process.actualPrintedOrientation;
            session.process.orientationNotes=parallelExperiment.process.orientationNotes;
            session.hasCoarseExperiment=true;
            session.coarseExperiment=parallelExperiment;
            session.coarseExperiment.process=session.process;
            ok&=require(managed.saveSession(&session,&error) &&
                        managed.exportSession(session.sessionIdentity,sessionPath,&error),
                        "parallel C-Clip managed-session export: "+error);
            if(QFileInfo::exists(sessionPath))ok&=validateSession(sessionPath,
                FitPrintedOrientation::FeatureAxisParallelToBuildPlate,parallel.artifactIdentity);
            QTextStream(stdout)<<"cClipParallelFixture="<<fixturePath<<Qt::endl
                               <<"cClipParallelSession="<<sessionPath<<Qt::endl;
        }
    }
    const int parallelValidateAt=args.indexOf(QStringLiteral("--c-clip-parallel-validate-session"));
    if(parallelValidateAt>=0 && parallelValidateAt+1<args.size())ok&=validateSession(args[parallelValidateAt+1],
        FitPrintedOrientation::FeatureAxisParallelToBuildPlate,parallel.artifactIdentity);
    return ok;
}
bool testAntiStudBore(const QStringList& args)
{
    bool ok=true;
    const auto artifact=StudReceivingCalibrationArtifact::generateAntiStudBore();
    ok&=require(artifact.ok,"distinct AntiStudBore fixture generation: "+artifact.diagnostic);
    if(!artifact.ok)return false;
    ok&=require(artifact.candidates.size()==7 && artifact.candidates[3].functionalDiameterMillimetres==4.8 &&
                std::abs(artifact.candidates.front().functionalDiameterMillimetres-4.5)<1e-9 &&
                std::abs(artifact.candidates.back().functionalDiameterMillimetres-5.1)<1e-9,
                "AntiStudBore centered opening candidates span 4.50-5.10 mm with nominal #4");
    ok&=require(artifact.analysis.connectedComponents==1 && artifact.analysis.boundaryEdges==0 &&
                artifact.analysis.nonManifoldEdges==0 && artifact.analysis.selfIntersections==0,
                "AntiStudBore shallow fixture is a validated closed printable mesh");
    const auto experiment=StudReceivingCalibrationArtifact::observationTemplate(artifact);
    ok&=require(experiment.regenerationPrototype.evidenceContract==QStringLiteral("official-ldraw-stud4o-antistud-bore-v1") &&
                experiment.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate &&
                experiment.preferredCandidateIndex==0 && experiment.state!=FitEvidenceState::Verified &&
                experiment.process.orientationNotes.contains(QStringLiteral("entering its center")),
                "AntiStudBore session begins unobserved and distinctly labeled");
    QString error;FitCalibrationExperiment restored;
    ok&=require(FitCalibrationExperimentJson::fromJson(FitCalibrationExperimentJson::toJson(experiment),&restored,&error) &&
                restored.regenerationPrototype.constructionRecipe==QStringLiteral("stud-receiving-antistud-bore-v1"),
                "AntiStudBore source contract survives existing session JSON round-trip: "+error);
    const auto repeated=StudReceivingCalibrationArtifact::generateAntiStudBore();
    ok&=require(repeated.ok&&sameMesh(repeated.mesh,artifact.mesh),"AntiStudBore fixture is deterministic");
    const int outputAt=args.indexOf(QStringLiteral("--anti-stud-output"));
    if(outputAt>=0&&outputAt+1<args.size()){
        QDir output(args[outputAt+1]);
        ok&=require(output.mkpath(QStringLiteral(".")),"AntiStudBore proof output directory");
        const QString fixturePath=output.filePath(FitCalibrationArtifactLocation::fixtureFileName(
            FitCalibrationNameKey::ClutchAntiStudBore,QStringLiteral("coarse"),1));
        const QString sessionPath=FitCalibrationArtifactLocation::companionPath(fixturePath);
        if(QFileInfo::exists(fixturePath)||QFileInfo::exists(sessionPath)){
            ok&=require(false,"Refusing to overwrite existing AntiStudBore calibration artifact");
        }else{
            PrintMesh labeled;
            ok&=require(FitCalibrationFixtureLabel::recessStandalone(artifact.mesh,
                FitCalibrationNameKey::ClutchAntiStudBore,&labeled,nullptr,&error),
                "AntiStudBore fixture label: "+error);
            if(!labeled.faces.empty()){
                ThreeMfWriter::Options options;
                options.objectName=artifact.artifactIdentity;
                options.partIdentity=artifact.artifactIdentity;
                options.modelColor=QColor("#0055BF");
                ok&=require(ThreeMfWriter::write(labeled,fixturePath,options,&error),
                            "AntiStudBore 3MF export: "+error);
            }
            QTemporaryDir managedRoot;
            FitCalibrationLibrary managed(managedRoot.path());
            FitCalibrationSession session;
            session.sessionIdentity=FitCalibrationLibrary::newStableIdentity();
            session.process.actualPrintedOrientation=experiment.process.actualPrintedOrientation;
            session.process.orientationNotes=experiment.process.orientationNotes;
            session.hasCoarseExperiment=true;
            session.coarseExperiment=experiment;
            session.coarseExperiment.process=session.process;
            ok&=require(managed.saveSession(&session,&error)&&
                        managed.exportSession(session.sessionIdentity,sessionPath,&error),
                        "AntiStudBore managed-session export: "+error);
            if(QFileInfo::exists(sessionPath))ok&=validateAntiStudBoreSession(sessionPath);
            QTextStream(stdout)<<"antiStudFixture="<<fixturePath<<Qt::endl
                               <<"antiStudSession="<<sessionPath<<Qt::endl;
        }
    }
    const int validateAt=args.indexOf(QStringLiteral("--anti-stud-validate-session"));
    if(validateAt>=0&&validateAt+1<args.size())
        ok&=validateAntiStudBoreSession(args[validateAt+1]);
    return ok;
}
}

int main(int argc,char**argv){QCoreApplication app(argc,argv);bool ok=true;const auto artifact=RoundTechnicCalibrationArtifact::generate(fixture());ok&=require(artifact.ok,"synthetic calibration artifact generation");ok&=require(artifact.artifactIdentity=="round-technic-female-horizontal-v1"&&artifact.orientationIdentity=="flat-base-horizontal-axis-y-v1","versioned artifact and orientation identity");ok&=require(artifact.candidates.size()==7,"seven independent candidates");const auto expected=RoundTechnicCalibrationArtifact::diameterCorrectionsMillimetres();for(int i=0;i<expected.size();++i){ok&=require(std::abs(artifact.candidates[i].diameterCorrectionMillimetres-expected[i])<1e-12,"ordered correction map");ok&=require(std::abs(artifact.candidates[i].functionalDiameterMillimetres-(4.80012+expected[i]))<1e-9,"actual candidate diameter");}ok&=require(artifact.analysis.connectedComponents==1&&artifact.analysis.boundaryEdges==0&&artifact.analysis.nonManifoldEdges==0&&artifact.analysis.nonManifoldVertices==0&&artifact.analysis.selfIntersections==0,"printable topology");const auto bounds=artifact.analysis.bounds;ok&=require(std::abs((bounds.maximum.x-bounds.minimum.x)-92.0)<1e-6&&std::abs((bounds.maximum.y-bounds.minimum.y)-8.0)<1e-6&&std::abs((bounds.maximum.z-bounds.minimum.z)-14.0)<1e-6,"artifact dimensions");const auto repeated=RoundTechnicCalibrationArtifact::generate(fixture());ok&=require(repeated.ok&&sameMesh(repeated.mesh,artifact.mesh),"deterministic generation");
    const auto parallelDefinition=RoundTechnicCalibrationArtifact::parallelCoarseDefinition();const auto parallelArtifact=RoundTechnicCalibrationArtifact::generate(RoundTechnicCalibrationArtifact::canonicalPrototype(),parallelDefinition);ok&=require(parallelArtifact.ok&&parallelArtifact.artifactIdentity=="round-technic-female-parallel-coarse-v1"&&parallelArtifact.orientationIdentity=="flat-base-horizontal-axis-y-v1","parallel coarse artifact has distinct evidence identity and horizontal modeled passages");ok&=require(parallelArtifact.candidates.size()==7,"parallel coarse artifact has seven candidates");for(int i=0;i<parallelArtifact.candidates.size();++i){ok&=require(std::abs(parallelArtifact.candidates[i].diameterCorrectionMillimetres-(.10*i))<1e-9&&std::abs(parallelArtifact.candidates[i].functionalDiameterMillimetres-(4.8+.10*i))<1e-9,"parallel coarse candidates span 4.80 through 5.40 mm at 0.10 mm spacing");}const auto parallelExperiment=RoundTechnicCalibrationArtifact::observationTemplate(parallelArtifact,parallelDefinition);ok&=require(parallelExperiment.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate&&parallelExperiment.process.orientationNotes.contains("parallel to the build plate")&&parallelExperiment.candidates.front().observations.isEmpty(),"parallel artifact starts as separate unobserved parallel evidence");
    const auto parallelArgs=app.arguments();const int parallelOutputAt=parallelArgs.indexOf("--parallel-output");if(parallelOutputAt>=0&&parallelOutputAt+1<parallelArgs.size()&&parallelArtifact.ok){QDir output(parallelArgs[parallelOutputAt+1]);ok&=require(output.mkpath("."),"parallel artifact output directory");const QString modelPath=output.filePath("BrickSuite-round-technic-female-parallel-coarse-v1.3mf");const QString sessionPath=output.filePath("BrickSuite-round-technic-female-parallel-coarse-v1-session.json");ThreeMfWriter::Options options;options.objectName="BrickSuite Round Technic Female Parallel Coarse v1";options.partIdentity=parallelArtifact.artifactIdentity;options.modelColor=QColor("#0055BF");QString parallelError;ok&=require(ThreeMfWriter::write(parallelArtifact.mesh,modelPath,options,&parallelError),QStringLiteral("parallel 3MF export: %1").arg(parallelError));QSaveFile file(sessionPath);ok&=require(file.open(QIODevice::WriteOnly)&&file.write(QJsonDocument(FitCalibrationExperimentJson::toJson(parallelExperiment)).toJson(QJsonDocument::Indented))>0&&file.commit(),"parallel session template export");QTextStream(stdout)<<"parallelArtifact="<<modelPath<<Qt::endl<<"parallelSession="<<sessionPath<<Qt::endl;}
    auto experiment=RoundTechnicCalibrationArtifact::observationTemplate(artifact);experiment.process.printerIdentity="Test Printer";experiment.process.materialIdentity="Test PLA";experiment.process.profileName="0.20 Standard";experiment.process.hasNozzleDiameter=true;experiment.process.nozzleDiameterMillimetres=.4;experiment.process.hasLayerHeight=true;experiment.process.layerHeightMillimetres=.2;experiment.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;experiment.performedUtc=QDateTime::fromString("2026-09-20T12:00:00.000Z",Qt::ISODateWithMs);FitCalibrationObservation first;first.result=FitObservation::Preferred;first.repeatNumber=1;first.notes="physical mate";first.performedUtc=experiment.performedUtc;QString error;ok&=require(FitCalibrationEvidencePolicy::addObservation(&experiment,3,first,&error),"first physical observation accepted");auto repeat=first;repeat.repeatNumber=2;ok&=require(FitCalibrationEvidencePolicy::addObservation(&experiment,3,repeat,&error)&&experiment.candidates[2].observations.size()==2,"multiple observations retained");ok&=require(!FitCalibrationEvidencePolicy::addObservation(&experiment,3,repeat,&error),"ambiguous duplicate repeat rejected");ok&=require(FitCalibrationEvidencePolicy::selectPreferredCandidate(&experiment,3,&error)&&experiment.state==FitEvidenceState::CandidateSelected,"provisional preferred candidate selected");ok&=require(!FitCalibrationEvidencePolicy::markVerified(&experiment,&error),"first-pass artifact cannot become verified");const auto json=FitCalibrationExperimentJson::toJson(experiment);FitCalibrationExperiment decoded;ok&=require(FitCalibrationExperimentJson::fromJson(json,&decoded,&error)&&decoded.artifactIdentity==experiment.artifactIdentity&&decoded.candidates.size()==7&&decoded.candidates[2].observations.size()==2&&decoded.preferredCandidateIndex==3&&decoded.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate&&decoded.process.profileName=="0.20 Standard"&&decoded.hasRegenerationPrototype,"versioned observation JSON round trip");QJsonObject invalid;ok&=require(!FitCalibrationExperimentJson::fromJson(invalid,&decoded,&error),"invalid observation document rejected");

    RoundTechnicCalibrationArtifactDefinition fine;fine.artifactIdentity="round-technic-female-horizontal-verification-v2";fine.parentArtifactIdentity=artifact.artifactIdentity;fine.centerDiameterCorrectionMillimetres=.10;fine.candidateSpacingMillimetres=.025;fine.candidateCount=5;const auto fineArtifact=RoundTechnicCalibrationArtifact::generate(decoded.regenerationPrototype,fine);ok&=require(fineArtifact.ok&&fineArtifact.candidates.size()==5,"fine-search artifact generated");for(int i=0;i<5;++i)ok&=require(std::abs(fineArtifact.candidates[i].diameterCorrectionMillimetres-(.05+.025*i))<1e-9,"arbitrary fine-search center and spacing");ok&=require(fineArtifact.analysis.connectedComponents==1&&fineArtifact.analysis.boundaryEdges==0&&fineArtifact.analysis.nonManifoldEdges==0&&fineArtifact.analysis.selfIntersections==0,"fine-search topology");ok&=require(std::abs((fineArtifact.analysis.bounds.maximum.x-fineArtifact.analysis.bounds.minimum.x)-68.0)<1e-6,"fine-search remains 100 percent physical size");const auto fineRepeated=RoundTechnicCalibrationArtifact::generate(decoded.regenerationPrototype,fine);ok&=require(fineRepeated.ok&&sameMesh(fineRepeated.mesh,fineArtifact.mesh),"fine-search generation deterministic");auto verification=RoundTechnicCalibrationArtifact::observationTemplate(fineArtifact,fine);verification.process=experiment.process;for(int candidateIndex:{2,3,4}){FitCalibrationObservation observation=first;observation.repeatNumber=1;observation.result=candidateIndex==3?FitObservation::Preferred:FitObservation::Acceptable;ok&=require(FitCalibrationEvidencePolicy::addObservation(&verification,candidateIndex,observation,&error),"verification comparison observation");if(candidateIndex==3){observation.repeatNumber=2;ok&=require(FitCalibrationEvidencePolicy::addObservation(&verification,candidateIndex,observation,&error),"verification repeat observation");}}ok&=require(FitCalibrationEvidencePolicy::selectPreferredCandidate(&verification,3,&error)&&FitCalibrationEvidencePolicy::markVerified(&verification,&error)&&verification.state==FitEvidenceState::Verified,"repeat plus adjacent evidence becomes verified");FitCalibrationEvidencePolicy::markStaleIfArtifactChanged(&verification,"round-technic-female-horizontal-verification-v3");ok&=require(verification.state==FitEvidenceState::Stale,"artifact version mismatch marks evidence stale");RoundTechnicCalibrationArtifactDefinition invalidFine=fine;invalidFine.candidateCount=4;ok&=require(!RoundTechnicCalibrationArtifact::generate(decoded.regenerationPrototype,invalidFine).ok,"ambiguous even candidate range rejected");
    const auto args=app.arguments();const int libraryAt=args.indexOf("--ldraw"),outputAt=args.indexOf("--output");if(libraryAt>=0&&libraryAt+1<args.size()){const auto loaded=LDrawLibraryService::loadPart(args[libraryAt+1],"3700");ok&=require(loaded.ok(),"real 3700 contract load");const auto semantic=LDrawSemanticOperandBuilder::build(loaded);const FunctionalFeature*feature=nullptr;for(const auto&operand:semantic.operands)if(!operand.functionalFeatures.isEmpty()){feature=&operand.functionalFeatures.front();break;}ok&=require(feature,"real retained functional contract");if(feature){const auto real=RoundTechnicCalibrationArtifact::generate(*feature);ok&=require(real.ok,"real-contract artifact generation");ok&=require(real.analysis.connectedComponents==1&&real.analysis.boundaryEdges==0&&real.analysis.nonManifoldEdges==0&&real.analysis.nonManifoldVertices==0&&real.analysis.selfIntersections==0,"real retained contract produces validated artifact topology");for(int i=0;i<real.candidates.size();++i)ok&=require(std::abs(real.candidates[i].functionalDiameterMillimetres-(feature->nominalDiameterMillimetres+expected[i]))<1e-9,"real contract drives candidate dimensions");if(outputAt>=0&&outputAt+1<args.size()){QDir output(args[outputAt+1]);ok&=require(output.mkpath("."),"artifact output directory");const auto modelPath=output.filePath("BrickSuite-round-technic-female-horizontal-v1.3mf");ThreeMfWriter::Options options;options.uniformScale=1.0;options.objectName="BrickSuite Round Technic Female Calibration v1";options.partIdentity=real.artifactIdentity;options.modelColor=QColor("#0055BF");ok&=require(ThreeMfWriter::write(real.mesh,modelPath,options,&error),QStringLiteral("3MF export: %1").arg(error));auto sheet=RoundTechnicCalibrationArtifact::observationTemplate(real);QSaveFile file(output.filePath("BrickSuite-round-technic-female-horizontal-v1-observations.json"));ok&=require(file.open(QIODevice::WriteOnly)&&file.write(QJsonDocument(FitCalibrationExperimentJson::toJson(sheet)).toJson(QJsonDocument::Indented))>0&&file.commit(),"observation template export");QTextStream(stdout)<<"artifact="<<modelPath<<Qt::endl<<"observations="<<output.filePath("BrickSuite-round-technic-female-horizontal-v1-observations.json")<<Qt::endl;}}}
    auto child=RoundTechnicCalibrationArtifact::observationTemplate(fineArtifact,fine);child.process=experiment.process;ok&=require(child.parentArtifactIdentity==artifact.artifactIdentity&&std::abs(child.centerDiameterCorrectionMillimetres-.10)<1e-12,"fine-search child retains parent identity and selected correction");ok&=require(child.process.printerIdentity==experiment.process.printerIdentity&&child.process.materialIdentity==experiment.process.materialIdentity&&child.process.nozzleDiameterMillimetres==experiment.process.nozzleDiameterMillimetres&&child.process.profileName==experiment.process.profileName&&child.process.layerHeightMillimetres==experiment.process.layerHeightMillimetres&&child.process.actualPrintedOrientation==experiment.process.actualPrintedOrientation&&child.process.orientationNotes==experiment.process.orientationNotes&&child.process.dimensionalCompensationNotes==experiment.process.dimensionalCompensationNotes,"fine-search child inherits manufacturing context");bool childHasObservations=false;for(const auto&candidate:child.candidates)childHasObservations|=!candidate.observations.isEmpty();ok&=require(!childHasObservations&&child.preferredCandidateIndex==0&&child.state==FitEvidenceState::Draft,"fine-search child starts with new unevaluated evidence");
    FitCalibrationSession session;session.sessionIdentity="round-technic-session-1";session.process=experiment.process;session.hasCoarseExperiment=true;session.coarseExperiment=experiment;session.hasFineExperiment=true;session.fineExperiment=child;const auto sessionJson=FitCalibrationSessionJson::toJson(session);FitCalibrationSession decodedSession;ok&=require(sessionJson.value("formatVersion").toInt()==3&&FitCalibrationSessionJson::fromJson(sessionJson,&decodedSession,&error),"unified calibration session round trip");ok&=require(decodedSession.hasCoarseExperiment&&decodedSession.hasFineExperiment&&decodedSession.coarseExperiment.candidates[2].observations.size()==2&&decodedSession.fineExperiment.candidates[0].observations.isEmpty(),"coarse and fine evidence remain independent");ok&=require(decodedSession.coarseExperiment.preferredCandidateIndex==3&&decodedSession.fineExperiment.preferredCandidateIndex==0&&decodedSession.process.printerIdentity==experiment.process.printerIdentity&&decodedSession.fineExperiment.parentArtifactIdentity==artifact.artifactIdentity,"session preserves independent selections, shared process, and lineage");FitCalibrationSession coarseOnly;ok&=require(FitCalibrationSessionJson::fromJson(json,&coarseOnly,&error)&&coarseOnly.hasCoarseExperiment&&!coarseOnly.hasFineExperiment&&coarseOnly.coarseExperiment.candidates[2].observations.size()==2,"legacy v2 coarse experiment upgrades without observation loss");const auto legacyChildJson=FitCalibrationExperimentJson::toJson(child);FitCalibrationSession fineOnly;ok&=require(FitCalibrationSessionJson::fromJson(legacyChildJson,&fineOnly,&error)&&!fineOnly.hasCoarseExperiment&&fineOnly.hasFineExperiment&&fineOnly.fineExperiment.parentArtifactIdentity==artifact.artifactIdentity,"legacy v2 fine child remains loadable");
    FitCalibrationExperiment emptyExperiment;ok&=require(FitCalibrationEvidencePolicy::guidanceText(emptyExperiment).contains("Load"),"guidance prompts when no experiment is loaded");ok&=require(FitCalibrationEvidencePolicy::guidanceText(artifact.ok?RoundTechnicCalibrationArtifact::observationTemplate(artifact):FitCalibrationExperiment()).contains("coarse"),"guidance prompts for coarse observations");ok&=require(FitCalibrationEvidencePolicy::guidanceText(experiment).contains("above"),"guidance identifies missing neighbor evidence for a bracketed coarse Preferred");ok&=require(FitCalibrationEvidencePolicy::guidanceText(child).contains("verification artifact"),"guidance prompts for new child evidence");ok&=require(FitCalibrationEvidencePolicy::guidanceText(verification).contains("stale"),"guidance identifies stale evidence");
    QJsonObject legacyV1{{"formatVersion",1},{"artifactIdentity","legacy-coarse-v1"},{"featureFamily","RoundTechnicPassage"},{"featureRole","female"},{"orientationIdentity","legacy-modeled-orientation"},{"printerIdentity","Legacy Printer"},{"materialIdentity","Legacy PETG"},{"candidates",QJsonArray{QJsonObject{{"index",1},{"diameterCorrectionMillimetres",0.1},{"functionalDiameterMillimetres",4.9},{"observation","acceptable"},{"repeatNumber",1}}}}};FitCalibrationSession legacyV1Session;ok&=require(FitCalibrationSessionJson::fromJson(legacyV1,&legacyV1Session,&error)&&legacyV1Session.hasCoarseExperiment&&legacyV1Session.coarseExperiment.candidates.front().observations.size()==1&&legacyV1Session.process.printerIdentity=="Legacy Printer","legacy v1 experiment upgrades without observation loss");
    auto readyGuidance=verification;readyGuidance.state=FitEvidenceState::CandidateSelected;ok&=require(FitCalibrationEvidencePolicy::guidanceText(readyGuidance).contains("Mark Verified is available"),"complete evidence guidance requests explicit verification and leaves more calibration optional");ok&=require(FitCalibrationEvidencePolicy::markVerified(&readyGuidance,&error)&&FitCalibrationEvidencePolicy::guidanceText(readyGuidance).contains("Click Save"),"verified guidance requests persistence");
    const auto studPrototype=StandardStudCalibrationArtifact::canonicalPrototype();StandardStudCalibrationArtifactDefinition studOd;studOd.artifactIdentity=StandardStudCalibrationArtifact::diameterArtifactIdentity();const auto studOdArtifact=StandardStudCalibrationArtifact::generate(studPrototype,studOd);ok&=require(studOdArtifact.ok&&studOdArtifact.candidates.size()==7,"standard stud OD artifact generation");if(studOdArtifact.ok){for(int i=0;i<7;++i){const double expectedCorrection=-.3+.1*i;ok&=require(std::abs(studOdArtifact.candidates[i].diameterCorrectionMillimetres-expectedCorrection)<1e-9&&std::abs(studOdArtifact.candidates[i].functionalDiameterMillimetres-(4.8+expectedCorrection))<1e-9&&std::abs(studOdArtifact.candidates[i].functionalHeightMillimetres-1.6)<1e-9,"stud OD candidates vary diameter independently");}const auto repeatedStud=StandardStudCalibrationArtifact::generate(studPrototype,studOd);ok&=require(repeatedStud.ok&&sameMesh(repeatedStud.mesh,studOdArtifact.mesh),"standard stud OD artifact deterministic");}
    if(studOdArtifact.ok){auto boundaryExperiment=StandardStudCalibrationArtifact::observationTemplate(studOdArtifact,studOd);boundaryExperiment.preferredCandidateIndex=7;auto plan=FitCalibrationEvidencePolicy::nextSearchPlan(boundaryExperiment);ok&=require(plan.boundary==FitPreferredBoundary::Upper&&std::abs(plan.centerCorrectionMillimetres-.45)<1e-9&&std::abs(plan.candidateSpacingMillimetres-.05)<1e-9&&plan.candidateCount==7,"upper-bound preferred candidate extends upward with overlapping half-step search");ok&=require(FitCalibrationEvidencePolicy::directVerificationPlan(boundaryExperiment).candidateCount==0,"boundary candidate cannot bypass extension through direct verification");ok&=require(FitCalibrationEvidencePolicy::guidanceText(boundaryExperiment).contains("upper boundary"),"upper-bound guidance requests upward extension");boundaryExperiment.preferredCandidateIndex=1;plan=FitCalibrationEvidencePolicy::nextSearchPlan(boundaryExperiment);ok&=require(plan.boundary==FitPreferredBoundary::Lower&&std::abs(plan.centerCorrectionMillimetres+.45)<1e-9&&std::abs(plan.candidateSpacingMillimetres-.05)<1e-9,"lower-bound preferred candidate extends downward");ok&=require(FitCalibrationEvidencePolicy::guidanceText(boundaryExperiment).contains("lower boundary"),"lower-bound guidance requests downward extension");boundaryExperiment.preferredCandidateIndex=4;plan=FitCalibrationEvidencePolicy::nextSearchPlan(boundaryExperiment);const auto direct=FitCalibrationEvidencePolicy::directVerificationPlan(boundaryExperiment);ok&=require(plan.boundary==FitPreferredBoundary::None&&std::abs(plan.centerCorrectionMillimetres)<1e-9&&std::abs(plan.candidateSpacingMillimetres-.025)<1e-9&&plan.candidateCount==5,"interior preferred candidate retains optional fine-search policy");ok&=require(direct.boundary==FitPreferredBoundary::None&&std::abs(direct.centerCorrectionMillimetres)<1e-9&&std::abs(direct.candidateSpacingMillimetres-.1)<1e-9&&direct.candidateCount==3,"interior preferred candidate also allows direct three-candidate verification");ok&=require(FitCalibrationEvidencePolicy::continuationAvailable(boundaryExperiment),"interior Preferred retains optional continuation");auto roundBoundary=RoundTechnicCalibrationArtifact::observationTemplate(artifact);roundBoundary.preferredCandidateIndex=roundBoundary.candidates.back().index;ok&=require(FitCalibrationEvidencePolicy::nextSearchPlan(roundBoundary).boundary==FitPreferredBoundary::Upper,"RoundTechnicPassage shares generic boundary handling");roundBoundary.preferredCandidateIndex=3;ok&=require(FitCalibrationEvidencePolicy::directVerificationPlan(roundBoundary).candidateCount==3,"RoundTechnicPassage also supports optional direct verification");}
    StandardStudCalibrationArtifactDefinition studHeight;studHeight.artifactIdentity=StandardStudCalibrationArtifact::heightArtifactIdentity();studHeight.dimension=StandardStudCalibrationDimension::Height;const auto studHeightArtifact=StandardStudCalibrationArtifact::generate(studPrototype,studHeight);ok&=require(studHeightArtifact.ok&&studHeightArtifact.candidates.size()==7,"standard stud height artifact generation");if(studHeightArtifact.ok){for(int i=0;i<7;++i){const double expectedCorrection=-.3+.1*i;ok&=require(std::abs(studHeightArtifact.candidates[i].heightCorrectionMillimetres-expectedCorrection)<1e-9&&std::abs(studHeightArtifact.candidates[i].functionalHeightMillimetres-(1.6+expectedCorrection))<1e-9&&std::abs(studHeightArtifact.candidates[i].functionalDiameterMillimetres-4.8)<1e-9,"stud height candidates vary height independently at nominal OD");}auto fixedOdDefinition=studHeight;fixedOdDefinition.fixedDiameterCorrectionMillimetres=.1;const auto fixedOdArtifact=StandardStudCalibrationArtifact::generate(studPrototype,fixedOdDefinition);ok&=require(fixedOdArtifact.ok&&std::abs(fixedOdArtifact.candidates.front().functionalDiameterMillimetres-4.9)<1e-9,"height calibration can hold a separately established OD correction fixed");auto studExperiment=StandardStudCalibrationArtifact::observationTemplate(fixedOdArtifact,fixedOdDefinition);const auto studJson=FitCalibrationExperimentJson::toJson(studExperiment);FitCalibrationExperiment decodedStud;ok&=require(FitCalibrationExperimentJson::fromJson(studJson,&decodedStud,&error)&&decodedStud.correctionDimension==FitCorrectionDimension::Height&&decodedStud.regenerationPrototype.family==FunctionalInterfaceFamily::StandardStud&&std::abs(decodedStud.fixedDiameterCorrectionMillimetres-.1)<1e-9,"managed session JSON preserves standard stud dimension, prototype, and fixed OD context");}
    StandardStudCalibrationArtifactDefinition studOdExtension;studOdExtension.artifactIdentity=QStringLiteral("standard-stud-male-od-upper-extension-v2");studOdExtension.parentArtifactIdentity=studOd.artifactIdentity;studOdExtension.centerCorrectionMillimetres=.45;studOdExtension.candidateSpacingMillimetres=.05;const auto studOdExtensionArtifact=StandardStudCalibrationArtifact::generate(studPrototype,studOdExtension);ok&=require(studOdExtensionArtifact.ok&&std::abs(studOdExtensionArtifact.candidates.front().functionalDiameterMillimetres-5.10)<1e-9&&std::abs(studOdExtensionArtifact.candidates.back().functionalDiameterMillimetres-5.40)<1e-9,"physical upper-bound continuation covers 5.10 through 5.40 mm");
    StandardStudCalibrationArtifactDefinition studOdVerification;studOdVerification.artifactIdentity=QStringLiteral("standard-stud-male-od-direct-verification-v2");studOdVerification.parentArtifactIdentity=studOdExtension.artifactIdentity;studOdVerification.centerCorrectionMillimetres=.35;studOdVerification.candidateSpacingMillimetres=.05;studOdVerification.candidateCount=3;const auto studOdVerificationArtifact=StandardStudCalibrationArtifact::generate(studPrototype,studOdVerification);ok&=require(studOdVerificationArtifact.ok&&std::abs(studOdVerificationArtifact.candidates[0].functionalDiameterMillimetres-5.10)<1e-9&&std::abs(studOdVerificationArtifact.candidates[1].functionalDiameterMillimetres-5.15)<1e-9&&std::abs(studOdVerificationArtifact.candidates[2].functionalDiameterMillimetres-5.20)<1e-9,"direct verification repeats physical preferred OD and acceptable neighbors");if(studOdVerificationArtifact.ok){auto verificationExperiment=StandardStudCalibrationArtifact::observationTemplate(studOdVerificationArtifact,studOdVerification);verificationExperiment.preferredCandidateIndex=2;QString verificationError;ok&=require(!FitCalibrationEvidencePolicy::markVerified(&verificationExperiment,&verificationError)&&verificationError.contains("process identity"),"Mark Verified explains the missing manufacturing context before evidence checks");ok&=require(!FitCalibrationEvidencePolicy::continuationAvailable(verificationExperiment),"direct verification does not invite another continuation child");auto extensionExperiment=StandardStudCalibrationArtifact::observationTemplate(studOdExtensionArtifact,studOdExtension);extensionExperiment.preferredCandidateIndex=2;ok&=require(FitCalibrationEvidencePolicy::continuationAvailable(extensionExperiment),"boundary-extension result may continue to fine search or direct verification");const auto verificationJson=FitCalibrationExperimentJson::toJson(verificationExperiment);FitCalibrationExperiment decodedVerification;ok&=require(FitCalibrationExperimentJson::fromJson(verificationJson,&decodedVerification,&error)&&decodedVerification.parentArtifactIdentity==studOdExtension.artifactIdentity&&std::abs(decodedVerification.centerDiameterCorrectionMillimetres-.35)<1e-9,"direct Standard Stud verification evidence round-trips with lineage");}
    if(outputAt>=0&&outputAt+1<args.size()&&studOdArtifact.ok&&studHeightArtifact.ok&&studOdExtensionArtifact.ok&&studOdVerificationArtifact.ok){QDir output(args[outputAt+1]);ok&=require(output.mkpath("."),"stud artifact output directory");auto writeStud=[&](const StandardStudCalibrationArtifactResult&a,const StandardStudCalibrationArtifactDefinition&d,const QString&name){ThreeMfWriter::Options options;options.objectName=name;options.partIdentity=a.artifactIdentity;options.modelColor=QColor("#0055BF");const QString model=output.filePath(name+".3mf");ok&=require(ThreeMfWriter::write(a.mesh,model,options,&error),QStringLiteral("stud 3MF export: %1").arg(error));QSaveFile file(output.filePath(name+"-session.json"));ok&=require(file.open(QIODevice::WriteOnly)&&file.write(QJsonDocument(FitCalibrationExperimentJson::toJson(StandardStudCalibrationArtifact::observationTemplate(a,d))).toJson(QJsonDocument::Indented))>0&&file.commit(),"stud session template export");};writeStud(studOdArtifact,studOd,"BrickSuite-standard-stud-od-perpendicular-v1");writeStud(studHeightArtifact,studHeight,"BrickSuite-standard-stud-height-perpendicular-v1");writeStud(studOdExtensionArtifact,studOdExtension,"BrickSuite-standard-stud-od-upper-extension-v2");writeStud(studOdVerificationArtifact,studOdVerification,"BrickSuite-standard-stud-od-direct-verification-v2");}
    if(studOdVerificationArtifact.ok){auto state=StandardStudCalibrationArtifact::observationTemplate(studOdVerificationArtifact,studOdVerification);state.process.printerIdentity="Bambu H2D";state.process.materialIdentity="PETG";state.process.profileName="0.20 mm Standard";state.process.hasNozzleDiameter=true;state.process.nozzleDiameterMillimetres=.4;state.process.hasLayerHeight=true;state.process.layerHeightMillimetres=.2;FitCalibrationObservation observation;observation.performedUtc=QDateTime::currentDateTimeUtc();observation.repeatNumber=1;observation.result=FitObservation::Preferred;ok&=require(FitCalibrationEvidencePolicy::addObservation(&state,2,observation,&error)&&state.preferredCandidateIndex==0,"Preferred observation does not implicitly select the experiment Preferred candidate");observation.result=FitObservation::Acceptable;ok&=require(FitCalibrationEvidencePolicy::addObservation(&state,1,observation,&error)&&state.candidates[1].observations.size()==1,"neighbor observation does not erase center evidence");observation.repeatNumber=2;observation.result=FitObservation::Preferred;ok&=require(FitCalibrationEvidencePolicy::addObservation(&state,2,observation,&error),"second center observation accumulates");observation.repeatNumber=1;observation.result=FitObservation::Acceptable;ok&=require(FitCalibrationEvidencePolicy::addObservation(&state,3,observation,&error),"upper neighbor observation accumulates in arbitrary order");ok&=require(FitCalibrationEvidencePolicy::observationCount(state.candidates[0])==1&&FitCalibrationEvidencePolicy::observationCount(state.candidates[1])==2&&FitCalibrationEvidencePolicy::observationCount(state.candidates[2])==1,"Stud OD projection exposes independent 1/2/1 observation counts");ok&=require(FitCalibrationEvidencePolicy::selectPreferredCandidate(&state,2,&error)&&state.candidates[0].observations.size()==1&&state.candidates[1].observations.size()==2&&state.candidates[2].observations.size()==1,"Preferred selection preserves all candidate observations");ok&=require(FitCalibrationEvidencePolicy::observationProgressText(state,state.candidates[0])=="Neighbor evidence recorded"&&FitCalibrationEvidencePolicy::observationProgressText(state,state.candidates[1])=="Preferred repeat 2/2"&&FitCalibrationEvidencePolicy::observationProgressText(state,state.candidates[2])=="Neighbor evidence recorded","table projection reports raw counts separately from verification progress");QString eligibilityReason;ok&=require(!FitCalibrationEvidencePolicy::canMarkVerified(state,&eligibilityReason)&&eligibilityReason.contains("actual feature orientation"),"Mark Verified eligibility identifies missing feature orientation");state.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;ok&=require(FitCalibrationEvidencePolicy::canMarkVerified(state,&eligibilityReason)&&eligibilityReason.isEmpty(),"orientation/context update immediately makes complete Stud OD evidence eligible");ok&=require(FitCalibrationEvidencePolicy::markVerified(&state,&error),"arbitrary-order Stud OD evidence satisfies unchanged verification policy: "+error);const auto stateJson=FitCalibrationExperimentJson::toJson(state);FitCalibrationExperiment restoredState;ok&=require(FitCalibrationExperimentJson::fromJson(stateJson,&restoredState,&error)&&restoredState.state==FitEvidenceState::Verified&&restoredState.candidates[0].observations.size()==1&&restoredState.candidates[1].observations.size()==2&&restoredState.candidates[2].observations.size()==1,"Verified Stud OD state and observation counts survive persistence round trip");}
    if(studHeightArtifact.ok){auto provisional=StandardStudCalibrationArtifact::observationTemplate(studHeightArtifact,studHeight);provisional.preferredCandidateIndex=6;StandardStudCalibrationArtifactDefinition focusedHeight;QString planningError;ok&=require(StandardStudCalibrationArtifact::heightVerificationDefinition(provisional,.35,&focusedHeight,&planningError),"Verified-OD Stud Height planning: "+planningError);ok&=require(focusedHeight.dimension==StandardStudCalibrationDimension::Height&&focusedHeight.parentArtifactIdentity==provisional.artifactIdentity&&focusedHeight.candidateCount==3&&std::abs(focusedHeight.centerCorrectionMillimetres-.2)<1e-9&&std::abs(focusedHeight.candidateSpacingMillimetres-.05)<1e-9&&std::abs(focusedHeight.fixedDiameterCorrectionMillimetres-.35)<1e-9,"provisional 1.80 mm result becomes a focused three-candidate verification plan with +0.35 mm OD fixed");const auto focusedArtifact=StandardStudCalibrationArtifact::generate(studPrototype,focusedHeight);ok&=require(focusedArtifact.ok&&focusedArtifact.candidates.size()==3,"focused Stud Height verification artifact generation");if(focusedArtifact.ok){ok&=require(std::abs(focusedArtifact.candidates[0].functionalHeightMillimetres-1.75)<1e-9&&std::abs(focusedArtifact.candidates[1].functionalHeightMillimetres-1.80)<1e-9&&std::abs(focusedArtifact.candidates[2].functionalHeightMillimetres-1.85)<1e-9,"focused Stud Height candidates bracket the provisional 1.80 mm result");for(const auto&candidate:focusedArtifact.candidates)ok&=require(std::abs(candidate.functionalDiameterMillimetres-5.15)<1e-9&&std::abs(candidate.diameterCorrectionMillimetres-.35)<1e-9,"every Stud Height candidate retains the Verified 5.15 mm OD");const auto focusedExperiment=StandardStudCalibrationArtifact::observationTemplate(focusedArtifact,focusedHeight);const auto focusedJson=FitCalibrationExperimentJson::toJson(focusedExperiment);FitCalibrationExperiment restoredFocused;ok&=require(FitCalibrationExperimentJson::fromJson(focusedJson,&restoredFocused,&planningError)&&restoredFocused.correctionDimension==FitCorrectionDimension::Height&&std::abs(restoredFocused.fixedDiameterCorrectionMillimetres-.35)<1e-9&&restoredFocused.parentArtifactIdentity==provisional.artifactIdentity,"focused height lineage and independent fixed OD context survive session persistence");}auto wrongDimension=provisional;wrongDimension.correctionDimension=FitCorrectionDimension::Diameter;ok&=require(!StandardStudCalibrationArtifact::heightVerificationDefinition(wrongDimension,.35,&focusedHeight,&planningError),"Stud OD evidence cannot be mistaken for provisional Stud Height evidence");}
    if(outputAt>=0&&outputAt+1<args.size()&&studHeightArtifact.ok){auto provisional=StandardStudCalibrationArtifact::observationTemplate(studHeightArtifact,studHeight);provisional.preferredCandidateIndex=6;StandardStudCalibrationArtifactDefinition definition;QString artifactError;if(StandardStudCalibrationArtifact::heightVerificationDefinition(provisional,.35,&definition,&artifactError)){const auto result=StandardStudCalibrationArtifact::generate(studPrototype,definition);QDir output(args[outputAt+1]);ThreeMfWriter::Options options;options.objectName=definition.artifactIdentity;options.partIdentity=definition.artifactIdentity;options.modelColor=QColor("#0055BF");const QString model=output.filePath(QStringLiteral("BrickSuite-standard-stud-height-direct-verification-v2.3mf"));ok&=require(result.ok&&ThreeMfWriter::write(result.mesh,model,options,&artifactError),QStringLiteral("focused Stud Height 3MF export: %1").arg(artifactError));QSaveFile file(output.filePath(QStringLiteral("BrickSuite-standard-stud-height-direct-verification-v2-session.json")));ok&=require(file.open(QIODevice::WriteOnly)&&file.write(QJsonDocument(FitCalibrationExperimentJson::toJson(StandardStudCalibrationArtifact::observationTemplate(result,definition))).toJson(QJsonDocument::Indented))>0&&file.commit(),"focused Stud Height session template export");QTextStream(stdout)<<"studHeightVerification="<<model<<Qt::endl;}}
    auto directParallel=parallelExperiment;directParallel.process=experiment.process;FitCalibrationObservation neighbor=first;neighbor.result=FitObservation::Acceptable;neighbor.repeatNumber=1;ok&=require(FitCalibrationEvidencePolicy::addObservation(&directParallel,2,neighbor,&error)&&FitCalibrationEvidencePolicy::addObservation(&directParallel,4,neighbor,&error),"parallel coarse evidence records both physical neighbors");ok&=require(FitCalibrationEvidencePolicy::addObservation(&directParallel,3,first,&error)&&FitCalibrationEvidencePolicy::addObservation(&directParallel,3,repeat,&error)&&FitCalibrationEvidencePolicy::selectPreferredCandidate(&directParallel,3,&error),"parallel coarse evidence records a repeated interior Preferred");QString directReason;ok&=require(FitCalibrationEvidencePolicy::canMarkVerified(directParallel,&directReason)&&directReason.isEmpty(),"sufficient parallel coarse evidence is directly eligible without a child artifact");ok&=require(FitCalibrationEvidencePolicy::continuationAvailable(directParallel),"Fine Search remains optional for directly eligible coarse evidence");ok&=require(FitCalibrationEvidencePolicy::guidanceText(directParallel).contains("Fine Search")&&FitCalibrationEvidencePolicy::guidanceText(directParallel).contains("optional"),"direct eligibility guidance presents additional calibration as optional");auto missingRepeat=directParallel;missingRepeat.candidates[2].observations.removeLast();ok&=require(!FitCalibrationEvidencePolicy::canMarkVerified(missingRepeat,&directReason)&&directReason.contains("requires 1 more"),"missing repeat still blocks direct verification");auto missingNeighbor=directParallel;missingNeighbor.candidates[3].observations.clear();ok&=require(!FitCalibrationEvidencePolicy::canMarkVerified(missingNeighbor,&directReason)&&directReason.contains("above"),"missing upper neighbor still blocks direct verification");auto missingContext=directParallel;missingContext.process.profileName.clear();ok&=require(!FitCalibrationEvidencePolicy::canMarkVerified(missingContext,&directReason)&&directReason.contains("manufacturing context"),"missing manufacturing context still blocks direct verification");auto missingOrientation=directParallel;missingOrientation.process.actualPrintedOrientation=FitPrintedOrientation::Unknown;ok&=require(!FitCalibrationEvidencePolicy::canMarkVerified(missingOrientation,&directReason)&&directReason.contains("actual feature orientation"),"missing orientation still blocks direct verification");auto boundaryDirect=directParallel;boundaryDirect.preferredCandidateIndex=1;ok&=require(!FitCalibrationEvidencePolicy::canMarkVerified(boundaryDirect,&directReason)&&directReason.contains("search boundary"),"boundary Preferred still requires range extension");ok&=require(FitCalibrationEvidencePolicy::markVerified(&directParallel,&error)&&directParallel.state==FitEvidenceState::Verified,"explicit Mark Verified accepts sufficient evidence on the current coarse artifact");const auto directJson=FitCalibrationExperimentJson::toJson(directParallel);FitCalibrationExperiment restoredDirect;ok&=require(FitCalibrationExperimentJson::fromJson(directJson,&restoredDirect,&error)&&restoredDirect.state==FitEvidenceState::Verified&&restoredDirect.parentArtifactIdentity.isEmpty(),"directly Verified coarse evidence remains compatible after persistence");
    if(studOdArtifact.ok){auto directStud=StandardStudCalibrationArtifact::observationTemplate(studOdArtifact,studOd);directStud.process=experiment.process;ok&=require(FitCalibrationEvidencePolicy::addObservation(&directStud,3,neighbor,&error)&&FitCalibrationEvidencePolicy::addObservation(&directStud,5,neighbor,&error)&&FitCalibrationEvidencePolicy::addObservation(&directStud,4,first,&error)&&FitCalibrationEvidencePolicy::addObservation(&directStud,4,repeat,&error)&&FitCalibrationEvidencePolicy::selectPreferredCandidate(&directStud,4,&error)&&FitCalibrationEvidencePolicy::canMarkVerified(directStud,&directReason),"generic direct-verification policy also accepts sufficient StandardStud coarse evidence");}
    const auto receiving=StudReceivingCalibrationArtifact::generate();ok&=require(receiving.ok&&receiving.candidates.size()==7,"TubeWallCell seven-candidate artifact generation: "+receiving.diagnostic);if(receiving.ok){for(int i=0;i<7;++i)ok&=require(std::abs(receiving.candidates[i].functionalDiameterMillimetres-(6.1+.1*i))<1e-9,"TubeWallCell candidates span 6.10 through 6.70 mm");ok&=require(receiving.analysis.connectedComponents==1&&receiving.analysis.boundaryEdges==0&&receiving.analysis.nonManifoldEdges==0&&receiving.analysis.selfIntersections==0,"TubeWallCell artifact is a validated connected printable mesh");const auto receivingAgain=StudReceivingCalibrationArtifact::generate();ok&=require(receivingAgain.ok&&sameMesh(receivingAgain.mesh,receiving.mesh),"TubeWallCell artifact generation is deterministic");auto receivingExperiment=StudReceivingCalibrationArtifact::observationTemplate(receiving);const auto receivingJson=FitCalibrationExperimentJson::toJson(receivingExperiment);FitCalibrationExperiment restoredReceiving;ok&=require(FitCalibrationExperimentJson::fromJson(receivingJson,&restoredReceiving,&error)&&restoredReceiving.featureFamily=="StudReceivingClutch"&&restoredReceiving.regenerationPrototype.family==FunctionalInterfaceFamily::StudReceivingClutch&&std::abs(restoredReceiving.regenerationPrototype.protectedInnerRadiusMillimetres-2.4)<1e-9,"TubeWallCell session and protected bore contract round-trip");const int receiverOutputAt=args.indexOf("--receiver-output");if(receiverOutputAt>=0&&receiverOutputAt+1<args.size()){QDir output(args[receiverOutputAt+1]);ok&=require(output.mkpath("."),"TubeWallCell output directory");const QString model=output.filePath("BrickSuite-stud-receiving-clutch-tube-wall-cell-perpendicular-v1.3mf");const QString sessionPath=output.filePath("BrickSuite-stud-receiving-clutch-tube-wall-cell-perpendicular-v1-session.json");ThreeMfWriter::Options options;options.objectName=receiving.artifactIdentity;options.partIdentity=receiving.artifactIdentity;options.modelColor=QColor("#0055BF");ok&=require(ThreeMfWriter::write(receiving.mesh,model,options,&error),"TubeWallCell 3MF export: "+error);QSaveFile file(sessionPath);ok&=require(file.open(QIODevice::WriteOnly)&&file.write(QJsonDocument(receivingJson).toJson(QJsonDocument::Indented))>0&&file.commit(),"TubeWallCell session export");QTextStream(stdout)<<"receivingArtifact="<<model<<Qt::endl<<"receivingSession="<<sessionPath<<Qt::endl;}}
    if(receiving.ok){StudReceivingCalibrationArtifactDefinition receivingFine;receivingFine.artifactIdentity="stud-receiving-clutch-tube-wall-cell-fine-search-v2";receivingFine.parentArtifactIdentity=receiving.artifactIdentity;receivingFine.centerDiameterCorrectionMillimetres=.1;receivingFine.candidateSpacingMillimetres=.025;receivingFine.candidateCount=5;const auto fineReceiving=StudReceivingCalibrationArtifact::generate(receiving.regenerationPrototype,receivingFine);ok&=require(fineReceiving.ok&&fineReceiving.candidates.size()==5&&std::abs(fineReceiving.candidates.front().functionalDiameterMillimetres-6.45)<1e-9&&std::abs(fineReceiving.candidates.back().functionalDiameterMillimetres-6.55)<1e-9,"TubeWallCell optional fine search uses retained contract and requested range");const auto fineReceivingExperiment=StudReceivingCalibrationArtifact::observationTemplate(fineReceiving,receivingFine);ok&=require(fineReceivingExperiment.parentArtifactIdentity==receiving.artifactIdentity&&fineReceivingExperiment.candidates.front().observations.isEmpty(),"TubeWallCell continuation retains lineage and starts with new physical evidence");}
    const auto postWall=StudReceivingCalibrationArtifact::generatePostWallCell();ok&=require(postWall.ok&&postWall.candidates.size()==7,"PostWallCell seven-candidate artifact generation: "+postWall.diagnostic);if(postWall.ok){for(int i=0;i<7;++i)ok&=require(std::abs(postWall.candidates[i].functionalDiameterMillimetres-(2.9+.1*i))<1e-9,"PostWallCell post OD candidates span 2.90 through 3.50 mm");ok&=require(postWall.analysis.connectedComponents==1&&postWall.analysis.boundaryEdges==0&&postWall.analysis.nonManifoldEdges==0&&postWall.analysis.selfIntersections==0,"PostWallCell artifact is a validated connected printable mesh");const auto b=postWall.analysis.bounds;ok&=require(std::abs((b.maximum.x-b.minimum.x)-126.0)<1e-6&&std::abs((b.maximum.y-b.minimum.y)-8.0)<1e-6&&std::abs((b.maximum.z-b.minimum.z)-4.8)<1e-6,"PostWallCell artifact dimensions preserve seven fixed wall cells");const auto repeatedPost=StudReceivingCalibrationArtifact::generatePostWallCell();ok&=require(repeatedPost.ok&&sameMesh(repeatedPost.mesh,postWall.mesh),"PostWallCell artifact generation is deterministic");auto postExperiment=StudReceivingCalibrationArtifact::observationTemplate(postWall);const auto postJson=FitCalibrationExperimentJson::toJson(postExperiment);FitCalibrationExperiment restoredPost;ok&=require(FitCalibrationExperimentJson::fromJson(postJson,&restoredPost,&error)&&restoredPost.regenerationPrototype.constructionRecipe=="stud-receiving-post-wall-cell-v1"&&restoredPost.regenerationPrototype.evidenceContract=="official-ldraw-stud3-post-wall-cell-v1"&&restoredPost.process.orientationNotes.contains("both bays")&&restoredPost.process.orientationNotes.contains("Candidate #1"),"PostWallCell session retains semantic contract, orientation, and marker guidance");StudReceivingCalibrationArtifactDefinition fine;fine.artifactIdentity="stud-receiving-clutch-post-wall-cell-verification-v2";fine.parentArtifactIdentity=postWall.artifactIdentity;fine.centerDiameterCorrectionMillimetres=.1;fine.candidateSpacingMillimetres=.025;fine.candidateCount=5;const auto finePost=StudReceivingCalibrationArtifact::generate(postWall.regenerationPrototype,fine);const auto fineExperiment=StudReceivingCalibrationArtifact::observationTemplate(finePost,fine);ok&=require(finePost.ok&&finePost.candidates.size()==5&&fineExperiment.parentArtifactIdentity==postWall.artifactIdentity&&fineExperiment.candidates.front().observations.isEmpty(),"PostWallCell continuation preserves lineage and starts fresh evidence");const int at=args.indexOf("--post-wall-output");if(at>=0&&at+1<args.size()){QDir output(args[at+1]);ok&=require(output.mkpath("."),"PostWallCell output directory");const QString model=output.filePath("BrickSuite-stud-receiving-clutch-post-wall-cell-perpendicular-v1.3mf"),sessionPath=output.filePath("BrickSuite-stud-receiving-clutch-post-wall-cell-perpendicular-v1-session.json");ThreeMfWriter::Options options;options.objectName=postWall.artifactIdentity;options.partIdentity=postWall.artifactIdentity;options.modelColor=QColor("#0055BF");ok&=require(ThreeMfWriter::write(postWall.mesh,model,options,&error),"PostWallCell 3MF export: "+error);FitCalibrationSession session;session.sessionIdentity="phase7a-post-wall-cell";session.process.printerIdentity="Bambu H2D";session.process.materialIdentity="PETG";session.process.profileName="0.20mm Standard @BBL H2D";session.process.hasNozzleDiameter=true;session.process.nozzleDiameterMillimetres=.4;session.process.hasLayerHeight=true;session.process.layerHeightMillimetres=.2;session.process.dimensionalCompensationNotes="None / Bambu Studio defaults";session.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;session.process.orientationNotes=postExperiment.process.orientationNotes;session.hasCoarseExperiment=true;session.coarseExperiment=postExperiment;session.coarseExperiment.process=session.process;QSaveFile file(sessionPath);ok&=require(file.open(QIODevice::WriteOnly)&&file.write(QJsonDocument(FitCalibrationSessionJson::toJson(session)).toJson(QJsonDocument::Indented))>0&&file.commit(),"PostWallCell managed session export");QTextStream(stdout)<<"postWallArtifact="<<model<<Qt::endl<<"postWallSession="<<sessionPath<<Qt::endl<<"postWallDimensions="<<(b.maximum.x-b.minimum.x)<<'x'<<(b.maximum.y-b.minimum.y)<<'x'<<(b.maximum.z-b.minimum.z)<<" mm"<<Qt::endl;}}
    const auto friction=FrictionTechnicPinCalibrationArtifact::generate();ok&=require(friction.ok&&friction.candidates.size()==7,"friction Technic-pin artifact: "+friction.diagnostic);if(friction.ok){for(int i=0;i<7;++i)ok&=require(std::abs(friction.candidates[i].functionalDiameterMillimetres-(4.85+.05*i))<1e-9,"friction ridge candidates span 4.85 through 5.15 mm");ok&=require(friction.analysis.connectedComponents==1&&friction.analysis.boundaryEdges==0&&friction.analysis.nonManifoldEdges==0&&friction.analysis.selfIntersections==0,"friction-pin artifact is deterministic printable geometry");const auto again=FrictionTechnicPinCalibrationArtifact::generate();ok&=require(again.ok&&sameMesh(friction.mesh,again.mesh),"friction-pin artifact generation is deterministic");auto experiment=FrictionTechnicPinCalibrationArtifact::observationTemplate(friction);const auto json=FitCalibrationExperimentJson::toJson(experiment);FitCalibrationExperiment restored;ok&=require(FitCalibrationExperimentJson::fromJson(json,&restored,&error)&&restored.featureFamily=="FrictionTechnicPin"&&restored.regenerationPrototype.family==FunctionalInterfaceFamily::FrictionTechnicPin,"friction-pin workspace/session semantic contract round-trips");ok&=require(restored.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate&&restored.process.orientationNotes.contains("Candidate #1"),"friction-pin fixture records orientation and physical marker guidance");const int frictionAt=args.indexOf("--friction-pin-output");if(frictionAt>=0&&frictionAt+1<args.size()){QDir output(args[frictionAt+1]);ok&=require(output.mkpath("."),"friction-pin output directory");const QString model=output.filePath("BrickSuite-friction-technic-pin-confric5-ridge-perpendicular-coarse-v1.3mf"),sessionPath=output.filePath("BrickSuite-friction-technic-pin-confric5-ridge-perpendicular-coarse-v1-session.json");ThreeMfWriter::Options options;options.objectName=friction.artifactIdentity;options.partIdentity=friction.artifactIdentity;options.modelColor=QColor("#A0A5A9");ok&=require(ThreeMfWriter::write(friction.mesh,model,options,&error),"friction-pin 3MF export: "+error);QSaveFile file(sessionPath);ok&=require(file.open(QIODevice::WriteOnly)&&file.write(QJsonDocument(json).toJson(QJsonDocument::Indented))>0&&file.commit(),"friction-pin session export");QTextStream(stdout)<<"frictionPinArtifact="<<model<<Qt::endl<<"frictionPinSession="<<sessionPath<<Qt::endl;}}
    const auto pin=FrictionlessTechnicPinCalibrationArtifact::generate();ok&=require(pin.ok&&pin.candidates.size()==7,"frictionless Technic-pin artifact: "+pin.diagnostic);if(pin.ok){for(int i=0;i<7;++i)ok&=require(std::abs(pin.candidates[i].functionalDiameterMillimetres-(6.1+.1*i))<1e-9,"pin candidates span 6.10 through 6.70 mm");ok&=require(pin.analysis.connectedComponents==1&&pin.analysis.boundaryEdges==0&&pin.analysis.nonManifoldEdges==0&&pin.analysis.selfIntersections==0,"pin artifact is deterministic printable geometry");const auto pinBounds=pin.analysis.bounds;ok&=require(std::abs((pinBounds.maximum.x-pinBounds.minimum.x)-112.0)<1e-6&&std::abs((pinBounds.maximum.y-pinBounds.minimum.y)-16.0)<1e-6&&std::abs((pinBounds.maximum.z-pinBounds.minimum.z)-10.1)<1e-6,"pin artifact dimensions are 112.0 x 16.0 x 10.1 mm");const auto again=FrictionlessTechnicPinCalibrationArtifact::generate();ok&=require(again.ok&&sameMesh(pin.mesh,again.mesh),"pin artifact generation is deterministic");auto experiment=FrictionlessTechnicPinCalibrationArtifact::observationTemplate(pin);const auto pinJson=FitCalibrationExperimentJson::toJson(experiment);FitCalibrationExperiment restored;ok&=require(FitCalibrationExperimentJson::fromJson(pinJson,&restored,&error)&&restored.featureFamily=="FrictionlessTechnicPin"&&restored.regenerationPrototype.family==FunctionalInterfaceFamily::FrictionlessTechnicPin,"pin calibration session and semantic contract round-trip");const int at=args.indexOf("--pin-output");if(at>=0&&at+1<args.size()){QDir output(args[at+1]);ok&=require(output.mkpath("."),"pin output directory");const QString model=output.filePath("BrickSuite-frictionless-technic-pin-perpendicular-coarse-v1.3mf");const QString sessionPath=output.filePath("BrickSuite-frictionless-technic-pin-perpendicular-coarse-v1-session.json");ThreeMfWriter::Options options;options.objectName=pin.artifactIdentity;options.partIdentity=pin.artifactIdentity;options.modelColor=QColor("#A0A5A9");ok&=require(ThreeMfWriter::write(pin.mesh,model,options,&error),"pin 3MF export: "+error);QSaveFile file(sessionPath);ok&=require(file.open(QIODevice::WriteOnly)&&file.write(QJsonDocument(pinJson).toJson(QJsonDocument::Indented))>0&&file.commit(),"pin session export");QTextStream(stdout)<<"pinArtifact="<<model<<Qt::endl<<"pinSession="<<sessionPath<<Qt::endl;}}
    const auto axle=TechnicAxleCalibrationArtifact::generate();ok&=require(axle.ok&&axle.candidates.size()==7,"ordinary Technic axle seven-candidate artifact: "+axle.diagnostic);if(axle.ok){for(int i=0;i<7;++i)ok&=require(std::abs(axle.candidates[i].functionalDiameterMillimetres-(4.65+.05*i))<1e-9,"axle candidates span 4.65 through 4.95 mm tip-to-tip");ok&=require(axle.analysis.connectedComponents==1&&axle.analysis.boundaryEdges==0&&axle.analysis.nonManifoldEdges==0&&axle.analysis.selfIntersections==0,"axle calibration artifact is a validated connected printable mesh");const auto again=TechnicAxleCalibrationArtifact::generate();ok&=require(again.ok&&sameMesh(axle.mesh,again.mesh),"axle artifact generation is deterministic");const auto axleExperiment=TechnicAxleCalibrationArtifact::observationTemplate(axle);const auto axleJson=FitCalibrationExperimentJson::toJson(axleExperiment);FitCalibrationExperiment restoredAxle;ok&=require(FitCalibrationExperimentJson::fromJson(axleJson,&restoredAxle,&error)&&restoredAxle.featureFamily=="TechnicAxle"&&restoredAxle.regenerationPrototype.family==FunctionalInterfaceFamily::TechnicAxle&&std::abs(restoredAxle.regenerationPrototype.protectedCrossArmHalfWidthMillimetres-.8)<1e-9,"axle workspace/session and protected cross-profile contract round-trip");ok&=require(restoredAxle.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate&&restoredAxle.process.orientationNotes.contains("Candidate #1"),"axle fixture records orientation, variant exclusions, and physical marker guidance");const int axleAt=args.indexOf("--axle-output");if(axleAt>=0&&axleAt+1<args.size()){QDir output(args[axleAt+1]);ok&=require(output.mkpath("."),"axle output directory");const QString model=output.filePath("BrickSuite-technic-axle-tip-envelope-perpendicular-coarse-v1.3mf"),sessionPath=output.filePath("BrickSuite-technic-axle-tip-envelope-perpendicular-coarse-v1-session.json");ThreeMfWriter::Options options;options.objectName=axle.artifactIdentity;options.partIdentity=axle.artifactIdentity;options.modelColor=QColor("#A0A5A9");ok&=require(ThreeMfWriter::write(axle.mesh,model,options,&error),"axle 3MF export: "+error);QSaveFile file(sessionPath);ok&=require(file.open(QIODevice::WriteOnly)&&file.write(QJsonDocument(axleJson).toJson(QJsonDocument::Indented))>0&&file.commit(),"axle session export");QTextStream(stdout)<<"axleArtifact="<<model<<Qt::endl<<"axleSession="<<sessionPath<<Qt::endl;}}
    const auto axleHole=TechnicAxleHoleCalibrationArtifact::generate();ok&=require(axleHole.ok&&axleHole.candidates.size()==7,"ordinary Technic axle-hole seven-candidate artifact: "+axleHole.diagnostic);if(axleHole.ok){for(int i=0;i<7;++i)ok&=require(std::abs(axleHole.candidates[i].functionalDiameterMillimetres-(4.80+.05*i))<1e-9,"axle-hole candidates span 4.80 through 5.10 mm tip-to-tip clearance");ok&=require(axleHole.analysis.connectedComponents==1&&axleHole.analysis.boundaryEdges==0&&axleHole.analysis.nonManifoldEdges==0&&axleHole.analysis.selfIntersections==0,"axle-hole calibration artifact is a validated connected printable mesh");const auto bounds=axleHole.analysis.bounds;ok&=require(std::abs((bounds.maximum.x-bounds.minimum.x)-112.0)<1e-3&&std::abs((bounds.maximum.y-bounds.minimum.y)-20.0)<1e-3&&std::abs((bounds.maximum.z-bounds.minimum.z)-8.0)<1e-6,"axle-hole artifact dimensions are 112 x 20 x 8 mm including marker");const auto again=TechnicAxleHoleCalibrationArtifact::generate();ok&=require(again.ok&&sameMesh(axleHole.mesh,again.mesh),"axle-hole artifact generation is deterministic");const auto experiment=TechnicAxleHoleCalibrationArtifact::observationTemplate(axleHole);const auto json=FitCalibrationExperimentJson::toJson(experiment);FitCalibrationExperiment restored;ok&=require(FitCalibrationExperimentJson::fromJson(json,&restored,&error)&&restored.featureFamily=="TechnicAxleHole"&&restored.featureRole=="female"&&restored.regenerationPrototype.family==FunctionalInterfaceFamily::TechnicAxleHole&&restored.regenerationPrototype.materialSide==FunctionalMaterialSide::EmptyInsideMaterialOutside&&std::abs(restored.regenerationPrototype.protectedCrossArmHalfWidthMillimetres-.8)<1e-9&&std::abs(restored.regenerationPrototype.nominalEngagementExtentMillimetres-8.0)<1e-9,"axle-hole session round-trip preserves female subtractive contract, arm width, relief topology, and engagement depth");ok&=require(restored.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate&&restored.process.orientationNotes.contains("Candidate #1")&&restored.process.orientationNotes.contains("specialized"),"axle-hole fixture records orientation, marker, and variant exclusions");const int at=args.indexOf("--axle-hole-output");if(at>=0&&at+1<args.size()){QDir output(args[at+1]);ok&=require(output.mkpath("."),"axle-hole output directory");const QString model=output.filePath("BrickSuite-technic-axle-hole-tip-clearance-perpendicular-coarse-v1.3mf"),sessionPath=output.filePath("BrickSuite-technic-axle-hole-tip-clearance-perpendicular-coarse-v1-session.json");ThreeMfWriter::Options options;options.objectName=axleHole.artifactIdentity;options.partIdentity=axleHole.artifactIdentity;options.modelColor=QColor("#A0A5A9");ok&=require(ThreeMfWriter::write(axleHole.mesh,model,options,&error),"axle-hole 3MF export: "+error);QSaveFile file(sessionPath);ok&=require(file.open(QIODevice::WriteOnly)&&file.write(QJsonDocument(json).toJson(QJsonDocument::Indented))>0&&file.commit(),"axle-hole session export");QTextStream(stdout)<<"axleHoleArtifact="<<model<<Qt::endl<<"axleHoleSession="<<sessionPath<<Qt::endl;}}
    if(axle.ok){auto allLoose=TechnicAxleCalibrationArtifact::observationTemplate(axle);allLoose.process.printerIdentity="Bambu H2D";allLoose.process.materialIdentity="PETG";allLoose.process.profileName="0.20 mm Standard";allLoose.process.hasNozzleDiameter=true;allLoose.process.nozzleDiameterMillimetres=.4;allLoose.process.hasLayerHeight=true;allLoose.process.layerHeightMillimetres=.2;FitCalibrationObservation directional;directional.result=FitObservation::TooLoose;directional.repeatNumber=1;directional.performedUtc=QDateTime::currentDateTimeUtc();for(const auto&candidate:allLoose.candidates)ok&=require(FitCalibrationEvidencePolicy::addObservation(&allLoose,candidate.index,directional,&error),"axle Too Loose evidence recorded");const auto upward=FitCalibrationEvidencePolicy::nextSearchPlan(allLoose);ok&=require(allLoose.preferredCandidateIndex==0&&upward.uniformResult==FitUniformResult::AllTooLoose&&upward.boundary==FitPreferredBoundary::Upper&&FitCalibrationEvidencePolicy::continuationAvailable(allLoose),"all Too Loose material-inside evidence enables upward continuation without Preferred");ok&=require(std::abs(upward.centerCorrectionMillimetres-.30)<1e-9&&std::abs(upward.candidateSpacingMillimetres-.05)<1e-9&&upward.candidateCount==7,"uniform axle continuation overlaps +0.15 mm and extends through +0.45 mm");ok&=require(FitCalibrationEvidencePolicy::guidanceText(allLoose).contains("All candidates are Too Loose")&&FitCalibrationEvidencePolicy::guidanceText(allLoose).contains("upward"),"uniform evidence guidance states the automatic extension direction");auto allTight=TechnicAxleCalibrationArtifact::observationTemplate(axle);directional.result=FitObservation::TooTight;for(const auto&candidate:allTight.candidates)ok&=require(FitCalibrationEvidencePolicy::addObservation(&allTight,candidate.index,directional,&error),"axle Too Tight evidence recorded");const auto downward=FitCalibrationEvidencePolicy::nextSearchPlan(allTight);ok&=require(downward.uniformResult==FitUniformResult::AllTooTight&&downward.boundary==FitPreferredBoundary::Lower&&std::abs(downward.centerCorrectionMillimetres+.30)<1e-9,"all Too Tight material-inside evidence extends in the opposite direction");auto femaleLoose=allLoose;femaleLoose.featureRole="female";femaleLoose.regenerationPrototype.materialSide=FunctionalMaterialSide::EmptyInsideMaterialOutside;const auto femaleDirection=FitCalibrationEvidencePolicy::nextSearchPlan(femaleLoose);ok&=require(femaleDirection.boundary==FitPreferredBoundary::Lower,"empty-inside female clearance reverses the dimensional direction generically");auto mixed=allLoose;mixed.candidates.back().observations.front().result=FitObservation::Acceptable;const auto mixedPlan=FitCalibrationEvidencePolicy::nextSearchPlan(mixed);ok&=require(mixedPlan.uniformResult==FitUniformResult::None&&!FitCalibrationEvidencePolicy::continuationAvailable(mixed),"mixed or bracketed evidence still requires the established Preferred workflow");TechnicAxleCalibrationArtifactDefinition extension;extension.artifactIdentity="technic-axle-tip-envelope-perpendicular-coarse-extension-v2";extension.parentArtifactIdentity=allLoose.artifactIdentity;extension.centerTipToTipCorrectionMillimetres=upward.centerCorrectionMillimetres;extension.candidateSpacingMillimetres=upward.candidateSpacingMillimetres;extension.candidateCount=upward.candidateCount;const auto extended=TechnicAxleCalibrationArtifact::generate(allLoose.regenerationPrototype,extension);ok&=require(extended.ok&&extended.candidates.size()==7,"uniform axle evidence generates an extended coarse artifact: "+extended.diagnostic);if(extended.ok){for(int i=0;i<7;++i)ok&=require(std::abs(extended.candidates[i].functionalDiameterMillimetres-(4.95+.05*i))<1e-9,"extended axle candidates span 4.95 through 5.25 mm");const auto child=TechnicAxleCalibrationArtifact::observationTemplate(extended,extension);FitCalibrationSession parent;parent.sessionIdentity="phase6a-axle-evidence";parent.process=allLoose.process;parent.hasCoarseExperiment=true;parent.coarseExperiment=allLoose;const auto continued=FitCalibrationLibrary::continuationSession(parent,allLoose,child);bool fresh=true;for(const auto&candidate:continued.fineExperiment.candidates)fresh&=candidate.observations.isEmpty();ok&=require(continued.coarseExperiment.candidates.front().observations.front().result==FitObservation::TooLoose&&continued.fineExperiment.parentArtifactIdentity==allLoose.artifactIdentity&&continued.process.printerIdentity=="Bambu H2D"&&continued.fineExperiment.process.actualPrintedOrientation==allLoose.process.actualPrintedOrientation&&fresh,"continuation preserves parent evidence, lineage, context, and orientation while child evidence starts fresh");const int extensionAt=args.indexOf("--axle-extension-output");if(extensionAt>=0&&extensionAt+1<args.size()){QDir output(args[extensionAt+1]);ok&=require(output.mkpath("."),"extended axle output directory");const QString model=output.filePath("BrickSuite-technic-axle-tip-envelope-perpendicular-coarse-extension-v2.3mf"),sessionPath=output.filePath("BrickSuite-technic-axle-tip-envelope-perpendicular-coarse-extension-v2-session.json");ThreeMfWriter::Options options;options.objectName=extension.artifactIdentity;options.partIdentity=extension.artifactIdentity;options.modelColor=QColor("#A0A5A9");ok&=require(ThreeMfWriter::write(extended.mesh,model,options,&error),"extended axle 3MF export: "+error);QSaveFile file(sessionPath);ok&=require(file.open(QIODevice::WriteOnly)&&file.write(QJsonDocument(FitCalibrationSessionJson::toJson(continued)).toJson(QJsonDocument::Indented))>0&&file.commit(),"extended axle lineage session export");const auto b=extended.analysis.bounds;QTextStream(stdout)<<"axleExtensionArtifact="<<model<<Qt::endl<<"axleExtensionSession="<<sessionPath<<Qt::endl<<"axleExtensionDimensions="<<(b.maximum.x-b.minimum.x)<<'x'<<(b.maximum.y-b.minimum.y)<<'x'<<(b.maximum.z-b.minimum.z)<<" mm"<<Qt::endl;}}}
    if(axleHole.ok){auto allTight=TechnicAxleHoleCalibrationArtifact::observationTemplate(axleHole);allTight.process.printerIdentity="Bambu H2D";allTight.process.materialIdentity="PETG";allTight.process.profileName="0.20mm Standard @BBL H2D";allTight.process.hasNozzleDiameter=true;allTight.process.nozzleDiameterMillimetres=.4;allTight.process.hasLayerHeight=true;allTight.process.layerHeightMillimetres=.2;allTight.process.dimensionalCompensationNotes="None / Bambu Studio defaults";FitCalibrationObservation tight;tight.result=FitObservation::TooTight;tight.repeatNumber=1;tight.performedUtc=QDateTime::currentDateTimeUtc();for(const auto&candidate:allTight.candidates)ok&=require(FitCalibrationEvidencePolicy::addObservation(&allTight,candidate.index,tight,&error),"axle-hole Too Tight evidence recorded");const auto plan=FitCalibrationEvidencePolicy::nextSearchPlan(allTight);ok&=require(allTight.preferredCandidateIndex==0&&plan.uniformResult==FitUniformResult::AllTooTight&&plan.boundary==FitPreferredBoundary::Upper&&FitCalibrationEvidencePolicy::continuationAvailable(allTight),"all Too Tight empty-inside evidence extends toward a larger female opening without Preferred");ok&=require(std::abs(plan.centerCorrectionMillimetres-.45)<1e-9&&std::abs(plan.candidateSpacingMillimetres-.05)<1e-9&&plan.candidateCount==7,"generic female continuation overlaps +0.30 mm and extends through +0.60 mm");TechnicAxleHoleCalibrationArtifactDefinition extension;extension.artifactIdentity="technic-axle-hole-tip-clearance-perpendicular-coarse-extension-v2";extension.parentArtifactIdentity=allTight.artifactIdentity;extension.centerTipToTipCorrectionMillimetres=plan.centerCorrectionMillimetres;extension.candidateSpacingMillimetres=plan.candidateSpacingMillimetres;extension.candidateCount=plan.candidateCount;const auto extended=TechnicAxleHoleCalibrationArtifact::generate(allTight.regenerationPrototype,extension);ok&=require(extended.ok&&extended.candidates.size()==7,"uniform axle-hole evidence generates an extended coarse artifact: "+extended.diagnostic);if(extended.ok){for(int i=0;i<7;++i)ok&=require(std::abs(extended.candidates[i].functionalDiameterMillimetres-(5.10+.05*i))<1e-9,"extended axle-hole candidates span 5.10 through 5.40 mm");const auto child=TechnicAxleHoleCalibrationArtifact::observationTemplate(extended,extension);FitCalibrationSession parent;parent.sessionIdentity="phase6c-axle-hole-evidence";parent.process=allTight.process;parent.hasCoarseExperiment=true;parent.coarseExperiment=allTight;const auto continued=FitCalibrationLibrary::continuationSession(parent,allTight,child);bool fresh=true;for(const auto&candidate:continued.fineExperiment.candidates)fresh&=candidate.observations.isEmpty();ok&=require(continued.coarseExperiment.candidates.size()==7&&continued.coarseExperiment.candidates.front().observations.front().result==FitObservation::TooTight&&continued.fineExperiment.parentArtifactIdentity==allTight.artifactIdentity&&continued.process.printerIdentity=="Bambu H2D"&&continued.process.materialIdentity=="PETG"&&continued.process.profileName=="0.20mm Standard @BBL H2D"&&continued.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate&&fresh,"axle-hole continuation preserves parent evidence, lineage, manufacturing context, and orientation while child evidence starts fresh");const int at=args.indexOf("--axle-hole-extension-output");if(at>=0&&at+1<args.size()){QDir output(args[at+1]);ok&=require(output.mkpath("."),"extended axle-hole output directory");const QString model=output.filePath("BrickSuite-technic-axle-hole-tip-clearance-perpendicular-coarse-extension-v2.3mf"),sessionPath=output.filePath("BrickSuite-technic-axle-hole-tip-clearance-perpendicular-coarse-extension-v2-session.json");ThreeMfWriter::Options options;options.objectName=extension.artifactIdentity;options.partIdentity=extension.artifactIdentity;options.modelColor=QColor("#A0A5A9");ok&=require(ThreeMfWriter::write(extended.mesh,model,options,&error),"extended axle-hole 3MF export: "+error);QSaveFile file(sessionPath);ok&=require(file.open(QIODevice::WriteOnly)&&file.write(QJsonDocument(FitCalibrationSessionJson::toJson(continued)).toJson(QJsonDocument::Indented))>0&&file.commit(),"extended axle-hole lineage session export");const auto b=extended.analysis.bounds;QTextStream(stdout)<<"axleHoleExtensionArtifact="<<model<<Qt::endl<<"axleHoleExtensionSession="<<sessionPath<<Qt::endl<<"axleHoleExtensionDimensions="<<(b.maximum.x-b.minimum.x)<<'x'<<(b.maximum.y-b.minimum.y)<<'x'<<(b.maximum.z-b.minimum.z)<<" mm"<<Qt::endl;}}}
    const auto axleHoleArmWidth=TechnicAxleHoleArmWidthCalibrationArtifact::generate();ok&=require(axleHoleArmWidth.ok&&axleHoleArmWidth.candidates.size()==7,"corrected axle-hole arm-width artifact: "+axleHoleArmWidth.diagnostic);if(axleHoleArmWidth.ok){for(int i=0;i<7;++i)ok&=require(std::abs(axleHoleArmWidth.candidates[i].functionalDiameterMillimetres-(1.60+.10*i))<1e-9,"axle-hole arm opening candidates span 1.60 through 2.20 mm");ok&=require(axleHoleArmWidth.analysis.connectedComponents==1&&axleHoleArmWidth.analysis.boundaryEdges==0&&axleHoleArmWidth.analysis.nonManifoldEdges==0&&axleHoleArmWidth.analysis.selfIntersections==0,"corrected arm-width artifact is a validated printable mesh");const auto repeated=TechnicAxleHoleArmWidthCalibrationArtifact::generate();ok&=require(repeated.ok&&sameMesh(repeated.mesh,axleHoleArmWidth.mesh),"corrected arm-width artifact generation is deterministic");const auto experiment=TechnicAxleHoleArmWidthCalibrationArtifact::observationTemplate(axleHoleArmWidth);ok&=require(experiment.parentArtifactIdentity=="technic-axle-hole-tip-clearance-perpendicular-coarse-extension-v2"&&experiment.fixedDiameterCorrectionMillimetres==.30&&experiment.regenerationPrototype.constructionRecipe=="technic-axle-hole-arm-width-clearance-v2"&&experiment.candidates.front().observations.isEmpty(),"new semantic contract retains old tip-to-tip lineage, fixes 5.10 mm tip clearance, and begins with fresh evidence");const int at=args.indexOf("--axle-hole-arm-width-output");if(at>=0&&at+1<args.size()){QDir output(args[at+1]);ok&=require(output.mkpath("."),"arm-width output directory");const QString model=output.filePath("BrickSuite-technic-axle-hole-arm-width-perpendicular-coarse-v2.3mf"),sessionPath=output.filePath("BrickSuite-technic-axle-hole-arm-width-perpendicular-coarse-v2-session.json");ThreeMfWriter::Options options;options.objectName=experiment.artifactIdentity;options.partIdentity=experiment.artifactIdentity;options.modelColor=QColor("#A0A5A9");ok&=require(ThreeMfWriter::write(axleHoleArmWidth.mesh,model,options,&error),"arm-width 3MF export: "+error);FitCalibrationSession corrected;corrected.sessionIdentity="phase6e-axle-hole-arm-width";corrected.process.printerIdentity="Bambu H2D";corrected.process.materialIdentity="PETG";corrected.process.profileName="0.20mm Standard @BBL H2D";corrected.process.hasNozzleDiameter=true;corrected.process.nozzleDiameterMillimetres=.4;corrected.process.hasLayerHeight=true;corrected.process.layerHeightMillimetres=.2;corrected.process.dimensionalCompensationNotes="None / Bambu Studio defaults";corrected.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;corrected.hasCoarseExperiment=true;corrected.coarseExperiment=experiment;corrected.coarseExperiment.process=corrected.process;QSaveFile file(sessionPath);ok&=require(file.open(QIODevice::WriteOnly)&&file.write(QJsonDocument(FitCalibrationSessionJson::toJson(corrected)).toJson(QJsonDocument::Indented))>0&&file.commit(),"arm-width session export");const auto b=axleHoleArmWidth.analysis.bounds;QTextStream(stdout)<<"axleHoleArmWidthArtifact="<<model<<Qt::endl<<"axleHoleArmWidthSession="<<sessionPath<<Qt::endl<<"axleHoleArmWidthDimensions="<<(b.maximum.x-b.minimum.x)<<'x'<<(b.maximum.y-b.minimum.y)<<'x'<<(b.maximum.z-b.minimum.z)<<" mm"<<Qt::endl;}}
    ok &= testWallPocket(args);
    ok &= testAntiStudBore(args);
    ok &= testStandardBar(args);
    ok &= testCClipBarReceiver(args);
    ok &= testBallJoint(args);
    const int socketRootAt=args.indexOf("--ball-socket-ldraw");
    if(socketRootAt>=0&&socketRootAt+1<args.size()) {
        const auto source=LDrawLibraryService::loadPart(args[socketRootAt+1],"14418");
        ok&=require(source.ok(),"Ball Socket source carrier loads");
        if(source.ok()) {
            const auto socket=BallSocketCalibrationArtifact::generate(source);
            ok&=require(socket.ok,"source-faithful Ball Socket candidates: "+socket.diagnostic);
            if(socket.ok) {
                ok&=require(socket.candidateMeshes.size()==7&&socket.candidates.size()==7,
                    "seven certified Ball Socket candidate meshes and matching evidence rows");
                const auto bounds=analyzeSource(socket.candidateMeshes.front()).bounds;
                QTextStream(stdout)<<"ballSocketBounds="<<bounds.minimum.x<<','<<bounds.minimum.y<<','<<bounds.minimum.z
                    <<" to "<<bounds.maximum.x<<','<<bounds.maximum.y<<','<<bounds.maximum.z
                    <<" vertices="<<socket.candidateMeshes.front().vertices.size()<<Qt::endl;
                const auto semanticSocket=BallSocketSemantic::recognize(source).front();
                const auto orientPoint=[](Point p){return Point{p.x,-p.z,p.y+4.0};};
                const auto orientVector=[](Point p){return Point{p.x,-p.z,p.y};};
                const auto plus=[](Point a,Point b){return Point{a.x+b.x,a.y+b.y,a.z+b.z};};
                const auto minus=[](Point a,Point b){return Point{a.x-b.x,a.y-b.y,a.z-b.z};};
                const auto times=[](Point p,double t){return Point{p.x*t,p.y*t,p.z*t};};
                const auto dot=[](Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;};
                const auto cross=[](Point a,Point b){return Point{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};
                const auto firstHit=[&](const PrintMesh& mesh,Point start,Point direction){
                    double nearest=1e100;
                    for(const auto& face:mesh.faces) {
                        const auto& a=mesh.vertices[face[0]];
                        const auto e1=minus(mesh.vertices[face[1]],a),e2=minus(mesh.vertices[face[2]],a);
                        const auto h=cross(direction,e2);
                        const double det=dot(e1,h);
                        if(std::abs(det)<1e-10)continue;
                        const double inv=1.0/det;
                        const auto s=minus(start,a);
                        const double u=inv*dot(s,h);
                        if(u<0||u>1)continue;
                        const auto q=cross(s,e1);
                        const double v=inv*dot(direction,q);
                        if(v<0||u+v>1)continue;
                        const double t=inv*dot(e2,q);
                        if(t>.01)nearest=std::min(nearest,t);
                    }
                    return nearest;
                };
                constexpr int numeralMasks[7]={0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07};
                const Point numeralProbes[7]={{-5.90,2.72,10},{-5.325,2.17,10},
                    {-5.325,1.05,10},{-5.90,.62,10},{-6.475,1.05,10},
                    {-6.475,2.17,10},{-5.90,1.65,10}};
                bool allNumbersMatch=true;
                for(int candidateIndex=0;candidateIndex<7;++candidateIndex)
                    for(int segment=0;segment<7;++segment) {
                        const double hit=firstHit(socket.candidateMeshes[candidateIndex],
                            numeralProbes[segment],Point{0,0,-1});
                        const bool raised=hit<1.55;
                        allNumbersMatch&=raised==bool(numeralMasks[candidateIndex]&(1<<segment))&&hit<1.85;
                    }
                ok&=require(allNumbersMatch,
                    "all seven exterior embossed numerals are distinct, legible seven-segment patterns");
                const Point center=orientPoint(semanticSocket.frame.origin);
                const Point axis=orientVector(semanticSocket.frame.axis);
                const Point transverse=orientVector(semanticSocket.frame.profileU);
                const double contactLow=firstHit(socket.candidateMeshes.front(),center,transverse);
                const double contactHigh=firstHit(socket.candidateMeshes.back(),center,transverse);
                const double mouthLow=firstHit(socket.candidateMeshes.front(),plus(center,times(axis,1.2)),transverse);
                const double mouthHigh=firstHit(socket.candidateMeshes.back(),plus(center,times(axis,1.2)),transverse);
                QTextStream(stdout)<<"ballSocketContact="<<contactLow<<".."<<contactHigh
                    <<" mouth="<<mouthLow<<".."<<mouthHigh<<Qt::endl;
                const Point second=orientVector(semanticSocket.frame.profileV);
                ok&=require(contactHigh-contactLow>.20&&contactHigh-contactLow<.40&&
                    mouthHigh-mouthLow>.12&&mouthHigh-mouthLow<.30&&
                    firstHit(socket.candidateMeshes[3],center,second)>1e50,
                    "printed source-owned spherical contact and open retaining mouth both vary without closing the compliant gap");
                ok&=require(std::abs(socket.candidates.front().diameterCorrectionMillimetres+.30)<1e-9&&
                    std::abs(socket.candidates[3].diameterCorrectionMillimetres)<1e-9&&
                    std::abs(socket.candidates.back().diameterCorrectionMillimetres-.30)<1e-9&&
                    std::abs(socket.candidates[3].functionalDiameterMillimetres-6.4)<1e-9&&
                    center.z>3.9&&bounds.minimum.z>=0,
                    "seven candidate offsets include nominal #4 and stand the socket clear of the print bed");
                const int outputAt=args.indexOf("--ball-socket-output");
                if(outputAt>=0&&outputAt+1<args.size()) {
                    QDir output(args[outputAt+1]);
                    ok&=require(output.mkpath("."),"Ball Socket artifact output directory");
                    QVector<ThreeMfWriter::NamedMesh> zones;
                    for(int i=0;i<socket.candidateMeshes.size();++i)
                        zones.push_back({QStringLiteral("Candidate %1 — %2 mm contact / throat offset")
                            .arg(i+1).arg(socket.candidates[i].diameterCorrectionMillimetres,0,'f',2),
                            socket.candidateMeshes[i],{8.0+24.0*(i%4),1.6+8.0*(i/4),0.0}});
                    ThreeMfWriter::Options options;
                    options.objectName=socket.artifactIdentity;
                    options.partIdentity=socket.artifactIdentity;
                    options.modelColor=QColor("#A0A5A9");
                    QString error;
                    const QString modelPath=output.filePath("BrickSuite-ball-socket-friction-contact-throat-parallel-coarse-v1.3mf");
                    ok&=require(ThreeMfWriter::writeCollection(zones,modelPath,options,&error),
                        "Ball Socket 3MF collection export: "+error);
                    FitCalibrationSession session;
                    session.sessionIdentity=FitCalibrationLibrary::newStableIdentity();
                    session.process.printerIdentity="Bambu H2D";
                    session.process.materialIdentity="PETG";
                    session.process.profileName="0.20mm Standard @BBL H2D";
                    session.process.hasNozzleDiameter=true;
                    session.process.nozzleDiameterMillimetres=.4;
                    session.process.hasLayerHeight=true;
                    session.process.layerHeightMillimetres=.2;
                    session.process.dimensionalCompensationNotes="None / Bambu Studio defaults";
                    session.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;
                    session.hasCoarseExperiment=true;
                    session.coarseExperiment=BallSocketCalibrationArtifact::observationTemplate(socket);
                    session.process.orientationNotes=session.coarseExperiment.process.orientationNotes;
                    session.coarseExperiment.process=session.process;
                    const QString sessionPath=output.filePath("BrickSuite-ball-socket-friction-contact-throat-parallel-coarse-v1-session.json");
                    QSaveFile file(sessionPath);
                    ok&=require(file.open(QIODevice::WriteOnly)&&
                        file.write(QJsonDocument(FitCalibrationSessionJson::toJson(session)).toJson(QJsonDocument::Indented))>0&&
                        file.commit(),"Ball Socket managed session export");
                    QFile reload(sessionPath);
                    FitCalibrationSession decoded,imported,resumed;
                    bool fresh=reload.open(QIODevice::ReadOnly)&&
                        FitCalibrationSessionJson::fromJson(QJsonDocument::fromJson(reload.readAll()).object(),&decoded,&error)&&
                        !decoded.sessionIdentity.isEmpty()&&decoded.hasCoarseExperiment&&!decoded.hasFineExperiment&&
                        decoded.coarseExperiment.featureFamily==QStringLiteral("BallSocket")&&
                        decoded.coarseExperiment.regenerationPrototype.family==FunctionalInterfaceFamily::BallSocket&&
                        decoded.coarseExperiment.regenerationPrototype.evidenceContract==QStringLiteral("official-ldraw-joint8socket-friction-v1")&&
                        decoded.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate&&
                        decoded.coarseExperiment.candidates.size()==socket.candidates.size()&&
                        decoded.coarseExperiment.preferredCandidateIndex==0&&
                        decoded.coarseExperiment.state!=FitEvidenceState::Verified;
                    if(fresh)for(int i=0;i<socket.candidates.size();++i)
                        fresh&=decoded.coarseExperiment.candidates[i].observations.isEmpty()&&
                            decoded.coarseExperiment.candidates[i].index==i+1&&
                            std::abs(decoded.coarseExperiment.candidates[i].diameterCorrectionMillimetres-
                                     socket.candidates[i].diameterCorrectionMillimetres)<1e-9;
                    ok&=require(fresh,"Ball Socket session reload retains exact unobserved candidates and contract: "+error);
                    QTemporaryDir managedRoot;
                    FitCalibrationLibrary managed(managedRoot.path());
                    FitCalibrationWorkspace selected;
                    selected.process=session.process;
                    ok&=require(fresh&&managed.importSessionIntoWorkspace(sessionPath,&selected,&imported,&error)&&
                        managed.loadSession(imported.sessionIdentity,&resumed,&error)&&
                        resumed.coarseExperiment.candidates.size()==7&&
                        resumed.coarseExperiment.candidates[3].observations.isEmpty(),
                        "Ball Socket managed session imports and resumes: "+error);
                    try {
                        Lib3MF::CWrapper wrapper;
                        auto reopened=wrapper.CreateModel();
                        reopened->QueryReader("3mf")->ReadFromFile(modelPath.toStdString());
                        ok&=require(reopened->GetMeshObjects()->Count()==7,
                            "Ball Socket 3MF reopens with seven printable candidate objects");
                    } catch(const std::exception& exception) {
                        ok&=require(false,QStringLiteral("Ball Socket 3MF reopening failed: %1")
                            .arg(QString::fromUtf8(exception.what())));
                    }
                    QTextStream(stdout)<<"ballSocketArtifact="<<modelPath<<Qt::endl
                        <<"ballSocketSession="<<sessionPath<<Qt::endl;
                }
            }
        }
    }
    return ok?0:1;
}
