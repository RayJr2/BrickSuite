#pragma once

#include <QList>
#include <QSqlDatabase>
#include <QString>

struct PartElementIdentifier
{
    int id = 0;
    QString provider;
    QString elementId;
    int partId = 0;
    int colorId = 0;
    QString designId;
    bool active = false;
};

class PartElementIdentifierRepository
{
public:
    explicit PartElementIdentifierRepository(QSqlDatabase database = QSqlDatabase());
    PartElementIdentifier findByElementId(const QString& provider,
                                          const QString& elementId) const;
    QList<PartElementIdentifier> findByPartColor(int partId, int colorId,
                                                 const QString& provider = {}) const;

private:
    QSqlDatabase m_database;
};
