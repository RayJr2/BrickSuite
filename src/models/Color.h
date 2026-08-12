#pragma once

#include <QDateTime>
#include <QString>

class Color
{
public:
    Color() = default;

    int id() const;
    void setId(int id);

    QString name() const;
    void setName(const QString& name);

    QString rgb() const;
    void setRgb(const QString& rgb);

    bool isTransparent() const;
    void setIsTransparent(bool isTransparent);

    int rebrickableId() const;
    void setRebrickableId(int rebrickableId);

    QDateTime createdUtc() const;
    void setCreatedUtc(const QDateTime& createdUtc);

    QDateTime modifiedUtc() const;
    void setModifiedUtc(const QDateTime& modifiedUtc);

private:
    int m_id = 0;
    QString m_name;
    QString m_rgb;
    bool m_isTransparent = false;
    int m_rebrickableId = 0;
    QDateTime m_createdUtc;
    QDateTime m_modifiedUtc;
};