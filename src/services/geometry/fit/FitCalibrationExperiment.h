#pragma once

#include "../print/SemanticOperand.h"
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace PrintGeometry {
enum class FitObservation { Unevaluated, TooTight, Acceptable, Preferred, TooLoose, UnableToEvaluate };
enum class FitEvidenceState { Draft, Experimental, CandidateSelected, Verified, Stale };
enum class FitPrintedOrientation { Unknown, FeatureAxisParallelToBuildPlate, FeatureAxisPerpendicularToBuildPlate, OtherUnsupported };
enum class FitCalibrationStage { None, Coarse, Fine };

struct FitCalibrationObservation { FitObservation result=FitObservation::Unevaluated; int repeatNumber=1; double measuredDiameterMillimetres=0; bool hasMeasuredDiameter=false; QString notes; QDateTime performedUtc; };
struct FitCalibrationCandidate { int index=0; double diameterCorrectionMillimetres=0; double functionalDiameterMillimetres=0; QVector<FitCalibrationObservation> observations; };
struct FitCalibrationProcess { QString printerIdentity; QString materialIdentity; double nozzleDiameterMillimetres=0; bool hasNozzleDiameter=false; QString profileName; double layerHeightMillimetres=0; bool hasLayerHeight=false; FitPrintedOrientation actualPrintedOrientation=FitPrintedOrientation::Unknown; QString orientationNotes; QString dimensionalCompensationNotes; };
struct FitCalibrationExperiment {
    QString artifactIdentity, parentArtifactIdentity, featureFamily, featureRole, modeledOrientationIdentity;
    double centerDiameterCorrectionMillimetres=0, candidateSpacingMillimetres=0;
    FunctionalFeature regenerationPrototype; bool hasRegenerationPrototype=false;
    FitCalibrationProcess process; QVector<FitCalibrationCandidate> candidates;
    int preferredCandidateIndex=0; FitEvidenceState state=FitEvidenceState::Draft; QDateTime performedUtc;
};
class FitCalibrationExperimentJson { public: static constexpr int CurrentFormatVersion=2; static QJsonObject toJson(const FitCalibrationExperiment&); static bool fromJson(const QJsonObject&,FitCalibrationExperiment*,QString*error=nullptr); };
struct FitCalibrationSession { QString sessionIdentity; FitCalibrationProcess process; bool hasCoarseExperiment=false; FitCalibrationExperiment coarseExperiment; bool hasFineExperiment=false; FitCalibrationExperiment fineExperiment; };
class FitCalibrationSessionJson { public: static constexpr int CurrentFormatVersion=3; static QJsonObject toJson(const FitCalibrationSession&); static bool fromJson(const QJsonObject&,FitCalibrationSession*,QString*error=nullptr); };
class FitCalibrationEvidencePolicy {
public:
    static bool addObservation(FitCalibrationExperiment*,int,const FitCalibrationObservation&,QString*error=nullptr);
    static bool selectPreferredCandidate(FitCalibrationExperiment*,int,QString*error=nullptr);
    static bool markVerified(FitCalibrationExperiment*,QString*error=nullptr);
    static bool validate(const FitCalibrationExperiment&,QString*error=nullptr);
    static void markStaleIfArtifactChanged(FitCalibrationExperiment*,const QString&);
    static QString guidanceText(const FitCalibrationExperiment&);
    static FitCalibrationStage preferredSessionStage(const FitCalibrationSession&);
};
} // namespace PrintGeometry
