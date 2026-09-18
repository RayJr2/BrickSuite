#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

#include <optional>

struct ElementIdentityResult
{
    bool applicable = false;
    QStringList elementIds;

    QString label() const;
    QString displayText() const;
};

struct ElementPartColorIdentity
{
    int partId = 0;
    int colorId = 0;
};

class ElementIdentityService
{
public:
    explicit ElementIdentityService(QSqlDatabase database = QSqlDatabase());

    ElementIdentityResult forInventory(int partId, int colorId,
                                       int manufacturerId) const;
    QStringList forPartColor(int partId, int colorId) const;
    std::optional<ElementPartColorIdentity> fromElementId(
        const QString& elementId) const;

private:
    QSqlDatabase m_database;
};
