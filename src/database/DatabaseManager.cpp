// DatabaseManager.cpp

#include "DatabaseManager.h"
#include "DatabaseSchema.h"

#include <QDebug>
#include <QDir>
#include <QSqlError>
#include <QStandardPaths>
#include <qsqlquery.h>

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager() = default;

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::initialize()
{
    const QString dataPath =
        QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);

    QDir directory;

    if (!directory.mkpath(dataPath))
    {
        qCritical() << "Unable to create application data directory:"
                    << dataPath;

        return false;
    }

    const QString databasePath =
        dataPath + QDir::separator() + "BrickSuite.db";

    m_database =
        QSqlDatabase::addDatabase("QSQLITE");

    m_database.setDatabaseName(databasePath);

    if (!m_database.open()) {
        qCritical() << "Unable to open BrickSuite database:" << m_database.lastError().text();

        return false;
    }

    QSqlQuery pragmaQuery(m_database);

    if (!pragmaQuery.exec("PRAGMA foreign_keys = ON")) {
        qCritical() << "Unable to enable SQLite foreign keys:" << pragmaQuery.lastError().text();

        return false;
    }

    qInfo() << "BrickSuite database:" << databasePath;

    if (!DatabaseSchema::initialize(m_database)) {
        qCritical() << "Unable to initialize BrickSuite database schema.";
        return false;
    }

    return true;
}

void DatabaseManager::close()
{
    if (m_database.isOpen())
        m_database.close();
}

QSqlDatabase DatabaseManager::database() const
{
    return m_database;
}