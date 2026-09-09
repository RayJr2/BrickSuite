#pragma once

#include "AsyncReadResult.h"
#include "dto/RemoteReadDtos.h"

#include <QJsonObject>
#include <QObject>

class BrickSuiteWebSocketClient;

// Transport-neutral asynchronous contracts used by the remote composition.
// Widgets are intentionally not converted in M26.4B.
class RemoteReadApplicationServices : public QObject
{
    Q_OBJECT
public:
    explicit RemoteReadApplicationServices(BrickSuiteWebSocketClient& client,
                                           QObject* parent = nullptr);

    ReadRequestToken listWorkspaces(QObject* context,
        AsyncReadCompletion<QList<RemoteReadDto::WorkspaceSummary>> completion);
    ReadRequestToken listStorage(qint64 workspaceId, QObject* context,
        AsyncReadCompletion<QList<RemoteReadDto::StorageSummary>> completion);
    ReadRequestToken searchInventory(const RemoteReadDto::InventorySearchRequest& request,
        QObject* context, AsyncReadCompletion<RemoteReadDto::Page<RemoteReadDto::InventoryRow>> completion);
    ReadRequestToken getInventory(qint64 workspaceId, qint64 inventoryRecordId, QObject* context,
        AsyncReadCompletion<RemoteReadDto::InventoryDetail> completion);
    ReadRequestToken inventoryHistory(qint64 workspaceId, const QString& partNumber,
        int rebrickableColorId, QObject* context,
        AsyncReadCompletion<QList<RemoteReadDto::InventoryHistoryRow>> completion);
    ReadRequestToken listBuilds(qint64 workspaceId, bool includeArchived, QObject* context,
        AsyncReadCompletion<QList<RemoteReadDto::BuildSummary>> completion);
    ReadRequestToken getBuild(qint64 buildId, QObject* context,
        AsyncReadCompletion<RemoteReadDto::BuildDetail> completion);
    ReadRequestToken buildRequirements(qint64 buildId, const RemoteReadDto::PageRequest& page,
        QObject* context, AsyncReadCompletion<RemoteReadDto::Page<RemoteReadDto::BuildRequirement>> completion);
    ReadRequestToken missingParts(qint64 workspaceId, qint64 buildId,
        const RemoteReadDto::PageRequest& page, QObject* context,
        AsyncReadCompletion<RemoteReadDto::Page<RemoteReadDto::MissingPart>> completion);
    ReadRequestToken pulling(qint64 buildId, const RemoteReadDto::PageRequest& page,
        QObject* context, AsyncReadCompletion<RemoteReadDto::Page<RemoteReadDto::PullingRow>> completion);
    ReadRequestToken searchCollection(const RemoteReadDto::CollectionSearchRequest& request,
        QObject* context, AsyncReadCompletion<RemoteReadDto::Page<RemoteReadDto::CollectionSummary>> completion);
    ReadRequestToken getCollection(qint64 collectionItemId, QObject* context,
        AsyncReadCompletion<RemoteReadDto::CollectionDetail> completion);
    ReadRequestToken listPartReferenceCustomizations(QObject* context,
        AsyncReadCompletion<QList<RemoteReadDto::PartReferenceCustomization>> completion);

    bool isAvailableFor(const QString& operation) const;

private:
    template <typename T, typename Decoder>
    ReadRequestToken request(const QString& operation, const QJsonObject& payload,
                             QObject* context, AsyncReadCompletion<T> completion,
                             Decoder decoder);
    static AsyncReadError mapError(const QString& code);
    BrickSuiteWebSocketClient& m_client;
};
