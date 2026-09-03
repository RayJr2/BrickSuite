#pragma once

#include <functional>
#include <QString>
#include <QStringList>

struct AutomaticBackupRetentionResult
{
    bool success = true;
    QStringList removedFiles;
    QString errorMessage;
};

class AutomaticBackupRetention
{
public:
    using RemoveFile = std::function<bool(const QString&)>;

    static AutomaticBackupRetentionResult clean(const QString& versionDirectory,
                                                int currentSchemaVersion,
                                                int retentionCount,
                                                const QString& protectedBackupPath,
                                                const RemoveFile& removeFile = {});
};
