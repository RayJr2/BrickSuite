// DatabaseManager.h

#pragma once

#include <QSqlDatabase>
#include <QString>

class DatabaseManager
{
public:
    static DatabaseManager& instance();

    bool initialize();
    void close();

    QString databasePath() const;
    QSqlDatabase database() const;

    bool backupDatabase(const QString& backupPath, QString* errorMessage = nullptr);
    bool verifyDatabaseBackup(const QString& backupPath, QString* errorMessage = nullptr) const;
    bool restoreDatabase(const QString& backupPath, QString* errorMessage = nullptr);

private:
    DatabaseManager();
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    QSqlDatabase m_database;
};