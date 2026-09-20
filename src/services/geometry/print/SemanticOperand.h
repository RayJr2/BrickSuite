#pragma once
#include "PrintMesh.h"
#include <QVector>
#include <QStringList>
namespace PrintGeometry {
enum class SemanticRole { PrimaryBody,SubtractivePassage,AdditiveAttachment,HollowAdditiveAttachment };
enum class SemanticFeature { BodyOrCavity,RoundThroughPassage,Stud,Tube,TubeBoreInterface };
enum class SemanticConfidence { HighConfidence,Ambiguous,Unsupported };
enum class FunctionalInterfaceFamily { RoundTechnicPassage, StandardStud };
enum class FunctionalInterfaceRole { Female, Male };
enum class FunctionalMaterialSide { EmptyInsideMaterialOutside, MaterialInside };
enum class FunctionalEligibility { Eligible, RecognizedNotEligible, Unsupported };
enum class FunctionalOperandAction { Subtract, Unite };
struct FunctionalFeatureFrame { Point origin,axis,profileU,profileV;bool mirrored=false; };
struct FunctionalFeatureProvenance { QString sourceFile;int referenceId=-1,sourceLine=0;bool inverted=false; };
struct FunctionalRadialSection { double axialPositionMillimetres=0.0,radiusMillimetres=0.0; };
struct FunctionalFeature {
    QString stableIdentity;
    FunctionalInterfaceFamily family=FunctionalInterfaceFamily::RoundTechnicPassage;
    FunctionalInterfaceRole role=FunctionalInterfaceRole::Female;
    FunctionalMaterialSide materialSide=FunctionalMaterialSide::EmptyInsideMaterialOutside;
    FunctionalEligibility eligibility=FunctionalEligibility::Unsupported;
    SemanticConfidence confidence=SemanticConfidence::Unsupported;
    FunctionalFeatureFrame frame;
    double nominalRadiusMillimetres=0.0,nominalDiameterMillimetres=0.0,nominalAxialExtentMillimetres=0.0,nominalEngagementExtentMillimetres=0.0;
    FunctionalOperandAction operandAction=FunctionalOperandAction::Subtract;
    QString governingOperandIdentity,constructionRecipe,evidenceContract;
    QVector<FunctionalRadialSection> radialProfile;
    QVector<FunctionalFeatureProvenance> provenance;
};
struct SemanticOperand { SemanticRole role=SemanticRole::PrimaryBody;
    SemanticFeature feature=SemanticFeature::BodyOrCavity;
    SemanticConfidence confidence=SemanticConfidence::Unsupported;
    PrintMesh sourceMesh,closedMesh;MeshAnalysisResult analysis;Point attachmentDirection;
    int closureTriangles=0;QStringList sourceFiles;QVector<FunctionalFeature> functionalFeatures; };
}
