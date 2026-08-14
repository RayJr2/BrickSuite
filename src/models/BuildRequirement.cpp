#include "BuildRequirement.h"

#include <QtGlobal>

int BuildRequirement::id() const
{
    return m_id;
}

void BuildRequirement::setId(int id)
{
    m_id = id;
}

int BuildRequirement::buildId() const
{
    return m_buildId;
}

void BuildRequirement::setBuildId(int buildId)
{
    m_buildId = buildId;
}

int BuildRequirement::partId() const
{
    return m_partId;
}

void BuildRequirement::setPartId(int partId)
{
    m_partId = partId;
}

int BuildRequirement::colorId() const
{
    return m_colorId;
}

void BuildRequirement::setColorId(int colorId)
{
    m_colorId = colorId;
}

int BuildRequirement::quantityRequired() const
{
    return m_quantityRequired;
}

void BuildRequirement::setQuantityRequired(int quantityRequired)
{
    m_quantityRequired = quantityRequired;
}

bool BuildRequirement::isSpare() const
{
    return m_isSpare;
}

void BuildRequirement::setIsSpare(bool isSpare)
{
    m_isSpare = isSpare;
}

QDateTime BuildRequirement::createdUtc() const
{
    return m_createdUtc;
}

void BuildRequirement::setCreatedUtc(const QDateTime& createdUtc)
{
    m_createdUtc = createdUtc;
}

QDateTime BuildRequirement::modifiedUtc() const
{
    return m_modifiedUtc;
}

void BuildRequirement::setModifiedUtc(const QDateTime& modifiedUtc)
{
    m_modifiedUtc = modifiedUtc;
}

int BuildRequirement::quantityPulled() const
{
    return m_quantityPulled;
}

void BuildRequirement::setQuantityPulled(int quantityPulled)
{
    m_quantityPulled = qMax(quantityPulled, 0);
}