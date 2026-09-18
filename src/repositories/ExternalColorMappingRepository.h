/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "../models/ExternalColorMapping.h"
#include "RepositoryConnection.h"

#include <optional>
#include <QList>
#include <QString>

class ExternalColorMappingRepository : protected RepositoryConnection
{
public:
    ExternalColorMappingRepository() = default;
    explicit ExternalColorMappingRepository(const QSqlDatabase& database)
        : RepositoryConnection(database) {}
    std::optional<ExternalColorMapping> getByColorAndProvider(
        int colorId,
        const QString& provider) const;

    QList<ExternalColorMapping> getByProvider(const QString& provider) const;

    QList<ExternalColorMapping> findByProviderAndExternalId(
        const QString& provider,
        const QString& externalId) const;

    int countByProviderAndStatus(const QString& provider,
                                 ExternalMappingStatus status) const;

    bool upsert(const ExternalColorMapping& mapping) const;
};
