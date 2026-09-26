#include "AutoFitProfileResolver.h"
#include "ManufacturingMeshService.h"

namespace PrintGeometry {
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
    if (compatible.isEmpty()) return {AutoFitResolutionState::NoCompatibleProfile, {}, QStringLiteral("Auto Fit found no compatible managed Verified Fit Profile; nominal Prepared Mesh remains selected.")};
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
    if(compatible.isEmpty())return {AutoFitResolutionState::NoCompatibleProfile,{},QStringLiteral("Auto Fit found no compatible managed Verified Fit Profile; nominal Prepared Mesh remains selected.")};
    if(compatible.size()!=1)return {AutoFitResolutionState::Ambiguous,{},QStringLiteral("Auto Fit found multiple compatible Verified Fit Profiles and will not guess; nominal Prepared Mesh remains selected.")};
    return {AutoFitResolutionState::Resolved,compatible.front(),QStringLiteral("Auto Fit uniquely resolved %1.").arg(compatible.front().name)};
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
