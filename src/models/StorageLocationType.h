#pragma once

#include <QString>

class StorageLocationType
{
public:
    StorageLocationType() = default;

    int id() const;
    void setId(int id);

    QString name() const;
    void setName(const QString& name);

    QString description() const;
    void setDescription(const QString& description);

    bool isSystem() const;
    void setIsSystem(bool isSystem);

    bool isActive() const;
    void setIsActive(bool isActive);

    int sortOrder() const;
    void setSortOrder(int sortOrder);

private:
    int m_id = 0;
    QString m_name;
    QString m_description;
    bool m_isSystem = false;
    bool m_isActive = true;
    int m_sortOrder = 0;
};