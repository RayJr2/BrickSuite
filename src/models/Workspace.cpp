#include "Workspace.h"

int Workspace::id() const
{
    return m_id;
}

void Workspace::setId(int id)
{
    m_id = id;
}

QString Workspace::name() const
{
    return m_name;
}

void Workspace::setName(const QString& name)
{
    m_name = name;
}

QString Workspace::description() const
{
    return m_description;
}

void Workspace::setDescription(const QString& description)
{
    m_description = description;
}

QDateTime Workspace::createdUtc() const
{
    return m_createdUtc;
}

void Workspace::setCreatedUtc(const QDateTime& createdUtc)
{
    m_createdUtc = createdUtc;
}

QDateTime Workspace::modifiedUtc() const
{
    return m_modifiedUtc;
}

void Workspace::setModifiedUtc(const QDateTime& modifiedUtc)
{
    m_modifiedUtc = modifiedUtc;
}

bool Workspace::isActive() const
{
    return m_isActive;
}

void Workspace::setIsActive(bool isActive)
{
    m_isActive = isActive;
}