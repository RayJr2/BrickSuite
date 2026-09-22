#pragma once

#include "FitCalibrationExperiment.h"
#include "../print/PrintMesh.h"

#include <QJsonObject>

namespace PrintGeometry {

struct FitCandidateValue {
    int index = 0;
    QString identity;
    double correctionMillimetres = 0;
    double functionalValueMillimetres = 0;
    Point position;
};

class FitCandidateSeries {
public:
    static bool generate(const QString& artifactIdentity, double centerMillimetres,
                         double spacingMillimetres, int count, QVector<FitCandidateValue>* values,
                         QString* error = nullptr);
};

// Witnesses are deliberately inside material or empty space, not on a mesh face.
// They prove that a physical notch/key survived boolean composition.
struct FitCandidateOneMarker {
    QString kind;
    Point emptyWitness;
    Point materialWitness;
    Point ordinaryEndWitness;
    int candidateIndex = 1;
};

struct FitCalibrationZone {
    QString identity;
    int version = 1;
    QString featureFamily;
    QString variant;
    QString correctionSemantic;
    QString semanticContract;
    QString artifactIdentity;
    QString sessionIdentity;
    QString manufacturingContextFingerprint;
    QString modeledOrientationIdentity;
    FitPrintedOrientation intendedPrintOrientation = FitPrintedOrientation::Unknown;
    QVector<FitCandidateValue> candidates;
    MeshBounds bounds;
    QString meshSha256;
    double clearanceMillimetres = 0;
    FitCandidateOneMarker marker;
    // Phase 1 packages reference standalone 3MF files. Translation is reserved for
    // a later coordinated plate; no mesh is transformed by this metadata.
    QString memberFile;
    Point translation;
    PrintMesh mesh;
};

struct FitCalibrationPackageManifest {
    static constexpr int CurrentVersion = 1;
    QString identity;
    int version = CurrentVersion;
    QString manufacturingContextFingerprint;
    QVector<FitCalibrationZone> zones;
};

class FitCalibrationPackage {
public:
    static FitCalibrationZone standardStudOdZone(const PrintMesh&, const MeshBounds&,
                                                  const FitCalibrationExperiment&,
                                                  const QString& sessionIdentity,
                                                  const QString& contextFingerprint,
                                                  const QString& memberFile);
    static FitCalibrationZone postWallCellZone(const PrintMesh&, const MeshBounds&,
                                               const FitCalibrationExperiment&,
                                               const QString& sessionIdentity,
                                               const QString& contextFingerprint,
                                               const QString& memberFile);
    static bool validateZone(const FitCalibrationZone&, QString* error = nullptr);
    static bool validate(const FitCalibrationPackageManifest&,
                         const QVector<FitCalibrationSession>& sessions,
                         QString* error = nullptr);
    static QJsonObject zoneToJson(const FitCalibrationZone&);
    static bool zoneFromJson(const QJsonObject&, FitCalibrationZone*, QString* error = nullptr);
    static QJsonObject toJson(const FitCalibrationPackageManifest&);
    static bool fromJson(const QJsonObject&, FitCalibrationPackageManifest*, QString* error = nullptr);
    static Point placedMarker(const FitCalibrationZone&);
};

} // namespace PrintGeometry
