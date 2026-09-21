#pragma once

#include "FitCalibrationExperiment.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace PrintGeometry {

struct FitProfileCorrection {
    QString featureFamily;
    QString featureRole;
    QString printedOrientation;
    double valueMillimetres = 0;
    QString units = "millimetres";
    QString semantics = "female-diameter-clearance";
    QString correctionContractVersion = "female-diameter-clearance-v1";
    QString semanticContractVersion;
    QString regeneratorAlgorithmVersion = "functional-operand-regenerator-v1";
    QString calibrationArtifactIdentity;
    bool hasRequiredDiameterCorrection = false;
    double requiredDiameterCorrectionMillimetres = 0;
};

struct FitProfile {
    QString profileIdentity;
    QString name;
    FitCalibrationProcess process;
    QString processFingerprint;
    QString sourceSessionIdentity;
    QDateTime verifiedUtc;
    FitEvidenceState verificationState = FitEvidenceState::Verified;
    QVector<FitProfileCorrection> corrections;
};

struct FitLibraryIssue {
    QString path;
    QString message;
};

struct FitSessionSummary {
    QString identity;
    QString displayName;
    QString path;
    FitEvidenceState state = FitEvidenceState::Draft;
};

struct FitCalibrationWorkspace {
    QString identity;
    QString displayName;
    FitCalibrationProcess process;
    QVector<FitCalibrationSession> featureSessions;
};

struct FitProfileSummary {
    QString identity;
    QString name;
    QString path;
    bool compatible = false;
    QString compatibilityMessage;
};

class FitProfileJson {
public:
    static constexpr int CurrentFormatVersion = 1;
    static QJsonObject toJson(const FitProfile&);
    static bool fromJson(const QJsonObject&, FitProfile*, QString* error = nullptr);
};

class FitCalibrationLibrary {
public:
    explicit FitCalibrationLibrary(QString storageRoot = {});

    static QString defaultStorageRoot();
    QString storageRoot() const { return m_root; }
    QString sessionsDirectory() const;
    QString profilesDirectory() const;

    static QString newStableIdentity();
    static QString processFingerprint(const FitCalibrationProcess&);
    static QString manufacturingContextFingerprint(const FitCalibrationProcess&);
    static QString workspaceDisplayName(const FitCalibrationProcess&);
    static QString currentSemanticContractVersion();
    static QString currentRegeneratorAlgorithmVersion();
    static QString sessionDisplayName(const FitCalibrationSession&);
    static FitCalibrationSession continuationSession(const FitCalibrationSession&, const FitCalibrationExperiment&, FitCalibrationExperiment);

    bool saveSession(FitCalibrationSession*, QString* error = nullptr);
    bool loadSession(const QString& identity, FitCalibrationSession*, QString* error = nullptr) const;
    QVector<FitSessionSummary> sessions(QVector<FitLibraryIssue>* issues = nullptr) const;
    QVector<FitCalibrationWorkspace> workspaces(QVector<FitLibraryIssue>* issues = nullptr) const;

    bool importSession(const QString& portablePath, FitCalibrationSession*, QString* error = nullptr);
    bool importSessionIntoWorkspace(const QString& portablePath, const FitCalibrationWorkspace* selectedWorkspace,
                                    FitCalibrationSession*, QString* error = nullptr);
    bool exportSession(const QString& identity, const QString& portablePath, QString* error = nullptr) const;

    static bool promoteVerifiedSession(const FitCalibrationSession&, const QString& profileName,
                                       FitProfile*, QString* error = nullptr);
    static bool mergeVerifiedSession(const FitCalibrationSession&, FitProfile*, QString* error = nullptr);
    bool saveVerifiedWorkspaceProfile(const FitCalibrationWorkspace&, const QString& profileName,
                                      FitProfile*, QString* error = nullptr);
    bool saveProfile(FitProfile*, QString* error = nullptr);
    bool loadProfile(const QString& identity, FitProfile*, QString* error = nullptr) const;
    QVector<FitProfileSummary> profiles(QVector<FitLibraryIssue>* issues = nullptr) const;
    static bool profileCompatibility(const FitProfile&, QString* reason = nullptr);

private:
    QString m_root;
};

} // namespace PrintGeometry
