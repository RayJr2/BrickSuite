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

#include "AutoFitProfileResolver.h"
#include "ManufacturingMeshService.h"

namespace PrintGeometry {
namespace {
QString noProfileDiagnostic(bool recognized)
{
    return recognized?
        QStringLiteral("Auto Fit found no compatible managed Verified Fit Profile for the recognized fit features; nominal Prepared Mesh remains selected."):
        QStringLiteral("No supported fit features were recognized; nominal Prepared Mesh remains selected.");
}
}
AutoFitProfileResolution AutoFitProfileResolver::resolve(bool enabled, const QString& partReference,
                                                         const QVector<FitProfile>& candidates,
                                                         const LDrawSemanticOperandBuilder::Result& semantics,
                                                         const PrintOrientation& printOrientation)
{
    if (!enabled) return {AutoFitResolutionState::Disabled, {}, QStringLiteral("Auto Fit is disabled; nominal Prepared Mesh remains selected.")};
    if (partReference.isEmpty()) return {AutoFitResolutionState::UnsupportedPart, {}, QStringLiteral("Auto Fit requires a resolved Part identity; nominal Prepared Mesh remains selected.")};
    QVector<FitProfile> compatible;
    for (const auto& profile : candidates)
        if (ManufacturingMeshService::hasApplicableCorrection(profile, semantics, printOrientation)) compatible.push_back(profile);
    if (compatible.isEmpty()) return {AutoFitResolutionState::NoCompatibleProfile, {}, noProfileDiagnostic(ManufacturingMeshService::hasRecognizedFitFeatures(semantics))};
    if (compatible.size() != 1) return {AutoFitResolutionState::Ambiguous, {}, QStringLiteral("Auto Fit found multiple compatible Verified Fit Profiles and will not guess; nominal Prepared Mesh remains selected.")};
    return {AutoFitResolutionState::Resolved, compatible.front(), QStringLiteral("Auto Fit uniquely resolved %1.").arg(compatible.front().name)};
}

AutoFitProfileResolution AutoFitProfileResolver::resolve(bool enabled,const QString&partReference,const QVector<FitProfile>&candidates,FitPrintedOrientation orientation)
{
    if(!enabled)return {AutoFitResolutionState::Disabled,{},QStringLiteral("Auto Fit is disabled; nominal Prepared Mesh remains selected.")};
    if(partReference.isEmpty())return {AutoFitResolutionState::UnsupportedPart,{},QStringLiteral("Auto Fit requires a resolved Part identity; nominal Prepared Mesh remains selected.")};
    QVector<FitProfile> compatible;for(const auto&profile:candidates)if(ManufacturingMeshService::compatibleCorrections(profile,orientation).any())compatible.push_back(profile);
    if(compatible.isEmpty())return {AutoFitResolutionState::NoCompatibleProfile,{},QStringLiteral("Auto Fit found no compatible managed Verified Fit Profile; nominal Prepared Mesh remains selected.")};
    if(compatible.size()!=1)return {AutoFitResolutionState::Ambiguous,{},QStringLiteral("Auto Fit found multiple compatible Verified Fit Profiles and will not guess; nominal Prepared Mesh remains selected.")};
    return {AutoFitResolutionState::Resolved,compatible.front(),QStringLiteral("Auto Fit uniquely resolved %1.").arg(compatible.front().name)};
}

AutoFitProfileResolution AutoFitProfileResolver::resolve(bool enabled,const QString&partReference,const QVector<FitProfile>&candidates,const LDrawGeometry::LDrawLoadResult&source,const PrintOrientation&printOrientation)
{
    if(!enabled)return {AutoFitResolutionState::Disabled,{},QStringLiteral("Auto Fit is disabled; nominal Prepared Mesh remains selected.")};
    if(!source.externalFilePath.isEmpty()||partReference.isEmpty())return {AutoFitResolutionState::UnsupportedPart,{},QStringLiteral("Auto Fit requires a resolved Part identity; nominal Prepared Mesh remains selected.")};
    QVector<FitProfile> compatible;for(const auto&profile:candidates)if(ManufacturingMeshService::hasApplicableCorrection(profile,source,printOrientation))compatible.push_back(profile);
    if(compatible.isEmpty())return {AutoFitResolutionState::NoCompatibleProfile,{},noProfileDiagnostic(ManufacturingMeshService::hasRecognizedFitFeatures(source))};
    if(compatible.size()!=1)return {AutoFitResolutionState::Ambiguous,{},QStringLiteral("Auto Fit found multiple compatible Verified Fit Profiles and will not guess; nominal Prepared Mesh remains selected.")};
    return {AutoFitResolutionState::Resolved,compatible.front(),QStringLiteral("Auto Fit uniquely resolved %1.").arg(compatible.front().name)};
}

AutoFitProfileResolution AutoFitProfileResolver::resolveExplicit(const FitProfile& profile,
    const QString& partReference,const LDrawGeometry::LDrawLoadResult& source,
    const PrintOrientation& orientation)
{
    auto result=resolve(true,partReference,{profile},source,orientation);
    result.diagnostic=result.resolved()
        ?QStringLiteral("Explicit Verified Fit Profile: %1. Only compatible source features and orientation evidence apply.").arg(profile.name)
        :QStringLiteral("Selected profile %1 has no applicable Verified evidence for this source/orientation; nominal Prepared Mesh remains available. %2")
            .arg(profile.name,result.diagnostic);
    return result;
}

AutoFitProfileResolution AutoFitProfileResolver::resolveManaged(bool enabled, const QString& partReference,
                                                                const FitCalibrationLibrary& library,
                                                                const LDrawGeometry::LDrawLoadResult& source,
                                                                const PrintOrientation& printOrientation)
{
    QVector<FitProfile> candidates;
    for (const auto& summary : library.profiles()) {
        if (!summary.compatible) continue;
        FitProfile profile;if (library.loadProfile(summary.identity, &profile, nullptr))candidates.push_back(profile);
    }
    return resolve(enabled,partReference,candidates,source,printOrientation);
}
}
