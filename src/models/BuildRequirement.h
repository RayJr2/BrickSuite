#pragma once

#include <QDateTime>

class BuildRequirement
{
public:
    BuildRequirement() = default;

    int id() const;
    void setId(int id);

    int buildId() const;
    void setBuildId(int buildId);

    int partId() const;
    void setPartId(int partId);

    int colorId() const;
    void setColorId(int colorId);

    int quantityRequired() const;
    void setQuantityRequired(int quantityRequired);

    bool isSpare() const;
    void setIsSpare(bool isSpare);

    QDateTime createdUtc() const;
    void setCreatedUtc(const QDateTime& createdUtc);

    QDateTime modifiedUtc() const;
    void setModifiedUtc(const QDateTime& modifiedUtc);

private:
    int m_id = 0;
    int m_buildId = 0;
    int m_partId = 0;
    int m_colorId = 0;
    int m_quantityRequired = 0;

    bool m_isSpare = false;

    QDateTime m_createdUtc;
    QDateTime m_modifiedUtc;
};