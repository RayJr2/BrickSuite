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

#include "FitCalibrationLibrary.h"
#include <functional>

namespace PrintGeometry {

// Owns generation and publication, never observations, verification, or profiles.
class FitCalibrationGenerationService {
public:
    enum class Stage { Coarse, FineSearch, Verification };
    struct Request {
        QString family, variant;
        Stage stage = Stage::Coarse;
        FitPrintedOrientation orientation = FitPrintedOrientation::Unknown;
        FitCalibrationWorkspace workspace;
        bool hasParent = false;
        FitCalibrationSession parent;
        // Zero retains the existing generator/evidence-policy candidate definition.
        double candidateSpacing = 0;
        int candidateCount = 0;
        QString libraryRoot;
    };
    struct Result {
        bool published = false, registered = false;
        QString directory, fixturePath, companionPath, diagnostic;
        FitCalibrationSession session;
        bool ok() const { return published && registered; }
    };
    struct PackageRequest { QVector<Request> selections; };
    struct FixturePlan { QString name, fileName; QVector<int> selections; bool combinedCore=false; };
    struct PackagePlan {
        QVector<FixturePlan> fixtures;
        QString diagnostic;
        bool ok() const {return diagnostic.isEmpty()&&!fixtures.isEmpty();}
    };
    struct PackageResult {
        bool published=false,registered=false;
        int registeredCount=0;
        QString directory,companionPath,diagnostic;
        QStringList fixturePaths;
        QVector<FitCalibrationSession> sessions;
        bool ok() const {return published&&registered;}
    };
    using Progress=std::function<void(const QString&)>;
    static PackageRequest recommended(const FitCalibrationWorkspace&,const QString& libraryRoot={});
    static PackagePlan planPackage(const PackageRequest&);
    static bool isPackageCompanion(const QString& path);
    PackageResult generatePackage(const PackageRequest&,Progress progress={}) const;
    PackageResult recoverPackage(const QString& directory,Progress progress={}) const;
    enum class Checkpoint { GeometryWritten, BeforeReopen, Published, Registered };
    // Optional fault/observation seam. Normal callers leave this empty.
    using Observer = std::function<bool(Checkpoint,const QString&,QString*)>;
    explicit FitCalibrationGenerationService(QString artifactRoot = {}, QString managedRoot = {}, Observer observer = {});
    static QString unavailableReason(const Request&);
    Result generate(const Request&) const;
    Result recover(const QString& publishedDirectory) const;
private:
    QString m_artifactRoot, m_managedRoot;
    Observer m_observer;
};
} // namespace PrintGeometry
