#include "../src/ui/parts/FitCalibrationFamilyCatalog.h"

#include <QCoreApplication>
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
    bool ok = require(options.size() == 10, "all implemented calibration families are selectable");

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
        FitCalibrationFamily::BallJoint
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
        QStringLiteral("Ball Joint")
    };
    QSet<int> uniqueFamilies;
    for (qsizetype i = 0; i < options.size(); ++i) {
        ok &= require(options[i].family == expectedFamilies[i],
                      QStringLiteral("family %1 retains its creation dispatch identity").arg(i));
        ok &= require(options[i].label == expectedLabels[i],
                      QStringLiteral("family %1 has the expected selector label").arg(i));
        uniqueFamilies.insert(int(options[i].family));
    }
    ok &= require(uniqueFamilies.size() == options.size(),
                  "each selector entry has one unambiguous creation path");
    return ok ? 0 : 1;
}
