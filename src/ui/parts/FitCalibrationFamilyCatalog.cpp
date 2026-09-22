#include "FitCalibrationFamilyCatalog.h"

QVector<FitCalibrationFamilyOption> FitCalibrationFamilyCatalog::availableFamilies()
{
    return {
        {FitCalibrationFamily::TechnicHole, QStringLiteral("Technic Hole")},
        {FitCalibrationFamily::StandardStud, QStringLiteral("Standard Stud")},
        {FitCalibrationFamily::StudReceivingClutch, QStringLiteral("Stud Receiving Clutch")},
        {FitCalibrationFamily::FrictionlessTechnicPin, QStringLiteral("Frictionless Technic Pin")},
        {FitCalibrationFamily::FrictionTechnicPin, QStringLiteral("Friction Technic Pin")},
        {FitCalibrationFamily::TechnicAxle, QStringLiteral("Technic Axle")},
        {FitCalibrationFamily::TechnicAxleHole, QStringLiteral("Technic Axle Hole")}
    };
}
