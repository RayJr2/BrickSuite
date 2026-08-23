#pragma once
#include "../models/ExternalPartIdentifier.h"
#include <QList>
#include <QHash>
#include <QString>

class ExternalPartIdentifierRepository
{
public:
    bool replaceProviderIds(
        int partId,
        const QHash<QString, QStringList>& externalIds,
        const QString& source) const;

    QList<ExternalPartIdentifier> findByExternalId(
        const QString& externalId,
        bool activeOnly = true) const;
};
