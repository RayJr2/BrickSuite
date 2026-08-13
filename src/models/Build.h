#pragma once

#include <QDateTime>
#include <QString>

class Build
{
public:
    Build() = default;

    int id() const;
    void setId(int id);

    int workspaceId() const;
    void setWorkspaceId(int workspaceId);

    QString buildType() const;
    void setBuildType(const QString& buildType);

    QString name() const;
    void setName(const QString& name);

    QString setNumber() const;
    void setSetNumber(const QString& setNumber);

    QString inventoryMode() const;
    void setInventoryMode(const QString& inventoryMode);

    QString status() const;
    void setStatus(const QString& status);

    QString notes() const;
    void setNotes(const QString& notes);

    QDateTime createdUtc() const;
    void setCreatedUtc(const QDateTime& createdUtc);

    QDateTime modifiedUtc() const;
    void setModifiedUtc(const QDateTime& modifiedUtc);

private:
    int m_id = 0;
    int m_workspaceId = 0;

    QString m_buildType = "Set";
    QString m_name;
    QString m_setNumber;
    QString m_inventoryMode = "Stock";
    QString m_status = "Planned";
    QString m_notes;

    QDateTime m_createdUtc;
    QDateTime m_modifiedUtc;
};
