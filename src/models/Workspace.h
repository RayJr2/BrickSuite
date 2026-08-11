#pragma once

#include <QString>
#include <QDateTime>

class Workspace
{
public:
    Workspace() = default;

    int id() const;
    void setId(int id);

    QString name() const;
    void setName(const QString& name);

    QString description() const;
    void setDescription(const QString& description);

    QDateTime createdUtc() const;
    void setCreatedUtc(const QDateTime& createdUtc);

    QDateTime modifiedUtc() const;
    void setModifiedUtc(const QDateTime& modifiedUtc);

    bool isActive() const;
    void setIsActive(bool isActive);

private:
    int m_id = 0;
    QString m_name;
    QString m_description;
    QDateTime m_createdUtc;
    QDateTime m_modifiedUtc;
    bool m_isActive = true;
};