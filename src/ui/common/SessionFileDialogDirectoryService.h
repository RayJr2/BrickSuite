#pragma once

#include <QHash>
#include <QString>

enum class FileDialogDirectoryCategory
{
    OpenImport,
    SaveExport
};

class SessionFileDialogDirectoryService
{
public:
    static SessionFileDialogDirectoryService& instance();

    QString rememberedDirectory(FileDialogDirectoryCategory category);
    QString initialDirectory(FileDialogDirectoryCategory category,
                             const QString& fallback = QString());
    QString initialFilePath(FileDialogDirectoryCategory category,
                            const QString& workflowDefaultPath);

    void rememberSelectedFile(FileDialogDirectoryCategory category,
                              const QString& filePath);
    void rememberSelectedDirectory(FileDialogDirectoryCategory category,
                                   const QString& directoryPath);

private:
    QHash<FileDialogDirectoryCategory, QString> m_directories;
};
