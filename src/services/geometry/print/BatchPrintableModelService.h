#pragma once

#include "PrintPreparation.h"
#include "../PrintOrientation.h"

#include <QColor>
#include <QVector>
#include <QStringList>
#include <functional>

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
};

enum class BatchPrintCategory {
    Success, SkippedNoColor, SkippedStickerCategory, SkippedNonstandardId, NoCatalogPart, NoLDrawModel, LoadFailed,
    PrepareNotReady, ResourceLimit, SourceCoverage, ManifoldFailure,
    SelfIntersection, ManufacturingFailed, ExportFailed, ReopenFailed,
    Cancelled, NotStarted
};

struct BatchPrintResult {
    int sequence = 0;
    QString partNumber, ldrawModel, preparationRoute, recognizedFeatures;
    QString fitResolution, profileIdentity, correctionSummary;
    BatchPrintCategory category = BatchPrintCategory::NotStarted;
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
    int skippedNonstandardIds = 0, noModel = 0, prepareFailures = 0;
    int manufacturingFailures = 0, exportFailures = 0;
    double modelAvailabilityPercent() const;
    double printServicePercent() const;
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

struct BatchPrintOptions {
    QString libraryRoot, outputRoot, runId;
    quint32 seed = 0;
    PrintOrientation printOrientation;
    QColor modelColor = QColor(QStringLiteral("#A0A5A9"));
    bool autoFitEnabled = true;
    bool excludeNonstandardIds = true;
    bool excludeNoModel = false;
    bool randomSample = false;
    int requestedEligibleCount = 0;
    BatchPrintPopulation population;
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
                                       const QString& partNumber,const QColor& color,QString* error = nullptr);
    static BatchPrintTotals summarize(const QVector<BatchPrintResult>& results);
    static QString categoryCode(BatchPrintCategory category);
    BatchPrintRun run(const QVector<BatchPrintablePart>& parts,const BatchPrintOptions& options,
                      CancellationState* cancellation = nullptr,const Progress& progress = {}) const;
};

} // namespace PrintGeometry
