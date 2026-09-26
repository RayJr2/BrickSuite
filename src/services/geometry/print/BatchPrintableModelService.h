#pragma once

#include "PrintPreparation.h"
#include "../PrintOrientation.h"

#include <QColor>
#include <QVector>
#include <QStringList>
#include <functional>
#include <QJsonObject>
#include <QMutex>

namespace PrintGeometry {

struct BatchPrintablePart {
    QString partNumber;
    QString category;
    int categoryId = 0;
    int rebrickableCategoryId = 0;
    QString material;
    QStringList ldrawCandidates;
    bool catalogPresent = false;
    bool noColor = false;
    int partId = 0;
};

enum class BatchPrintCategory {
    Success, SkippedNoColor, SkippedStickerCategory, SkippedNonstandardId, NoCatalogPart, NoLDrawModel, LoadFailed,
    PrepareNotReady, ResourceLimit, SourceCoverage, ManifoldFailure,
    SelfIntersection, ManufacturingFailed, ExportFailed, ReopenFailed,
    Cancelled, NotStarted, StrictOverrideSuccess, UserOverrideSuccess
};

struct BatchPrintResult {
    int sequence = 0;
    QString partNumber, ldrawModel, preparationRoute, recognizedFeatures;
    QString fitResolution, profileIdentity, correctionSummary;
    BatchPrintCategory category = BatchPrintCategory::NotStarted;
    BatchPrintCategory nativeCategory = BatchPrintCategory::NotStarted;
    QString nativeDiagnostic;
    QString localOverrideState = QStringLiteral("not_checked");
    QString localOverrideDiagnostic, localOverrideRoute, localOverrideIdentity;
    bool localOverrideUsed = false, localOverrideStale = false;
    QString diagnostic, exportPath;
    QString diagnosticExportPath;
    std::size_t sourceTriangles = 0, sourceGroups = 0;
    std::size_t preparedVertices = 0, preparedFaces = 0;
    bool coverageComplete = false, reopened = false, diagnosticExportAvailable = false;
    qint64 prepareMilliseconds = 0, totalMilliseconds = 0;
};

struct BatchPrintTotals {
    int input = 0, attempted = 0, eligible = 0, modelAvailable = 0;
    int successful = 0, skippedNoColor = 0, skippedStickerCategory = 0;
    int nativeSuccessful = 0, overrideRecoveries = 0;
    int skippedNonstandardIds = 0, noModel = 0, prepareFailures = 0;
    int manufacturingFailures = 0, exportFailures = 0;
    double modelAvailabilityPercent() const;
    double printServicePercent() const;
    double practicalPrintablePercent() const;
    double catalogToPrintablePercent() const;
};

struct BatchPrintPopulation {
    int catalogTotal = 0;
    int excludedNoColor = 0;
    int excludedStickerCategory = 0;
    int excludedNonstandardId = 0;
    int excludedNoModel = 0;
    int eligibleTotal = 0;
    int actualSampled = 0;
};

// Serializes worker checkpoints and immediate UI Stop requests to the same file.
class BatchPrintRunState {
public:
    bool save(const QString& path,const QJsonObject& state,QString* error);
    bool requestStop(QString* error = nullptr);
private:
    bool write(QString* error);
    QMutex m_mutex;
    QString m_path;
    QJsonObject m_state;
    bool m_stopRequested = false;
};

struct BatchPrintOptions {
    QString libraryRoot, outputRoot, runId;
    // Empty uses the normal application override store. Load-only during audits.
    QString localOverrideRoot;
    quint32 seed = 0;
    PrintOrientation printOrientation;
    QColor modelColor = QColor(QStringLiteral("#A0A5A9"));
    bool autoFitEnabled = true;
    bool excludeNonstandardIds = true;
    bool excludeNoModel = false;
    bool randomSample = false;
    int requestedEligibleCount = 0;
    BatchPrintPopulation population;
    std::shared_ptr<BatchPrintRunState> runState;
    std::function<void(int,const QString&,const QString&)> phaseProgress;
    std::function<bool(const QString&,std::size_t,QString*)> reopenValidator;
    // Called only after the active Part checkpoint is durable, before model loading.
    std::function<void(const QString&,int,const QString&)> beforePart;
};

struct BatchPrintRun {
    QString runDirectory, csvPath, metadataPath, statePath, diagnostic;
    BatchPrintPopulation population;
    QVector<BatchPrintResult> results;
    BatchPrintTotals totals;
    bool ok = false, stopped = false;
};

class BatchPrintableModelService {
public:
    using Progress = std::function<void(const BatchPrintResult&,const BatchPrintTotals&)>;
    static QVector<BatchPrintablePart> loadCatalog(const QString& databasePath,QString* error = nullptr);
    static QVector<BatchPrintablePart> explicitParts(const QVector<BatchPrintablePart>& catalog,
                                                      const QStringList& partNumbers);
    static QVector<BatchPrintablePart> randomSample(const QVector<BatchPrintablePart>& catalog,
                                                     int count,quint32 seed,bool excludeNonstandardIds = true,
                                                     BatchPrintPopulation* population = nullptr,
                                                     bool excludeNoModel = false,const QString& libraryRoot = {});
    static bool isStandardAuditPartNumber(const QString& partNumber);
    static bool isStickerCategory(const BatchPrintablePart& part);
    static bool writeDiagnosticThreeMf(const PrintMesh& candidate,const QString& path,
                                       const QString& partNumber,const QColor& color,QString* error = nullptr,
                                       const CancellationState* cancellation = nullptr,
                                       const std::function<void(const QString&)>& phase = {});
    static bool diagnosticWithinBounds(const PrintMesh& mesh);
    static BatchPrintTotals summarize(const QVector<BatchPrintResult>& results);
    static QString categoryCode(BatchPrintCategory category);
    BatchPrintRun run(const QVector<BatchPrintablePart>& parts,const BatchPrintOptions& options,
                      CancellationState* cancellation = nullptr,const Progress& progress = {}) const;
};

} // namespace PrintGeometry
