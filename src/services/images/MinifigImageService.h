#pragma once

#include <QHash>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>

class QNetworkAccessManager;

class MinifigImageService : public QObject
{
    Q_OBJECT

public:
    explicit MinifigImageService(QObject* parent = nullptr);

    void requestMinifigImage(const QString& minifigNumber, const QString& imageUrl);
    void clearQueuedRequests();
    QString cachedImagePath(const QString& minifigNumber) const;
    bool isImageKnownUnavailable(const QString& minifigNumber,
                                 const QString& imageUrl) const;

signals:
    void imageReady(const QString& minifigNumber, const QString& imagePath);
    void imageFailed(const QString& minifigNumber, const QString& message);

private:
    struct Request
    {
        QString minifigNumber;
        QString imageUrl;
    };

    QString cacheDirectory() const;
    QString cacheFilePath(const QString& minifigNumber,
                          const QString& imageUrl = QString()) const;
    QString cacheKey(const QString& minifigNumber) const;
    QString requestKey(const QString& minifigNumber) const;
    void startQueuedRequests();
    void downloadImage(const Request& request);

    static constexpr int MaximumConcurrentDownloads = 4;

    QNetworkAccessManager* m_networkManager = nullptr;
    QQueue<Request> m_queue;
    QSet<QString> m_pendingKeys;
    QHash<QString, QString> m_cachedPaths;
    int m_activeDownloads = 0;
};
