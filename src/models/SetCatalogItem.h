#pragma once

#include <QDateTime>
#include <QString>

class SetCatalogItem
{
public:
    int id() const;
    void setId(int id);

    QString setNumber() const;
    void setSetNumber(const QString& setNumber);

    QString name() const;
    void setName(const QString& name);

    int year() const;
    void setYear(int year);

    int themeId() const;
    void setThemeId(int themeId);

    int numberOfParts() const;
    void setNumberOfParts(int numberOfParts);

    QString imageUrl() const;
    void setImageUrl(const QString& imageUrl);

    QDateTime createdUtc() const;
    void setCreatedUtc(const QDateTime& createdUtc);

    QDateTime modifiedUtc() const;
    void setModifiedUtc(const QDateTime& modifiedUtc);

private:
    int m_id = 0;

    QString m_setNumber;
    QString m_name;

    int m_year = 0;
    int m_themeId = 0;
    int m_numberOfParts = 0;

    QString m_imageUrl;

    QDateTime m_createdUtc;
    QDateTime m_modifiedUtc;
};