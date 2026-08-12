#pragma once

#include <QDateTime>
#include <QString>

class PartCategory
{
public:
    PartCategory() = default;

    int id() const;
    void setId(int id);

    QString name() const;
    void setName(const QString& name);

    int rebrickableId() const;
    void setRebrickableId(int rebrickableId);

    QDateTime createdUtc() const;
    void setCreatedUtc(const QDateTime& createdUtc);

    QDateTime modifiedUtc() const;
    void setModifiedUtc(const QDateTime& modifiedUtc);

private:
    int m_id = 0;
    QString m_name;
    int m_rebrickableId = 0;
    QDateTime m_createdUtc;
    QDateTime m_modifiedUtc;
};
