#include "SessionFileDialogDirectoryService.h"

#include <QDir>
#include <QFileInfo>

SessionFileDialogDirectoryService& SessionFileDialogDirectoryService::instance()
{
    static SessionFileDialogDirectoryService service;
    return service;
}

QString SessionFileDialogDirectoryService::rememberedDirectory(
    FileDialogDirectoryCategory category)
{
    const QString path = m_directories.value(category);
    const QFileInfo info(path);
    if (path.isEmpty() || !info.exists() || !info.isDir()) {
        m_directories.remove(category);
        return {};
    }
    return QDir::cleanPath(info.absoluteFilePath());
}

QString SessionFileDialogDirectoryService::initialDirectory(
    FileDialogDirectoryCategory category, const QString& fallback)
{
    const QString remembered = rememberedDirectory(category);
    return remembered.isEmpty() ? fallback : remembered;
}

QString SessionFileDialogDirectoryService::initialFilePath(
    FileDialogDirectoryCategory category, const QString& workflowDefaultPath)
{
    const QString remembered = rememberedDirectory(category);
    if (remembered.isEmpty())
        return workflowDefaultPath;

    const QString fileName = QFileInfo(workflowDefaultPath).fileName();
    return fileName.isEmpty() ? remembered : QDir(remembered).filePath(fileName);
}

void SessionFileDialogDirectoryService::rememberSelectedFile(
    FileDialogDirectoryCategory category, const QString& filePath)
{
    if (filePath.isEmpty())
        return;
    rememberSelectedDirectory(category, QFileInfo(filePath).absolutePath());
}

void SessionFileDialogDirectoryService::rememberSelectedDirectory(
    FileDialogDirectoryCategory category, const QString& directoryPath)
{
    if (directoryPath.isEmpty())
        return;
    const QFileInfo info(directoryPath);
    if (info.exists() && info.isDir())
        m_directories.insert(category, QDir::cleanPath(info.absoluteFilePath()));
}
