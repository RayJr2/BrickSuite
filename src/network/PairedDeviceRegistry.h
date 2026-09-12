#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>
#include <optional>

struct PairedDeviceRecord
{
    QString deviceId;
    QString friendlyName;
    QString firstPairedUtc;
    QString lastSeenUtc;
    QString clientVersion;
    QString platform;
    QString credentialReference;
    bool active = true;
};

class PairedDeviceRegistry
{
public:
    explicit PairedDeviceRegistry(QString path = QString());

    static QString defaultPath();
    bool load(QString* error = nullptr);
    bool add(const PairedDeviceRecord& record, QString* error = nullptr);
    bool updateLastSeen(const QString& deviceId, const QDateTime& utc,
                        QString* error = nullptr);
    bool rename(const QString& deviceId, const QString& friendlyName,
                QString* error = nullptr);
    bool deactivate(const QString& deviceId, QString* error = nullptr);
    bool deactivateAll(QString* error = nullptr);
    bool remove(const QString& deviceId, QString* error = nullptr);
    bool removeAllInactive(QString* error = nullptr);
    std::optional<PairedDeviceRecord> find(const QString& deviceId) const;
    QVector<PairedDeviceRecord> devices() const { return m_devices; }
    QString path() const { return m_path; }
    bool available() const { return m_available; }
    QString lastError() const { return m_lastError; }

private:
    bool save(QString* error);
    QString m_path;
    QVector<PairedDeviceRecord> m_devices;
    bool m_available = true;
    QString m_lastError;
};
