#pragma once

#include "../fit/FitCalibrationLibrary.h"
#include <QString>
#include <QVector>

namespace PrintGeometry {
enum class AutoFitResolutionState { Disabled, UnsupportedPart, NoCompatibleProfile, Ambiguous, Resolved };
struct AutoFitProfileResolution {
    AutoFitResolutionState state = AutoFitResolutionState::Disabled;
    FitProfile profile;
    QString diagnostic;
    bool resolved() const { return state == AutoFitResolutionState::Resolved; }
};
class AutoFitProfileResolver {
public:
    static AutoFitProfileResolution resolve(bool enabled, const QString& partReference,
                                            const QVector<FitProfile>& candidates,
                                            FitPrintedOrientation orientation);
    static AutoFitProfileResolution resolveManaged(bool enabled, const QString& partReference,
                                                   const FitCalibrationLibrary& library,
                                                   FitPrintedOrientation orientation);
};
}
