#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class PartImageService : public QObject
{
    Q_OBJECT

public:
    explicit PartImageService(
        QObject* parent = nullptr);

    void requestPartImage(
        const QString& partNumber,
        const QString& imageUrl);

    QString cachedImagePath(
        const QString& partNumber) const;

    bool hasCachedImage(
        const QString& partNumber) const;

    void requestPartColorImage(const QString& partNumber,
                               int rebrickableColorId,
                               const QString& imageUrl);

    QString cachedPartColorImagePath(const QString& partNumber, int rebrickableColorId) const;

    bool hasCachedPartColorImage(const QString& partNumber, int rebrickableColorId) const;

signals:
    void imageReady(
        const QString& partNumber,
        const QString& imagePath);

    void imageFailed(
        const QString& partNumber,
        const QString& message);

    void partColorImageReady(const QString& partNumber,
                             int rebrickableColorId,
                             const QString& imagePath);

    void partColorImageFailed(const QString& partNumber,
                              int rebrickableColorId,
                              const QString& message);

private:
    QString cacheDirectory() const;

    QString cacheFilePath(
        const QString& partNumber,
        const QString& imageUrl = QString()) const;

    QString safePartNumber(
        const QString& partNumber) const;

    void downloadImage(
        const QString& partNumber,
        const QString& imageUrl);

    QString colorCacheDirectory() const;

    QString colorCacheFilePath(const QString& partNumber,
                               int rebrickableColorId,
                               const QString& imageUrl = QString()) const;

    QString partColorKey(const QString& partNumber, int rebrickableColorId) const;

    void downloadPartColorImage(const QString& partNumber,
                                int rebrickableColorId,
                                const QString& imageUrl);

    QNetworkAccessManager* m_networkManager = nullptr;

    QSet<QString> m_pendingPartNumbers;

    QHash<QString, QString> m_cachedPaths;

    QSet<QString> m_pendingPartColors;

    QHash<QString, QString> m_cachedColorPaths;
};