/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "../models/ExternalPartMapping.h"
#include "RepositoryConnection.h"

#include <optional>
#include <QList>
#include <QString>

class ExternalPartMappingRepository : protected RepositoryConnection
{
public:
    ExternalPartMappingRepository() = default;
    explicit ExternalPartMappingRepository(const QSqlDatabase& database)
        : RepositoryConnection(database) {}
    std::optional<ExternalPartMapping> getByPartAndProvider(
        int partId,
        const QString& provider) const;

    QList<ExternalPartMapping> findByProviderAndExternalId(
        const QString& provider,
        const QString& externalId) const;

    bool upsert(const ExternalPartMapping& mapping) const;

    bool remove(int partId, const QString& provider) const;
};
