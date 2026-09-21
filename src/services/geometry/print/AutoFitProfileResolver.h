#pragma once

#include "../fit/FitCalibrationLibrary.h"
#include "LDrawPrintGeometryBuilder.h"
#include "../PrintOrientation.h"
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
                                            const LDrawSemanticOperandBuilder::Result& semantics,
                                            const PrintOrientation& printOrientation);
    static AutoFitProfileResolution resolve(bool enabled, const QString& partReference,
                                            const QVector<FitProfile>& candidates,
                                            const LDrawGeometry::LDrawLoadResult& source,
                                            const PrintOrientation& printOrientation);
    static AutoFitProfileResolution resolve(bool enabled, const QString& partReference,
                                            const QVector<FitProfile>& candidates,
                                            FitPrintedOrientation orientation);
    static AutoFitProfileResolution resolveManaged(bool enabled, const QString& partReference,
                                                   const FitCalibrationLibrary& library,
                                                   const LDrawGeometry::LDrawLoadResult& source,
                                                   const PrintOrientation& printOrientation);
};
}
