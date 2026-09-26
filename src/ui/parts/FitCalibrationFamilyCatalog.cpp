#include "FitCalibrationFamilyCatalog.h"
#include "../../services/geometry/fit/FitCalibrationCapabilities.h"
#include <QSet>
QVector<FitCalibrationFamilyOption> FitCalibrationFamilyCatalog::availableFamilies()
{
    QVector<FitCalibrationFamilyOption> result;QSet<QString> seen;
    for(const auto& capability:PrintGeometry::FitCalibrationCapabilities::entries()) {
        if(!capability.initial||seen.contains(capability.family))continue;
        seen.insert(capability.family);
        result.push_back({FitCalibrationFamily(result.size()),capability.name,capability.family});
    }
    return result;
}
