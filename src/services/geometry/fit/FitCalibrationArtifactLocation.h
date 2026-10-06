/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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
