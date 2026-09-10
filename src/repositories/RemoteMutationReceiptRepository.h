#pragma once

#include "RepositoryConnection.h"
#include "../models/RemoteMutationReceipt.h"

#include <optional>

class RemoteMutationReceiptRepository : protected RepositoryConnection
{
public:
    explicit RemoteMutationReceiptRepository(const QSqlDatabase& database)
        : RepositoryConnection(database) {}

    std::optional<RemoteMutationReceipt> find(const QString& mutationId,
                                               QString* error = nullptr) const;
    bool insert(const RemoteMutationReceipt& receipt, QString* error = nullptr) const;
    int removeCommittedBefore(const QDateTime& cutoffUtc, int maximumRows,
                              QString* error = nullptr) const;
};
