#pragma once

#include <QString>
#include <QVector>

enum class FitCalibrationFamily {
    TechnicHole,
    StandardStud,
    StudReceivingClutch,
    FrictionlessTechnicPin,
    FrictionTechnicPin,
    TechnicAxle,
    TechnicAxleHole
};

struct FitCalibrationFamilyOption {
    FitCalibrationFamily family;
    QString label;
};

class FitCalibrationFamilyCatalog {
public:
    static QVector<FitCalibrationFamilyOption> availableFamilies();
};
