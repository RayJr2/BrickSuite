#include "RebrickableReferenceImporter.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

RebrickableReferenceImporter::RebrickableReferenceImporter(
    QSqlDatabase& database)
    : m_database(database)
{
}

QStringList RebrickableReferenceImporter::parseCsvLine(
    const QString& line) const
{
    QStringList fields;
    QString currentField;

    bool insideQuotes = false;

    for (qsizetype i = 0; i < line.length(); ++i)
    {
        const QChar character = line.at(i);

        if (character == '"')
        {
            // Two consecutive quotation marks inside a quoted
            // value represent one literal quotation mark.
            if (insideQuotes &&
                i + 1 < line.length() &&
                line.at(i + 1) == '"')
            {
                currentField.append('"');
                ++i;
            }
            else
            {
                insideQuotes = !insideQuotes;
            }
        }
        else if (character == ',' && !insideQuotes)
        {
            fields.append(currentField);
            currentField.clear();
        }
        else
        {
            currentField.append(character);
        }
    }

    fields.append(currentField);

    return fields;
}

bool RebrickableReferenceImporter::validateHeader(
    const QStringList& actualHeader,
    const QStringList& requiredColumns,
    const QString& fileName) const
{
    for (const QString& requiredColumn : requiredColumns)
    {
        if (!actualHeader.contains(
                requiredColumn,
                Qt::CaseInsensitive))
        {
            qCritical()
                << "Rebrickable import:"
                << fileName
                << "is missing required column:"
                << requiredColumn;

            return false;
        }
    }

    return true;
}

bool RebrickableReferenceImporter::importColors(const QString& filePath,
                                                ImportResult& result,
                                                bool manageTransaction)
{
    result = {};

    QFile file(filePath);

    if (!file.open(
            QIODevice::ReadOnly |
            QIODevice::Text))
    {
        qCritical()
            << "Unable to open Rebrickable colors file:"
            << filePath
            << file.errorString();

        return false;
    }

    QTextStream stream(&file);

    if (stream.atEnd())
    {
        qCritical()
            << "Rebrickable colors file is empty:"
            << filePath;

        return false;
    }

    const QStringList header =
        parseCsvLine(stream.readLine());

    const QStringList requiredColumns =
    {
        "id",
        "name",
        "rgb",
        "is_trans"
    };

    if (!validateHeader(
            header,
            requiredColumns,
            QFileInfo(filePath).fileName()))
    {
        return false;
    }

    const int idIndex =
        header.indexOf("id");

    const int nameIndex =
        header.indexOf("name");

    const int rgbIndex =
        header.indexOf("rgb");

    const int transparentIndex =
        header.indexOf("is_trans");

    if (manageTransaction) {
        if (!m_database.transaction()) {
            qCritical() << "Unable to begin color import transaction:"
                        << m_database.lastError().text();

            return false;
        }
    }

    QSqlQuery query(m_database);

    if (!query.prepare(R"(
        INSERT INTO color
        (
            name,
            rgb,
            is_transparent,
            rebrickable_id,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :name,
            :rgb,
            :is_transparent,
            :rebrickable_id,
            :created_utc,
            :modified_utc
        )

        ON CONFLICT(rebrickable_id)
        DO UPDATE SET
            name = excluded.name,
            rgb = excluded.rgb,
            is_transparent = excluded.is_transparent,
            modified_utc = excluded.modified_utc
    )"))
    {
        qCritical()
            << "Unable to prepare color import:"
            << query.lastError().text();

        if (manageTransaction)
            m_database.rollback();
        return false;
    }

    while (!stream.atEnd())
    {
        const QString line =
            stream.readLine();

        if (line.trimmed().isEmpty())
            continue;

        ++result.recordsProcessed;

        const QStringList fields =
            parseCsvLine(line);

        if (fields.size() != header.size())
        {
            ++result.recordsFailed;

            qWarning()
                << "Invalid Rebrickable color row:"
                << result.recordsProcessed;

            continue;
        }

        bool idValid = false;

        const int rebrickableId =
            fields.at(idIndex).toInt(&idValid);

        if (!idValid)
        {
            ++result.recordsFailed;

            qWarning()
                << "Invalid Rebrickable color ID:"
                << fields.at(idIndex);

            continue;
        }

        const QString name =
            fields.at(nameIndex).trimmed();

        const QString rgb =
            fields.at(rgbIndex).trimmed();

        const QString transparentValue =
            fields.at(transparentIndex)
                .trimmed()
                .toLower();

        const bool isTransparent =
            transparentValue == "true" ||
            transparentValue == "1";

        if (name.isEmpty())
        {
            ++result.recordsFailed;
            continue;
        }

        const QString now =
            QDateTime::currentDateTimeUtc()
                .toString(Qt::ISODateWithMs);

        query.bindValue(
            ":name",
            name);

        query.bindValue(
            ":rgb",
            rgb);

        query.bindValue(
            ":is_transparent",
            isTransparent ? 1 : 0);

        query.bindValue(
            ":rebrickable_id",
            rebrickableId);

        query.bindValue(
            ":created_utc",
            now);

        query.bindValue(
            ":modified_utc",
            now);

        if (!query.exec())
        {
            ++result.recordsFailed;

            qWarning()
                << "Unable to import Rebrickable color:"
                << rebrickableId
                << name
                << query.lastError().text();

            continue;
        }

        ++result.recordsImported;
    }

    if (result.recordsFailed > 0)
    {
        qCritical()
            << "Rebrickable color import encountered"
            << result.recordsFailed
            << "failed records.";

        if (manageTransaction)
            m_database.rollback();

        return false;
    }

    if (manageTransaction) {
        if (!m_database.commit()) {
            qCritical() << "Unable to commit Rebrickable color import:"
                        << m_database.lastError().text();

            m_database.rollback();
            return false;
        }
    }

    qInfo() << "Rebrickable colors imported:" << result.recordsImported;

    return true;
}

bool RebrickableReferenceImporter::importPartCategories(const QString& filePath,
                                                        ImportResult& result,
                                                        bool manageTransaction)
{
    result = {};

    QFile file(filePath);

    if (!file.open(
            QIODevice::ReadOnly |
            QIODevice::Text))
    {
        qCritical()
            << "Unable to open Rebrickable part categories file:"
            << filePath
            << file.errorString();

        return false;
    }

    QTextStream stream(&file);

    if (stream.atEnd())
    {
        qCritical()
            << "Rebrickable part categories file is empty:"
            << filePath;

        return false;
    }

    const QStringList header =
        parseCsvLine(stream.readLine());

    const QStringList requiredColumns =
    {
        "id",
        "name"
    };

    if (!validateHeader(
            header,
            requiredColumns,
            QFileInfo(filePath).fileName()))
    {
        return false;
    }

    const int idIndex =
        header.indexOf("id");

    const int nameIndex =
        header.indexOf("name");

    if (manageTransaction) {
        if (!m_database.transaction()) {
            qCritical() << "Unable to begin color import transaction:"
                        << m_database.lastError().text();

            return false;
        }
    }

    QSqlQuery query(m_database);

    if (!query.prepare(R"(
        INSERT INTO part_category
        (
            name,
            rebrickable_id,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :name,
            :rebrickable_id,
            :created_utc,
            :modified_utc
        )

        ON CONFLICT(rebrickable_id)
        DO UPDATE SET
            name = excluded.name,
            modified_utc = excluded.modified_utc
    )"))
    {
        qCritical()
            << "Unable to prepare category import:"
            << query.lastError().text();

        if (manageTransaction)
            m_database.rollback();
        return false;
    }

    while (!stream.atEnd())
    {
        const QString line =
            stream.readLine();

        if (line.trimmed().isEmpty())
            continue;

        ++result.recordsProcessed;

        const QStringList fields =
            parseCsvLine(line);

        if (fields.size() != header.size())
        {
            ++result.recordsFailed;

            qWarning()
                << "Invalid Rebrickable category row:"
                << result.recordsProcessed;

            continue;
        }

        bool idValid = false;

        const int rebrickableId =
            fields.at(idIndex).toInt(&idValid);

        if (!idValid)
        {
            ++result.recordsFailed;
            continue;
        }

        const QString name =
            fields.at(nameIndex).trimmed();

        if (name.isEmpty())
        {
            ++result.recordsFailed;
            continue;
        }

        const QString now =
            QDateTime::currentDateTimeUtc()
                .toString(Qt::ISODateWithMs);

        query.bindValue(
            ":name",
            name);

        query.bindValue(
            ":rebrickable_id",
            rebrickableId);

        query.bindValue(
            ":created_utc",
            now);

        query.bindValue(
            ":modified_utc",
            now);

        if (!query.exec())
        {
            ++result.recordsFailed;

            qWarning()
                << "Unable to import Rebrickable category:"
                << rebrickableId
                << name
                << query.lastError().text();

            continue;
        }

        ++result.recordsImported;
    }

    if (result.recordsFailed > 0)
    {
        qCritical()
            << "Rebrickable category import encountered"
            << result.recordsFailed
            << "failed records.";

        if (manageTransaction)
            m_database.rollback();

        return false;
    }

    if (manageTransaction) {
        if (!m_database.commit()) {
            qCritical() << "Unable to commit Rebrickable color import:"
                        << m_database.lastError().text();

            m_database.rollback();
            return false;
        }
    }

    qInfo()
        << "Rebrickable part categories imported:"
        << result.recordsImported;

    return true;
}