#pragma once

#include <QString>
#include <QStringList>

class QSqlDatabase;

class RebrickableReferenceImporter
{
public:
    struct ImportResult
    {
        int recordsProcessed = 0;
        int recordsImported = 0;
        int recordsFailed = 0;
    };

    explicit RebrickableReferenceImporter(QSqlDatabase& database);

    bool importColors(const QString& filePath, ImportResult& result, bool manageTransaction = true);

    bool importPartCategories(const QString& filePath,
                              ImportResult& result,
                              bool manageTransaction = true);

private:
    QStringList parseCsvLine(const QString& line) const;

    bool validateHeader(const QStringList& actualHeader,
                        const QStringList& requiredColumns,
                        const QString& fileName) const;

    QSqlDatabase& m_database;
};