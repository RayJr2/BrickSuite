#pragma once

#include <functional>

#include <QString>

class QSqlDatabase;

class RebrickableWorkerDatabaseSession
{
public:
    using Work = std::function<bool(QSqlDatabase&, QString&)>;

    // Call from the worker thread. The connection is created, used, closed,
    // and removed in that same thread; no QSqlDatabase crosses thread boundaries.
    static bool execute(const QString& databasePath, const Work& work, QString& errorMessage);
};

