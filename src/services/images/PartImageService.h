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

signals:
    void imageReady(
        const QString& partNumber,
        const QString& imagePath);

    void imageFailed(
        const QString& partNumber,
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

    QNetworkAccessManager* m_networkManager = nullptr;

    QSet<QString> m_pendingPartNumbers;

    QHash<QString, QString> m_cachedPaths;
};