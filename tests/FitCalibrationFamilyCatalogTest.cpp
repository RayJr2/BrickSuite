#include "../src/ui/parts/FitCalibrationFamilyCatalog.h"

#include <QCoreApplication>
#include "../src/services/geometry/fit/FitCalibrationCapabilities.h"
#include <QSet>
#include <QTextStream>

namespace {
bool require(bool condition, const QString& message)
{
    if (!condition)
        QTextStream(stderr) << "FAIL: " << message << Qt::endl;
    return condition;
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    const auto options = FitCalibrationFamilyCatalog::availableFamilies();
    bool ok = require(options.size() == 16, "all implemented calibration families are selectable");

    const QVector<FitCalibrationFamily> expectedFamilies = {
        FitCalibrationFamily::TechnicHole,
        FitCalibrationFamily::StandardStud,
        FitCalibrationFamily::StudReceivingClutch,
        FitCalibrationFamily::FrictionlessTechnicPin,
        FitCalibrationFamily::FrictionTechnicPin,
        FitCalibrationFamily::TechnicAxle,
        FitCalibrationFamily::TechnicAxleHole,
        FitCalibrationFamily::StandardBar,
        FitCalibrationFamily::CClipBarReceiver,
        FitCalibrationFamily::BallJoint,FitCalibrationFamily::BallSocket,FitCalibrationFamily::PinBarrelHinge,
        FitCalibrationFamily::InterleavedFingerHinge,FitCalibrationFamily::ClickHinge,FitCalibrationFamily::RetainedRotatingWheel,FitCalibrationFamily::PlainRoundBoreWheel
    };
    const QStringList expectedLabels = {
        QStringLiteral("Technic Hole"),
        QStringLiteral("Standard Stud"),
        QStringLiteral("Stud Receiving Clutch"),
        QStringLiteral("Frictionless Technic Pin"),
        QStringLiteral("Friction Technic Pin"),
        QStringLiteral("Technic Axle"),
        QStringLiteral("Technic Axle Hole"),
        QStringLiteral("Standard Bar"),
        QStringLiteral("C-Clip / Bar Receiver"),
        QStringLiteral("Ball Joint"),"Ball Socket","Pin / Barrel Hinge","Interleaved-Finger Hinge","Click Hinge","Retained Rotating Wheel","Plain Round-Bore Wheel"
    };
    QSet<int> uniqueFamilies;
    for (qsizetype i = 0; i < expectedFamilies.size(); ++i) {
        ok &= require(options[i].family == expectedFamilies[i],
                      QStringLiteral("family %1 retains its creation dispatch identity").arg(i));
        ok &= require(options[i].label == expectedLabels[i],
                      QStringLiteral("family %1 has the expected selector label").arg(i));
        uniqueFamilies.insert(int(options[i].family));
    }
    ok &= require(uniqueFamilies.size() == expectedFamilies.size(),
                  "each selector entry has one unambiguous creation path");
    for(const auto& c:PrintGeometry::FitCalibrationCapabilities::entries()){
        ok&=require(c.initial&&c.continuation&&!c.orientations.isEmpty()&&c.maximumCandidates==7&&!c.stages.isEmpty(),"registry exposes supported stage/orientation/count rules");
        if(!c.sourcePart.isEmpty())ok&=require(!c.marker.isEmpty()&&!c.continuationRequirement.isEmpty(),"source families describe identity and continuation requirements");
    }
    return ok ? 0 : 1;
}
