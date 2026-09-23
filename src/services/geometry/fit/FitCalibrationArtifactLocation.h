#pragma once

#include "FitCalibrationNamingCatalog.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace PrintGeometry {

class FitCalibrationArtifactLocation {
public:
    static QString directory(const QString& testRoot = {}) {
        QString root = testRoot;
        if (root.isEmpty()) root = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if (root.isEmpty()) root = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        return QDir(root).filePath(QStringLiteral("BrickSuite/Calibration Artifacts"));
    }
    static QString fixtureFileName(FitCalibrationNameKey key, const QString& stage, int version) {
        return QStringLiteral("%1-%2-v%3.3mf")
            .arg(QString::fromLatin1(FitCalibrationNamingCatalog::forKey(key).slug), stage)
            .arg(version);
    }
    static QString fixturePath(FitCalibrationNameKey key, const QString& stage, int version,
                               const QString& testRoot = {}) {
        return QDir(directory(testRoot)).filePath(fixtureFileName(key, stage, version));
    }
    static QString companionPath(const QString& fixturePath) {
        const QFileInfo info(fixturePath);
        return QDir(info.absolutePath()).filePath(info.completeBaseName() + QStringLiteral("-session.json"));
    }
};

} // namespace PrintGeometry
