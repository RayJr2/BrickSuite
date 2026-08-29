/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 */

#include "PartReferenceManifest.h"

#include <QFile>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <QDebug>

#include <algorithm>

namespace {

constexpr auto kManifestResourcePath = ":/data/part_reference_manifest.csv";

QVector<QStringList> parseCsv(const QString& text, QString* errorMessage)
{
    QVector<QStringList> records;
    QStringList record;
    QString field;
    bool quoted = false;

    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);

        if (quoted) {
            if (ch == QLatin1Char('"')) {
                if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('"')) {
                    field.append(QLatin1Char('"'));
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                field.append(ch);
            }
            continue;
        }

        if (ch == QLatin1Char('"')) {
            if (!field.isEmpty()) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("Unexpected quote in CSV field.");
                return {};
            }
            quoted = true;
        } else if (ch == QLatin1Char(',')) {
            record.append(field);
            field.clear();
        } else if (ch == QLatin1Char('\n')) {
            record.append(field);
            field.clear();
            records.append(record);
            record.clear();
        } else if (ch != QLatin1Char('\r')) {
            field.append(ch);
        }
    }

    if (quoted) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Unterminated quoted CSV field.");
        return {};
    }

    if (!field.isEmpty() || !record.isEmpty()) {
        record.append(field);
        records.append(record);
    }

    return records;
}

} // namespace

bool PartReferenceManifest::load(QString* errorMessage)
{
    clear();

    QFile file(QString::fromLatin1(kManifestResourcePath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString message = QStringLiteral("Unable to open embedded Part Reference manifest: %1")
                                    .arg(file.errorString());
        qWarning().noquote() << message;
        if (errorMessage)
            *errorMessage = message;
        return false;
    }

    QString parseError;
    const QVector<QStringList> records = parseCsv(QString::fromUtf8(file.readAll()), &parseError);
    if (records.isEmpty()) {
        const QString message = parseError.isEmpty()
                                    ? QStringLiteral("Embedded Part Reference manifest is empty.")
                                    : QStringLiteral("Unable to parse Part Reference manifest: %1")
                                          .arg(parseError);
        qWarning().noquote() << message;
        if (errorMessage)
            *errorMessage = message;
        return false;
    }

    const QStringList headers = records.first();
    QHash<QString, int> column;
    for (int i = 0; i < headers.size(); ++i) {
        QString key = headers.at(i).trimmed().toLower();
        if (i == 0 && !key.isEmpty() && key.front() == QChar(0xFEFF))
            key.remove(0, 1);
        column.insert(key, i);
    }

    const QStringList requiredColumns = {
        QStringLiteral("part_num"),
        QStringLiteral("part_name"),
        QStringLiteral("catalog"),
        QStringLiteral("section"),
        QStringLiteral("display_order"),
        QStringLiteral("source_category_id"),
        QStringLiteral("source_category"),
        QStringLiteral("material"),
        QStringLiteral("representative_for"),
        QStringLiteral("notes")
    };

    for (const QString& required : requiredColumns) {
        if (!column.contains(required)) {
            const QString message = QStringLiteral("Part Reference manifest is missing required column '%1'.")
                                        .arg(required);
            qWarning().noquote() << message;
            if (errorMessage)
                *errorMessage = message;
            return false;
        }
    }

    auto valueAt = [&column](const QStringList& fields, const QString& name) -> QString {
        const int index = column.value(name, -1);
        return (index >= 0 && index < fields.size()) ? fields.at(index).trimmed() : QString();
    };

    QSet<QString> seenCatalogs;

    for (int recordIndex = 1; recordIndex < records.size(); ++recordIndex) {
        const QStringList& fields = records.at(recordIndex);

        bool allBlank = true;
        for (const QString& field : fields) {
            if (!field.trimmed().isEmpty()) {
                allBlank = false;
                break;
            }
        }
        if (allBlank)
            continue;

        PartReferenceEntry entry;
        entry.partNumber = valueAt(fields, QStringLiteral("part_num"));
        entry.partName = valueAt(fields, QStringLiteral("part_name"));
        entry.catalog = valueAt(fields, QStringLiteral("catalog"));
        entry.section = valueAt(fields, QStringLiteral("section"));
        entry.sourceCategory = valueAt(fields, QStringLiteral("source_category"));
        entry.material = valueAt(fields, QStringLiteral("material"));
        entry.representativeFor = valueAt(fields, QStringLiteral("representative_for"));
        entry.notes = valueAt(fields, QStringLiteral("notes"));

        bool displayOrderOk = false;
        entry.displayOrder = valueAt(fields, QStringLiteral("display_order")).toInt(&displayOrderOk);
        bool sourceCategoryIdOk = false;
        entry.sourceCategoryId = valueAt(fields, QStringLiteral("source_category_id")).toInt(&sourceCategoryIdOk);

        const int csvLine = recordIndex + 1;
        if (entry.partNumber.isEmpty() || entry.partName.isEmpty() || entry.catalog.isEmpty()
            || entry.section.isEmpty() || !displayOrderOk || entry.displayOrder <= 0
            || !sourceCategoryIdOk || entry.sourceCategoryId <= 0) {
            const QString message = QStringLiteral("Invalid Part Reference manifest row at CSV line %1.")
                                        .arg(csvLine);
            qWarning().noquote() << message;
            if (errorMessage)
                *errorMessage = message;
            clear();
            return false;
        }

        const QString partKey = normalizedKey(entry.partNumber);
        if (m_entryIndexByPartNumber.contains(partKey)) {
            const QString message = QStringLiteral("Duplicate Part Reference part number '%1' at CSV line %2.")
                                        .arg(entry.partNumber)
                                        .arg(csvLine);
            qWarning().noquote() << message;
            if (errorMessage)
                *errorMessage = message;
            clear();
            return false;
        }

        if (!seenCatalogs.contains(entry.catalog)) {
            seenCatalogs.insert(entry.catalog);
            m_catalogs.append(entry.catalog);
        }

        m_entryIndexByPartNumber.insert(partKey, m_entries.size());
        m_entries.append(entry);
    }

    if (m_entries.size() != ExpectedEntryCount) {
        const QString message = QStringLiteral("Part Reference manifest count mismatch: expected %1 entries, loaded %2.")
                                    .arg(ExpectedEntryCount)
                                    .arg(m_entries.size());
        qWarning().noquote() << message;
        if (errorMessage)
            *errorMessage = message;
        clear();
        return false;
    }

    if (m_catalogs.size() != 37) {
        const QString message = QStringLiteral("Part Reference manifest catalog count mismatch: expected 37 catalogs, loaded %1.")
                                    .arg(m_catalogs.size());
        qWarning().noquote() << message;
        if (errorMessage)
            *errorMessage = message;
        clear();
        return false;
    }

    m_loaded = true;
    qInfo() << "Part Reference manifest loaded:"
            << m_entries.size() << "entries across"
            << m_catalogs.size() << "catalogs.";
    return true;
}

bool PartReferenceManifest::isLoaded() const
{
    return m_loaded;
}

int PartReferenceManifest::entryCount() const
{
    return m_entries.size();
}

const QList<PartReferenceEntry>& PartReferenceManifest::entries() const
{
    return m_entries;
}

QStringList PartReferenceManifest::catalogs() const
{
    return m_catalogs;
}

QList<PartReferenceEntry> PartReferenceManifest::entriesForCatalog(const QString& catalog) const
{
    QList<PartReferenceEntry> result;
    const QString wanted = normalizedKey(catalog);

    for (const PartReferenceEntry& entry : m_entries) {
        if (normalizedKey(entry.catalog) == wanted)
            result.append(entry);
    }

    return result;
}

QList<PartReferenceEntry> PartReferenceManifest::search(const QString& text) const
{
    QList<PartReferenceEntry> result;
    const QString query = normalizedKey(text);
    if (query.isEmpty())
        return result;

    for (const PartReferenceEntry& entry : m_entries) {
        if (normalizedKey(entry.partNumber).contains(query)
            || normalizedKey(entry.partName).contains(query)
            || normalizedKey(entry.catalog).contains(query)
            || normalizedKey(entry.section).contains(query)) {
            result.append(entry);
        }
    }

    return result;
}

const PartReferenceEntry* PartReferenceManifest::findByPartNumber(const QString& partNumber) const
{
    const auto it = m_entryIndexByPartNumber.constFind(normalizedKey(partNumber));
    if (it == m_entryIndexByPartNumber.cend())
        return nullptr;

    return &m_entries.at(it.value());
}

void PartReferenceManifest::clear()
{
    m_entries.clear();
    m_catalogs.clear();
    m_entryIndexByPartNumber.clear();
    m_loaded = false;
}

QString PartReferenceManifest::normalizedKey(const QString& value)
{
    return value.trimmed().toLower();
}
