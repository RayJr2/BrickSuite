/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include "PrintMesh.h"
#include <QVector>
#include <QStringList>
namespace PrintGeometry {
enum class SemanticRole { PrimaryBody,SubtractivePassage,AdditiveAttachment,HollowAdditiveAttachment };
enum class SemanticFeature { BodyOrCavity,RoundThroughPassage,Stud,Tube,TubeBoreInterface };
enum class SemanticConfidence { HighConfidence,Ambiguous,Unsupported };
enum class FunctionalInterfaceFamily { RoundTechnicPassage, StandardStud, StudReceivingClutch, FrictionlessTechnicPin, FrictionTechnicPin, TechnicAxle, TechnicAxleHole, StandardBar, CClipBarReceiver, BallJoint, BallSocket, PinBarrelHinge, InterleavedFingerHinge, ClickHinge, RetainedRotatingWheel, PlainRoundBoreWheel };
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
    double protectedInnerRadiusMillimetres=0.0;
    double protectedCrossArmHalfWidthMillimetres=0.0,nominalCrossShoulderRadiusMillimetres=0.0;
    FunctionalOperandAction operandAction=FunctionalOperandAction::Subtract;
    QString governingOperandIdentity,constructionRecipe,evidenceContract;
    QVector<FunctionalRadialSection> radialProfile;
    QVector<FunctionalFeatureProvenance> provenance;
};
struct SemanticOperand { SemanticRole role=SemanticRole::PrimaryBody;
    SemanticFeature feature=SemanticFeature::BodyOrCavity;
    SemanticConfidence confidence=SemanticConfidence::Unsupported;
    PrintMesh sourceMesh,closedMesh;MeshAnalysisResult analysis;Point attachmentDirection;
    int closureTriangles=0;int compositionPriority=0;bool conformingBodyContact=false;QVector<int> sourceTriangleIndices;QStringList sourceFiles;QVector<FunctionalFeature> functionalFeatures; };
}
