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

#include "ManufacturingMesh.h"
#include <algorithm>
#include <cmath>

namespace PrintGeometry {
// Reporting only: recognition never establishes correction application or ownership.
struct ManufacturingFitSummary {
    int recognized=0, applied=0, nonzero=0, verifiedZero=0;
    bool partial() const { return applied>0&&applied<recognized; }
    static ManufacturingFitSummary fromLedger(const QVector<AppliedFitFeature>& ledger,int recognizedCount) {
        ManufacturingFitSummary result;result.recognized=recognizedCount;
        for(const auto& entry:ledger){++result.applied;if(entry.nonzero)++result.nonzero;else ++result.verifiedZero;}
        return result;
    }
    QString status() const {
        return partial()?QStringLiteral("partial_verified_fit"):
            nonzero>0?QStringLiteral("verified_nonzero_applied"):
            verifiedZero>0?QStringLiteral("verified_zero_applied"):
            QStringLiteral("nominal_no_verified_application");
    }
    QString description() const {
        if(!applied)return QStringLiteral("Nominal output; no compatible Verified corrections were applied.");
        const QString counts=QStringLiteral("%1 of %2 recognized interfaces corrected (%3 nonzero, %4 Verified-zero).")
            .arg(applied).arg(recognized).arg(nonzero).arg(verifiedZero);
        if(partial())return QStringLiteral("Partial Verified Fit applied. ")+counts;
        return (nonzero?QStringLiteral("Verified Fit corrections applied. "):
            QStringLiteral("Verified Fit applied with zero dimensional adjustment. "))+counts;
    }
    QString exportCompletion(bool reopened) const {
        return description()+(reopened?QStringLiteral(" Exported and reopened successfully."):
            QStringLiteral(" Exported successfully."));
    }
    // Preserve Phase A source/operand equivalence for reporting counts only.
    static void appendRecognized(QVector<FunctionalFeature>& features,const QVector<FunctionalFeature>& additions) {
        for(const auto& candidate:additions){
            const auto equivalent=[&](const FunctionalFeature& feature){
                if(feature.stableIdentity==candidate.stableIdentity)return true;
                const auto& a=feature.frame;const auto& b=candidate.frame;
                return feature.family==candidate.family&&feature.role==candidate.role&&
                    feature.evidenceContract==candidate.evidenceContract&&
                    std::abs(a.origin.x-b.origin.x)<=1e-6&&std::abs(a.origin.y-b.origin.y)<=1e-6&&
                    std::abs(a.origin.z-b.origin.z)<=1e-6&&
                    std::abs(a.axis.x*b.axis.x+a.axis.y*b.axis.y+a.axis.z*b.axis.z)>=1.0-1e-6;
            };
            if(std::none_of(features.cbegin(),features.cend(),equivalent))features.append(candidate);
        }
    }
};
}
