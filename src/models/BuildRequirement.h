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

    int substitutePartId() const;
    void setSubstitutePartId(int partId);

    int substituteColorId() const;
    void setSubstituteColorId(int colorId);

    int effectivePartId() const;
    int effectiveColorId() const;

    int quantityRequired() const;
    void setQuantityRequired(int quantityRequired);

    int quantityPulled() const;
    void setQuantityPulled(int quantityPulled);

    int quantityReleased() const;
    void setQuantityReleased(int quantityReleased);

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
    int m_substitutePartId = 0;
    int m_substituteColorId = 0;
    int m_quantityRequired = 0;
    int m_quantityPulled = 0;
    int m_quantityReleased = 0;
    bool m_isSpare = false;
    QDateTime m_createdUtc;
    QDateTime m_modifiedUtc;
};
