/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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
