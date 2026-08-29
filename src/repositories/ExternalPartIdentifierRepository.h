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

    QList<ExternalPartIdentifier> findByProviderAndExternalId(
        const QString& provider,
        const QString& externalId,
        bool activeOnly = true) const;

    // Background enrichment status is tracked separately from identifier
    // rows because a successful provider lookup may legitimately return no
    // external IDs. Without a terminal status BrickSuite would repeatedly
    // request the same part forever.
    bool isLookupComplete(int partId, const QString& source) const;
    bool setLookupStatus(int partId,
                         const QString& source,
                         const QString& status) const;
};
