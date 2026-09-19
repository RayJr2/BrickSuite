#pragma once
#include "PrintMesh.h"
#include <QStringList>
namespace PrintGeometry {
enum class SemanticRole { PrimaryBody,SubtractivePassage,AdditiveAttachment,HollowAdditiveAttachment };
enum class SemanticFeature { BodyOrCavity,RoundThroughPassage,Stud,Tube,TubeBoreInterface };
enum class SemanticConfidence { HighConfidence,Ambiguous,Unsupported };
struct SemanticOperand { SemanticRole role=SemanticRole::PrimaryBody;
    SemanticFeature feature=SemanticFeature::BodyOrCavity;
    SemanticConfidence confidence=SemanticConfidence::Unsupported;
    PrintMesh sourceMesh,closedMesh;MeshAnalysisResult analysis;Point attachmentDirection;
    int closureTriangles=0;QStringList sourceFiles; };
}
