#pragma once

#include <QString>
#include <QVector>

enum class FitCalibrationFamily {
    TechnicHole,
    StandardStud,
    StudReceivingClutch,
    FrictionlessTechnicPin
};

struct FitCalibrationFamilyOption {
    FitCalibrationFamily family;
    QString label;
};

class FitCalibrationFamilyCatalog {
public:
    static QVector<FitCalibrationFamilyOption> availableFamilies();
};
