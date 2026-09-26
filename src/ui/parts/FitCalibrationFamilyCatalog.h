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
    TechnicAxleHole,
    StandardBar,
    CClipBarReceiver,
    BallJoint, BallSocket, PinBarrelHinge, InterleavedFingerHinge, ClickHinge, RetainedRotatingWheel, PlainRoundBoreWheel
};

struct FitCalibrationFamilyOption {
    FitCalibrationFamily family;
    QString label;
    QString identity;
};

class FitCalibrationFamilyCatalog {
public:
    static QVector<FitCalibrationFamilyOption> availableFamilies();
};
