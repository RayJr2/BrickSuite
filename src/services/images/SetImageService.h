#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

class QNetworkAccessManager;

class SetImageService : public QObject
{
    Q_OBJECT

public:
    explicit SetImageService(QObject* parent = nullptr);

    void requestSetImage(const QString& setNumber, const QString& imageUrl);

    QString cachedImagePath(const QString& setNumber) const;

signals:
    void imageReady(const QString& setNumber, const QString& imagePath);

    void imageFailed(const QString& setNumber, const QString& message);

private:
    QString cacheDirectory() const;

    QString cacheFilePath(const QString& setNumber, const QString& imageUrl = QString()) const;

    QString safeSetNumber(const QString& setNumber) const;

    void downloadImage(const QString& setNumber, const QString& imageUrl);

    QNetworkAccessManager* m_networkManager = nullptr;

    QSet<QString> m_pendingSetNumbers;

    QHash<QString, QString> m_cachedPaths;
};
