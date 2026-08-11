#pragma once

class QSqlDatabase;

class ReferenceDataSeeder
{
public:
    explicit ReferenceDataSeeder(
        QSqlDatabase& database);

    bool seedIfRequired();

private:
    bool getRecordCount(
        const char* tableName,
        int& count) const;

    bool seedReferenceData();

    QSqlDatabase& m_database;
};