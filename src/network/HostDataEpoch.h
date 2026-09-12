#pragma once

#include <QString>

// Installation-local operational database lineage. Stored outside BrickSuite.db,
// independent of TLS identity, and deliberately not treated as a secret.
class HostDataEpoch
{
public:
    struct LoadResult {
        bool success = false;
        bool bootstrapped = false;
        QString epoch;
        QString error;
    };

    static QString storageDirectory();
    static LoadResult loadOrBootstrap(const QString& directory = QString());
    static LoadResult loadExisting(const QString& directory = QString());
    static QString current();
    static QString create();
    static bool persist(const QString& epoch, QString* error = nullptr,
                        const QString& directory = QString());
    static bool isValid(const QString& epoch);

private:
    static LoadResult load(const QString& directory, bool permitBootstrap);
};
