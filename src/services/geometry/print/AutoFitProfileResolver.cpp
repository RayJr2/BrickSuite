#include "AutoFitProfileResolver.h"
#include "ManufacturingMeshService.h"

namespace PrintGeometry {
AutoFitProfileResolution AutoFitProfileResolver::resolve(bool enabled, const QString& partReference,
                                                         const QVector<FitProfile>& candidates,
                                                         FitPrintedOrientation orientation)
{
    if (!enabled) return {AutoFitResolutionState::Disabled, {}, QStringLiteral("Auto Fit is disabled; nominal Prepared Mesh remains selected.")};
    if (partReference != QStringLiteral("3700")) return {AutoFitResolutionState::UnsupportedPart, {}, QStringLiteral("Auto Fit has no verified manufacturing rule for this Part; nominal Prepared Mesh remains selected.")};
    QVector<FitProfile> compatible;
    for (const auto& profile : candidates)
        if (ManufacturingMeshService::compatibleCorrection(profile, orientation)) compatible.push_back(profile);
    if (compatible.isEmpty()) return {AutoFitResolutionState::NoCompatibleProfile, {}, QStringLiteral("Auto Fit found no compatible managed Verified Fit Profile; nominal Prepared Mesh remains selected.")};
    if (compatible.size() != 1) return {AutoFitResolutionState::Ambiguous, {}, QStringLiteral("Auto Fit found multiple compatible Verified Fit Profiles and will not guess; nominal Prepared Mesh remains selected.")};
    return {AutoFitResolutionState::Resolved, compatible.front(), QStringLiteral("Auto Fit uniquely resolved %1.").arg(compatible.front().name)};
}

AutoFitProfileResolution AutoFitProfileResolver::resolveManaged(bool enabled, const QString& partReference,
                                                                const FitCalibrationLibrary& library,
                                                                FitPrintedOrientation orientation)
{
    QVector<FitProfile> candidates;
    for (const auto& summary : library.profiles()) {
        if (!summary.compatible) continue;
        FitProfile profile;
        if (library.loadProfile(summary.identity, &profile, nullptr)) candidates.push_back(profile);
    }
    return resolve(enabled, partReference, candidates, orientation);
}
}
