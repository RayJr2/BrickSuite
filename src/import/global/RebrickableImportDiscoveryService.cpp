#include "RebrickableImportDiscoveryService.h"

#include "RebrickableDatasetRegistry.h"
#include "../RebrickableCsvInputResolver.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
QStringList parseCsvLine(const QString& line, bool& ok)
{
    QStringList fields;
    QString field;
    bool quoted = false;
    ok = true;
    for (int index = 0; index < line.size(); ++index) {
        const QChar character = line.at(index);
        if (character == QChar('"')) {
            if (quoted && index + 1 < line.size() && line.at(index + 1) == QChar('"')) {
                field += QChar('"');
                ++index;
            } else {
                quoted = !quoted;
            }
        } else if (character == QChar(',') && !quoted) {
            fields.append(field);
            field.clear();
        } else {
            field += character;
        }
    }
    if (quoted)
        ok = false;
    fields.append(field);
    return fields;
}

RebrickableImportSourceType sourceType(const QString& path)
{
    return path.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)
        ? RebrickableImportSourceType::Zip : RebrickableImportSourceType::Csv;
}
}

RebrickableImportPlan
RebrickableImportDiscoveryService::buildPlan(const QString& directoryPath) const
{
    RebrickableImportPlan plan;
    plan.sourceDirectory = QDir(directoryPath).absolutePath();
    QElapsedTimer timer;
    timer.start();
    qInfo() << "Rebrickable data-file scan started:" << plan.sourceDirectory;

    QDir directory(directoryPath);
    const QFileInfoList files = directory.exists()
        ? directory.entryInfoList(QDir::Files | QDir::NoSymLinks,
                                  QDir::Name | QDir::IgnoreCase)
        : QFileInfoList();

    for (const auto& descriptor : RebrickableDatasetRegistry::datasets()) {
        RebrickableImportPlanEntry entry;
        entry.dataset = descriptor.id;
        entry.displayName = descriptor.displayName;
        const QString csvName = descriptor.csvFileName.toCaseFolded();
        const QString zipName = (descriptor.csvFileName + QStringLiteral(".zip")).toCaseFolded();
        for (const QFileInfo& file : files) {
            const QString name = file.fileName().toCaseFolded();
            if (name == csvName || name == zipName)
                entry.conflictingSourcePaths.append(file.absoluteFilePath());
        }

        if (entry.conflictingSourcePaths.isEmpty()) {
            entry.status = RebrickableImportStatus::Missing;
            entry.message = QStringLiteral("No supported source file was found.");
        } else if (entry.conflictingSourcePaths.size() > 1) {
            entry.status = RebrickableImportStatus::Ambiguous;
            entry.message = QStringLiteral("Multiple CSV/ZIP sources match this dataset.");
            qWarning() << "Ambiguous Rebrickable dataset" << entry.displayName
                       << entry.conflictingSourcePaths;
        } else {
            entry.sourcePath = entry.conflictingSourcePaths.constFirst();
            entry.sourceType = sourceType(entry.sourcePath);
            preflight(entry);
        }
        plan.entries.append(entry);
    }

    applyDependencies(plan);
    int recognized = 0;
    for (const auto& entry : plan.entries)
        if (entry.status != RebrickableImportStatus::Missing)
            ++recognized;
    qInfo() << "Rebrickable data-file scan completed. Recognized:" << recognized
            << "Elapsed ms:" << timer.elapsed();
    return plan;
}

void RebrickableImportDiscoveryService::preflight(RebrickableImportPlanEntry& entry) const
{
    const auto* descriptor = RebrickableDatasetRegistry::descriptor(entry.dataset);
    if (!descriptor)
        return;

    QTemporaryDir temporaryDirectory;
    QString resolvedPath;
    QString error;
    if (!RebrickableCsvInputResolver::resolve(entry.sourcePath, descriptor->csvFileName,
                                              temporaryDirectory, resolvedPath, error)) {
        entry.status = RebrickableImportStatus::Invalid;
        entry.message = error;
        entry.issues.append({RebrickableValidationSeverity::Fatal, error, -1});
        qWarning() << "Invalid Rebrickable source" << entry.sourcePath << error;
        return;
    }

    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        entry.status = RebrickableImportStatus::Invalid;
        entry.message = QStringLiteral("The resolved CSV cannot be read: %1").arg(file.errorString());
        entry.issues.append({RebrickableValidationSeverity::Fatal, entry.message, -1});
        return;
    }
    QTextStream stream(&file);
    if (stream.atEnd()) {
        entry.status = RebrickableImportStatus::Invalid;
        entry.message = QStringLiteral("The CSV is empty.");
        entry.rowTotalHint = 0;
        entry.issues.append({RebrickableValidationSeverity::Fatal, entry.message, -1});
        return;
    }

    QString headerLine = stream.readLine();
    if (!headerLine.isEmpty() && headerLine.front() == QChar(0xfeff))
        headerLine.remove(0, 1);
    bool headerOk = false;
    const QStringList headers = parseCsvLine(headerLine, headerOk);
    QStringList missingHeaders;
    for (const QString& required : descriptor->requiredHeaders)
        if (!headers.contains(required))
            missingHeaders.append(required);
    if (!headerOk || !missingHeaders.isEmpty()
        || (descriptor->exactHeaders && headers != descriptor->requiredHeaders)) {
        entry.status = RebrickableImportStatus::Invalid;
        entry.message = !headerOk
            ? QStringLiteral("The CSV header is malformed.")
            : !missingHeaders.isEmpty()
                ? QStringLiteral("Missing required header(s): %1").arg(missingHeaders.join(", "))
                : QStringLiteral("The CSV header does not match the required exact layout.");
        entry.issues.append({RebrickableValidationSeverity::Fatal, entry.message, 1});
        return;
    }

    bool hasData = false;
    while (!stream.atEnd()) {
        const QString dataLine = stream.readLine();
        if (dataLine.trimmed().isEmpty())
            continue;
        bool rowOk = false;
        const QStringList fields = parseCsvLine(dataLine, rowOk);
        if (!rowOk || fields.size() != headers.size()) {
            entry.status = RebrickableImportStatus::Invalid;
            entry.message = QStringLiteral("The first data row is structurally malformed.");
            entry.issues.append({RebrickableValidationSeverity::Fatal, entry.message, 2});
            return;
        }
        hasData = true;
        break;
    }
    if (!hasData) {
        entry.status = RebrickableImportStatus::Invalid;
        entry.message = QStringLiteral("The CSV contains a header but no data rows.");
        entry.rowTotalHint = 0;
        entry.issues.append({RebrickableValidationSeverity::Fatal, entry.message, -1});
        return;
    }

    entry.status = RebrickableImportStatus::Ready;
    entry.message = QStringLiteral("Header and source structure are valid.");
    entry.rowTotalHint = -1;
}

void RebrickableImportDiscoveryService::applyDependencies(RebrickableImportPlan& plan) const
{
    QHash<RebrickableDatasetId, int> indexes;
    for (int index = 0; index < plan.entries.size(); ++index)
        indexes.insert(plan.entries.at(index).dataset, index);

    for (auto& entry : plan.entries) {
        if (entry.status != RebrickableImportStatus::Ready)
            continue;
        const auto* descriptor = RebrickableDatasetRegistry::descriptor(entry.dataset);
        if (!descriptor)
            continue;

        if (descriptor->requiresSchema33 || !descriptor->importerImplemented) {
            entry.status = RebrickableImportStatus::NotImplemented;
            entry.message = QStringLiteral("Source is valid; persistence is planned for a later M25 phase.");
            continue;
        }

        QStringList unavailable;
        for (const auto dependency : descriptor->hardDependencies) {
            const auto* dependencyDescriptor = RebrickableDatasetRegistry::descriptor(dependency);
            const auto iterator = indexes.constFind(dependency);
            if (iterator == indexes.constEnd()
                || plan.entries.at(iterator.value()).status != RebrickableImportStatus::Ready) {
                unavailable.append(dependencyDescriptor ? dependencyDescriptor->displayName
                                                        : QStringLiteral("Unknown"));
            }
        }
        if (!unavailable.isEmpty()) {
            entry.status = RebrickableImportStatus::BlockedByDependency;
            entry.message = QStringLiteral("Needs a valid current-run dependency: %1. Existing-database sufficiency is not yet safely defined.")
                                .arg(unavailable.join(", "));
            qWarning() << "Rebrickable dataset blocked by dependency" << entry.displayName
                       << unavailable;
        }
    }
    qInfo() << "Rebrickable import plan is ready for review.";
}
