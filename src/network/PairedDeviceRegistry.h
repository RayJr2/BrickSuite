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
    std::optional<PairedDeviceRecord> find(const QString& deviceId) const;
    QVector<PairedDeviceRecord> devices() const { return m_devices; }
    QString path() const { return m_path; }

private:
    bool save(QString* error);
    QString m_path;
    QVector<PairedDeviceRecord> m_devices;
};
