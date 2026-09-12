#pragma once

#include "AsyncReadResult.h"
#include "dto/RemoteReadDtos.h"

#include <QJsonObject>
#include <QObject>
#include <QStringList>

class BrickSuiteWebSocketClient;
class RemoteSessionState;

// Transport-neutral asynchronous contracts used by the remote composition.
// Widgets are intentionally not converted in M26.4B.
class RemoteReadApplicationServices : public QObject
{
    Q_OBJECT
public:
    explicit RemoteReadApplicationServices(BrickSuiteWebSocketClient& client,
                                           RemoteSessionState* session = nullptr,
                                           QObject* parent = nullptr);

    ReadRequestToken listWorkspaces(QObject* context,
        AsyncReadCompletion<QList<RemoteReadDto::WorkspaceSummary>> completion);
    ReadRequestToken listManufacturerNames(QObject* context,
        AsyncReadCompletion<QStringList> completion);
    ReadRequestToken listStorage(qint64 workspaceId, QObject* context,
        AsyncReadCompletion<QList<RemoteReadDto::StorageSummary>> completion);
    ReadRequestToken listStorage(qint64 workspaceId, bool includeInactive, QObject* context,
        AsyncReadCompletion<QList<RemoteReadDto::StorageSummary>> completion);
    ReadRequestToken getStorage(qint64 workspaceId, qint64 storageId, QObject* context,
        AsyncReadCompletion<RemoteReadDto::StorageDetail> completion);
    ReadRequestToken listStorageTypes(QObject* context,
        AsyncReadCompletion<QList<RemoteReadDto::StorageType>> completion);
    ReadRequestToken searchInventory(const RemoteReadDto::InventorySearchRequest& request,
        QObject* context, AsyncReadCompletion<RemoteReadDto::Page<RemoteReadDto::InventoryRow>> completion);
    ReadRequestToken getInventory(qint64 workspaceId, qint64 inventoryRecordId, QObject* context,
        AsyncReadCompletion<RemoteReadDto::InventoryDetail> completion);
    ReadRequestToken inventoryHistory(qint64 workspaceId, const QString& partNumber,
        int rebrickableColorId, QObject* context,
        AsyncReadCompletion<QList<RemoteReadDto::InventoryHistoryRow>> completion);
    ReadRequestToken listLostInventory(qint64 workspaceId, QObject* context,
        AsyncReadCompletion<QList<RemoteReadDto::LostInventoryRow>> completion);
    ReadRequestToken listBuilds(qint64 workspaceId, bool includeArchived,
        const RemoteReadDto::PageRequest& page, QObject* context,
        AsyncReadCompletion<RemoteReadDto::Page<RemoteReadDto::BuildSummary>> completion);
    ReadRequestToken getBuild(qint64 workspaceId, qint64 buildId, QObject* context,
        AsyncReadCompletion<RemoteReadDto::BuildDetail> completion);
    ReadRequestToken buildCancellationReturns(qint64 workspaceId, qint64 buildId, QObject* context,
        AsyncReadCompletion<QList<RemoteReadDto::BuildCancellationReturnRow>> completion);
    ReadRequestToken buildDisassemblyReturns(qint64 workspaceId, qint64 buildId, QObject* context,
        AsyncReadCompletion<QList<RemoteReadDto::BuildCancellationReturnRow>> completion);
    ReadRequestToken buildRequirements(qint64 workspaceId, qint64 buildId, const RemoteReadDto::PageRequest& page,
        QObject* context, AsyncReadCompletion<RemoteReadDto::Page<RemoteReadDto::BuildRequirement>> completion);
    ReadRequestToken missingParts(qint64 workspaceId, qint64 buildId,
        const RemoteReadDto::PageRequest& page, QObject* context,
        AsyncReadCompletion<RemoteReadDto::Page<RemoteReadDto::MissingPart>> completion);
    ReadRequestToken pulling(qint64 workspaceId, qint64 buildId, const RemoteReadDto::PageRequest& page,
        QObject* context, AsyncReadCompletion<RemoteReadDto::Page<RemoteReadDto::PullingRow>> completion);
    ReadRequestToken searchCollection(const RemoteReadDto::CollectionSearchRequest& request,
        QObject* context, AsyncReadCompletion<RemoteReadDto::Page<RemoteReadDto::CollectionSummary>> completion);
    ReadRequestToken getCollection(qint64 workspaceId, qint64 collectionItemId, QObject* context,
        AsyncReadCompletion<RemoteReadDto::CollectionDetail> completion);
    ReadRequestToken listPartReferenceCustomizations(QObject* context,
        AsyncReadCompletion<QList<RemoteReadDto::PartReferenceCustomization>> completion);

    bool isAvailableFor(const QString& operation) const;

private:
    template <typename T, typename Decoder>
    ReadRequestToken request(const QString& operation, const QJsonObject& payload,
                             QObject* context, AsyncReadCompletion<T> completion,
                             Decoder decoder, bool hostGlobal = false);
    static AsyncReadError mapError(const QString& code);
    BrickSuiteWebSocketClient& m_client;
    RemoteSessionState* m_session = nullptr;
};
