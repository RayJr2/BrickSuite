#include "ImageUnavailableCache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

QString ImageUnavailableCache::markerPath(const QString& cacheDirectory,
                                          const QString& providerIdentity,
                                          const QString& imageUrl)
{
    QByteArray key = providerIdentity.trimmed().toUtf8();
    key.append('\0');
    key.append(imageUrl.trimmed().toUtf8());
    const QString fileName = QString::fromLatin1(
                                 QCryptographicHash::hash(key, QCryptographicHash::Sha256)
                                     .toHex())
                             + QStringLiteral(".unavailable");
    return QDir(cacheDirectory).filePath(QStringLiteral("unavailable/%1").arg(fileName));
}

bool ImageUnavailableCache::isKnownUnavailable(const QString& cacheDirectory,
                                                const QString& providerIdentity,
                                                const QString& imageUrl)
{
    return QFile::exists(markerPath(cacheDirectory, providerIdentity, imageUrl));
}

ImageUnavailableCache::MarkResult ImageUnavailableCache::markUnavailable(
    const QString& cacheDirectory,
    const QString& providerIdentity,
    const QString& imageUrl)
{
    const QString path = markerPath(cacheDirectory, providerIdentity, imageUrl);
    if (QFile::exists(path))
        return MarkResult::AlreadyKnown;
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return MarkResult::Failed;

    QSaveFile file(path);
    const QByteArray contents("unavailable\n");
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size()
        || !file.commit()) {
        file.cancelWriting();
        return MarkResult::Failed;
    }
    return MarkResult::Created;
}
