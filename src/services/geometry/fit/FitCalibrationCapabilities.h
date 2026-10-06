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
#include "FitCalibrationExperiment.h"
#include <limits>

namespace PrintGeometry {
// Generation capabilities only: physical observations and verification remain
// governed by FitCalibrationEvidencePolicy. Historical contracts are not rewritten.
struct FitCalibrationCapability {
    QString family, name, variant, variantName, sourcePart, prerequisite;
    QVector<FitPrintedOrientation> orientations;
    QStringList stages={"Coarse Search","Fine Search","Direct Verification","Boundary Extension within source range"};
    bool initial=true, continuation=true;
    double minimumSpacing=.001,maximumSpacing=1.0;
    int minimumCandidates=3, maximumCandidates=7;
    double minimumCorrection=-std::numeric_limits<double>::infinity();
    double maximumCorrection=std::numeric_limits<double>::infinity();
    QString marker="Integrated fixture candidate mapping";
    QString continuationRequirement="Retained authoritative regeneration prototype";
};
class FitCalibrationCapabilities {
public:
    static QVector<FitCalibrationCapability> entries() {
        using O=FitPrintedOrientation;
        const auto perpendicular=O::FeatureAxisPerpendicularToBuildPlate;
        const auto parallel=O::FeatureAxisParallelToBuildPlate;
        QVector<FitCalibrationCapability> result;
        const auto add=[&](QString family,QString name,QString variant={},QString variantName={},QString source={},bool horizontal=false){
            FitCalibrationCapability c;c.family=family;c.name=name;c.variant=variant;c.variantName=variantName;
            c.sourcePart=source;c.orientations={horizontal?parallel:perpendicular};
            if(!source.isEmpty()){c.marker="One to seven relief dots on each candidate";c.continuationRequirement="Installed certified source, matching source contract and supported fixture orientation";}
            result.push_back(c);
        };
        add("RoundTechnicPassage","Technic Hole",{}, {},{},true);
        add("StandardStud","Standard Stud","Diameter","Outside diameter");
        add("StandardStud","Standard Stud","Height","Height");result.last().prerequisite="Verified Standard Stud OD in the same manufacturing workspace";
        add("StudReceivingClutch","Stud Receiving Clutch","TubeWallCell","Tube wall cell");
        add("StudReceivingClutch","Stud Receiving Clutch","PostWallCell","Post wall cell");
        add("StudReceivingClutch","Stud Receiving Clutch","WallPocket","Wall pocket");
        add("StudReceivingClutch","Stud Receiving Clutch","AntiStudBore","Anti-stud bore");
        add("FrictionlessTechnicPin","Frictionless Technic Pin");
        add("FrictionTechnicPin","Friction Technic Pin");
        add("TechnicAxle","Technic Axle");
        add("TechnicAxleHole","Technic Axle Hole");
        add("StandardBar","Standard Bar");
        add("CClipBarReceiver","C-Clip / Bar Receiver");result.last().orientations={perpendicular,parallel};
        add("BallJoint","Ball Joint");result.last().orientations={perpendicular,parallel};
        add("BallSocket","Ball Socket",{},{},"14418",true);result.last().marker="Embossed candidate numeral";result.last().minimumCorrection=-.4;result.last().maximumCorrection=.4;
        add("PinBarrelHinge","Pin / Barrel Hinge",{},{},"3938",true);result.last().minimumCorrection=-.25;result.last().maximumCorrection=.25;
        add("InterleavedFingerHinge","Interleaved-Finger Hinge",{},{},"4275a",true);result.last().minimumCorrection=-.15;result.last().maximumCorrection=.15;
        add("ClickHinge","Click Hinge",{},{},"30345",true);result.last().minimumCorrection=-.15;result.last().maximumCorrection=.15;
        add("RetainedRotatingWheel","Retained Rotating Wheel",{},{},"30027b");result.last().minimumCorrection=-.15;result.last().maximumCorrection=.15;
        add("PlainRoundBoreWheel","Plain Round-Bore Wheel",{},{},"30027a");
        return result;
    }
    static FitCalibrationCapability find(const QString& family,const QString& variant={}) {
        for(const auto& c:entries())if(c.family==family&&(variant.isEmpty()||c.variant==variant))return c;
        FitCalibrationCapability missing;missing.initial=false;missing.continuation=false;return missing;
    }
};
}
