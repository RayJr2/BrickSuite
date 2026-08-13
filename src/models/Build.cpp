#include "Build.h"

int Build::id() const
{
    return m_id;
}

void Build::setId(int id)
{
    m_id = id;
}

int Build::workspaceId() const
{
    return m_workspaceId;
}

void Build::setWorkspaceId(int workspaceId)
{
    m_workspaceId = workspaceId;
}

QString Build::buildType() const
{
    return m_buildType;
}

void Build::setBuildType(const QString& buildType)
{
    m_buildType = buildType.trimmed();
}

QString Build::name() const
{
    return m_name;
}

void Build::setName(const QString& name)
{
    m_name = name.trimmed();
}

QString Build::setNumber() const
{
    return m_setNumber;
}

void Build::setSetNumber(const QString& setNumber)
{
    m_setNumber = setNumber.trimmed();
}

QString Build::inventoryMode() const
{
    return m_inventoryMode;
}

void Build::setInventoryMode(const QString& inventoryMode)
{
    m_inventoryMode = inventoryMode.trimmed();
}

QString Build::status() const
{
    return m_status;
}

void Build::setStatus(const QString& status)
{
    m_status = status.trimmed();
}

QString Build::notes() const
{
    return m_notes;
}

void Build::setNotes(const QString& notes)
{
    m_notes = notes.trimmed();
}

QDateTime Build::createdUtc() const
{
    return m_createdUtc;
}

void Build::setCreatedUtc(const QDateTime& createdUtc)
{
    m_createdUtc = createdUtc;
}

QDateTime Build::modifiedUtc() const
{
    return m_modifiedUtc;
}

void Build::setModifiedUtc(const QDateTime& modifiedUtc)
{
    m_modifiedUtc = modifiedUtc;
}