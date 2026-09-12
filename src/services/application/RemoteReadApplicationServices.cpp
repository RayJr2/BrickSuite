#include "RemoteReadApplicationServices.h"

#include "dto/RemoteReadJson.h"
#include "../../network/BrickSuiteWebSocketClient.h"
#include "../../network/RemoteSessionState.h"

#include <QJsonArray>
#include <QTimer>

RemoteReadApplicationServices::RemoteReadApplicationServices(
    BrickSuiteWebSocketClient& client, RemoteSessionState* session, QObject* parent)
    : QObject(parent), m_client(client), m_session(session) {}

bool RemoteReadApplicationServices::isAvailableFor(const QString& operation) const
{ return m_client.status().state == BrickSuiteConnectionState::ConnectedAuthenticated
      && m_client.supportsOperation(operation); }

AsyncReadError RemoteReadApplicationServices::mapError(const QString& code)
{
    if (code == QStringLiteral("NOT_FOUND")) return AsyncReadError::NotFound;
    if (code == QStringLiteral("INVALID_REQUEST")) return AsyncReadError::InvalidRequest;
    if (code == QStringLiteral("TIMEOUT")) return AsyncReadError::Timeout;
    if (code == QStringLiteral("SERVER_BUSY")) return AsyncReadError::ServerBusy;
    if (code == QStringLiteral("UNKNOWN_OPERATION")) return AsyncReadError::Unsupported;
    if (code == QStringLiteral("AUTH_REQUIRED")) return AsyncReadError::Unavailable;
    return AsyncReadError::InternalFailure;
}

template <typename T, typename Decoder>
ReadRequestToken RemoteReadApplicationServices::request(
    const QString& operation, const QJsonObject& payload, QObject* context,
    AsyncReadCompletion<T> completion, Decoder decoder, bool hostGlobal)
{
    const ReadRequestToken token = nextReadRequestToken();
    if (!context) return token;
    const RemoteSessionState::Snapshot sessionSnapshot = m_session
        ? m_session->snapshot() : RemoteSessionState::Snapshot{};
    if (m_client.status().state != BrickSuiteConnectionState::ConnectedAuthenticated) {
        QTimer::singleShot(0, context, [token, completion = std::move(completion)]() mutable {
            completion(AsyncReadResult<T>::failure(token, AsyncReadError::Unavailable,
                QStringLiteral("Connect and authenticate to BrickSuite Host first.")));
        });
        return token;
    }
    if (!m_client.supportsOperation(operation)) {
        QTimer::singleShot(0, context, [token, completion = std::move(completion)]() mutable {
            completion(AsyncReadResult<T>::failure(token, AsyncReadError::Unsupported,
                QStringLiteral("The connected Host does not support this read operation.")));
        });
        return token;
    }
    const QString requestId = m_client.sendRequest(operation, payload, context,
        [this, token, completion, decoder, sessionSnapshot, hostGlobal](const QJsonObject& response) mutable {
            if (m_session && (hostGlobal
                    ? !m_session->acceptsHostGlobal(sessionSnapshot)
                    : !m_session->accepts(sessionSnapshot))) return;
            T value;
            QString error;
            if (!decoder(response, &value, &error)) {
                completion(AsyncReadResult<T>::failure(token, AsyncReadError::InternalFailure,
                    error.isEmpty() ? QStringLiteral("The Host returned invalid read data.") : error));
                return;
            }
            completion(AsyncReadResult<T>::success(token, std::move(value)));
        }, [this, token, completion, sessionSnapshot, hostGlobal](const BrickSuiteProtocol::Error& error) mutable {
            if (m_session && (hostGlobal
                    ? !m_session->acceptsHostGlobal(sessionSnapshot)
                    : !m_session->accepts(sessionSnapshot))) return;
            completion(AsyncReadResult<T>::failure(token, mapError(error.code), error.message));
        });
    if (requestId.isEmpty()) {
        QTimer::singleShot(0, context, [token, completion = std::move(completion)]() mutable {
            completion(AsyncReadResult<T>::failure(token, AsyncReadError::ServerBusy,
                QStringLiteral("Too many Host requests are already pending.")));
        });
    }
    return token;
}

ReadRequestToken RemoteReadApplicationServices::listWorkspaces(QObject* context,
    AsyncReadCompletion<QList<RemoteReadDto::WorkspaceSummary>> completion)
{
    return request<QList<RemoteReadDto::WorkspaceSummary>>(QStringLiteral("workspace.list"), {}, context,
        std::move(completion), [](const QJsonObject& o, auto* out, QString* error) {
            const QJsonValue rows = o.value(QStringLiteral("rows"));
            if (!rows.isArray() || rows.toArray().size() > RemoteReadDto::MaximumPageSize) return false;
            for (const QJsonValue& value : rows.toArray()) {
                RemoteReadDto::WorkspaceSummary row; RemoteReadJson::DecodeError e;
                if (!value.isObject() || !RemoteReadJson::fromJson(value.toObject(), &row, &e)) { if(error)*error=e.message; return false; }
                out->append(row);
            } return true;
        }, true);
}

ReadRequestToken RemoteReadApplicationServices::listManufacturerNames(
    QObject* context, AsyncReadCompletion<QStringList> completion)
{
    return request<QStringList>(QStringLiteral("manufacturers.list"), {}, context,
        std::move(completion), [](const QJsonObject& object, QStringList* out, QString*) {
            const QJsonValue rows = object.value(QStringLiteral("rows"));
            if (!rows.isArray() || rows.toArray().size() > RemoteReadDto::MaximumPageSize)
                return false;
            QSet<QString> seen;
            for (const QJsonValue& value : rows.toArray()) {
                if (!value.isString() || value.toString().trimmed().isEmpty()
                    || value.toString().size() > RemoteReadDto::MaximumTextLength)
                    return false;
                const QString name = value.toString();
                const QString key = name.toCaseFolded();
                if (seen.contains(key)) return false;
                seen.insert(key); out->append(name);
            }
            return true;
        }, true);
}

ReadRequestToken RemoteReadApplicationServices::listStorage(qint64 workspaceId, QObject* context,
    AsyncReadCompletion<QList<RemoteReadDto::StorageSummary>> completion)
{ return listStorage(workspaceId, false, context, std::move(completion)); }

ReadRequestToken RemoteReadApplicationServices::listStorage(qint64 workspaceId, bool includeInactive, QObject* context,
    AsyncReadCompletion<QList<RemoteReadDto::StorageSummary>> completion)
{
    QJsonObject payload{{"workspaceId",double(workspaceId)}};
    if (includeInactive) payload.insert(QStringLiteral("includeInactive"), true);
    return request<QList<RemoteReadDto::StorageSummary>>(QStringLiteral("storage.list"),
        payload, context, std::move(completion),
        [](const QJsonObject& o, auto* out, QString* error) {
            const QJsonValue rows=o.value("rows"); if(!rows.isArray() || rows.toArray().size()>RemoteReadDto::MaximumStorageLocations)return false;
            for(const auto& v:rows.toArray()){RemoteReadDto::StorageSummary row;RemoteReadJson::DecodeError e;if(!v.isObject()||!RemoteReadJson::fromJson(v.toObject(),&row,&e)){if(error)*error=e.message;return false;}out->append(row);}return true;
        });
}
ReadRequestToken RemoteReadApplicationServices::getStorage(qint64 workspaceId,qint64 storageId,QObject*context,AsyncReadCompletion<RemoteReadDto::StorageDetail> completion)
{return request<RemoteReadDto::StorageDetail>("storage.get",{{"workspaceId",double(workspaceId)},{"storageId",double(storageId)}},context,std::move(completion),[](const QJsonObject&o,auto*out,QString*error){RemoteReadJson::DecodeError e;const auto v=o.value("item");if(!v.isObject()||!RemoteReadJson::fromJson(v.toObject(),out,&e)){if(error)*error=e.message;return false;}return true;});}
ReadRequestToken RemoteReadApplicationServices::listStorageTypes(QObject*context,AsyncReadCompletion<QList<RemoteReadDto::StorageType>> completion)
{return request<QList<RemoteReadDto::StorageType>>("storage.types.list",{},context,std::move(completion),[](const QJsonObject&o,auto*out,QString*error){const auto rows=o.value("rows");if(!rows.isArray()||rows.toArray().size()>RemoteReadDto::MaximumStorageLocations)return false;for(const auto&v:rows.toArray()){RemoteReadDto::StorageType row;RemoteReadJson::DecodeError e;if(!v.isObject()||!RemoteReadJson::fromJson(v.toObject(),&row,&e)){if(error)*error=e.message;return false;}out->append(row);}return true;});}

ReadRequestToken RemoteReadApplicationServices::searchInventory(
    const RemoteReadDto::InventorySearchRequest& r, QObject* context,
    AsyncReadCompletion<RemoteReadDto::Page<RemoteReadDto::InventoryRow>> completion)
{
    QJsonObject payload{{"workspaceId",double(r.workspaceId)},{"text",r.text},{"storageId",double(r.storageId)},
        {"rebrickableCategoryId",r.rebrickableCategoryId},{"rebrickableColorId",r.rebrickableColorId},
        {"page",r.paging.page},{"pageSize",r.paging.pageSize}};
    return request<RemoteReadDto::Page<RemoteReadDto::InventoryRow>>(QStringLiteral("inventory.search"),payload,context,std::move(completion),
        [](const QJsonObject& o, auto* out, QString* error){qint64 total=o.value("totalRows").toInteger(-1);int page=o.value("page").toInt();int size=o.value("pageSize").toInt();const auto rows=o.value("rows");if(total<0||page<1||size<1||size>RemoteReadDto::MaximumPageSize||!rows.isArray()||rows.toArray().size()>size)return false;out->page=page;out->pageSize=size;out->totalRows=int(qMin<qint64>(total,INT_MAX));for(const auto& v:rows.toArray()){RemoteReadDto::InventoryRow row;RemoteReadJson::DecodeError e;if(!v.isObject()||!RemoteReadJson::fromJson(v.toObject(),&row,&e)){if(error)*error=e.message;return false;}out->rows.append(row);}return true;});
}

ReadRequestToken RemoteReadApplicationServices::listBuilds(qint64 workspaceId, bool archived, QObject* context,
    AsyncReadCompletion<QList<RemoteReadDto::BuildSummary>> completion)
{
    return request<QList<RemoteReadDto::BuildSummary>>(QStringLiteral("builds.list"),{{"workspaceId",double(workspaceId)},{"includeArchived",archived}},context,std::move(completion),
        [](const QJsonObject& o,auto* out,QString* error){const auto rows=o.value("rows");if(!rows.isArray()||rows.toArray().size()>RemoteReadDto::MaximumPageSize)return false;for(const auto& v:rows.toArray()){RemoteReadDto::BuildSummary row;RemoteReadJson::DecodeError e;if(!v.isObject()||!RemoteReadJson::fromJson(v.toObject(),&row,&e)){if(error)*error=e.message;return false;}out->append(row);}return true;});
}
ReadRequestToken RemoteReadApplicationServices::getBuild(qint64 workspace,qint64 id,QObject*context,AsyncReadCompletion<RemoteReadDto::BuildDetail> completion)
{return request<RemoteReadDto::BuildDetail>("builds.get",{{"workspaceId",double(workspace)},{"buildId",double(id)}},context,std::move(completion),[](const QJsonObject&o,auto*out,QString*error){RemoteReadJson::DecodeError e;const auto v=o.value("build");if(!v.isObject()||!RemoteReadJson::fromJson(v.toObject(),static_cast<RemoteReadDto::BuildSummary*>(out),&e)){if(error)*error=e.message;return false;}return true;});}

ReadRequestToken RemoteReadApplicationServices::buildCancellationReturns(qint64 workspace,qint64 id,QObject*context,AsyncReadCompletion<QList<RemoteReadDto::BuildCancellationReturnRow>> completion)
{return request<QList<RemoteReadDto::BuildCancellationReturnRow>>("builds.cancelReturns",{{"workspaceId",double(workspace)},{"buildId",double(id)}},context,std::move(completion),[](const QJsonObject&o,auto*out,QString*error){const auto rows=o.value("rows");if(!rows.isArray()||rows.toArray().size()>10000)return false;for(const auto&v:rows.toArray()){RemoteReadDto::BuildCancellationReturnRow row;RemoteReadJson::DecodeError e;if(!v.isObject()||!RemoteReadJson::fromJson(v.toObject(),&row,&e)){if(error)*error=e.message;return false;}out->append(row);}return true;});}

ReadRequestToken RemoteReadApplicationServices::buildDisassemblyReturns(qint64 workspace,qint64 id,QObject*context,AsyncReadCompletion<QList<RemoteReadDto::BuildCancellationReturnRow>> completion)
{return request<QList<RemoteReadDto::BuildCancellationReturnRow>>("builds.disassemblyReturns",{{"workspaceId",double(workspace)},{"buildId",double(id)}},context,std::move(completion),[](const QJsonObject&o,auto*out,QString*error){const auto rows=o.value("rows");if(!rows.isArray()||rows.toArray().isEmpty()||rows.toArray().size()>500)return false;for(const auto&v:rows.toArray()){RemoteReadDto::BuildCancellationReturnRow row;RemoteReadJson::DecodeError e;if(!v.isObject()||!RemoteReadJson::fromJson(v.toObject(),&row,&e)){if(error)*error=e.message;return false;}out->append(row);}return true;});}

ReadRequestToken RemoteReadApplicationServices::getInventory(qint64 workspaceId,qint64 id,QObject*context,AsyncReadCompletion<RemoteReadDto::InventoryDetail> completion)
{return request<RemoteReadDto::InventoryDetail>("inventory.get",{{"workspaceId",double(workspaceId)},{"inventoryRecordId",double(id)}},context,std::move(completion),[](const QJsonObject&o,auto*out,QString*error){RemoteReadJson::DecodeError e;const auto v=o.value("item");if(!v.isObject()||!RemoteReadJson::fromJson(v.toObject(),out,&e)){if(error)*error=e.message;return false;}return true;});}

ReadRequestToken RemoteReadApplicationServices::inventoryHistory(qint64 workspace,const QString&part,int color,QObject*context,AsyncReadCompletion<QList<RemoteReadDto::InventoryHistoryRow>> completion)
{return request<QList<RemoteReadDto::InventoryHistoryRow>>("inventory.history",{{"workspaceId",double(workspace)},{"partNumber",part},{"rebrickableColorId",color}},context,std::move(completion),[](const QJsonObject&o,auto*out,QString*error){const auto rows=o.value("rows");if(!rows.isArray()||rows.toArray().size()>RemoteReadDto::MaximumPageSize)return false;for(const auto&v:rows.toArray()){RemoteReadDto::InventoryHistoryRow x;RemoteReadJson::DecodeError e;if(!v.isObject()||!RemoteReadJson::fromJson(v.toObject(),&x,&e)){if(error)*error=e.message;return false;}out->append(x);}return true;});}

ReadRequestToken RemoteReadApplicationServices::listLostInventory(qint64 workspace,QObject*context,AsyncReadCompletion<QList<RemoteReadDto::LostInventoryRow>> completion)
{return request<QList<RemoteReadDto::LostInventoryRow>>("inventory.lost.list",{{"workspaceId",double(workspace)}},context,std::move(completion),[](const QJsonObject&o,auto*out,QString*error){const auto rows=o.value("rows");if(!rows.isArray()||rows.toArray().size()>RemoteReadDto::MaximumLostInventoryRows)return false;for(const auto&v:rows.toArray()){RemoteReadDto::LostInventoryRow x;RemoteReadJson::DecodeError e;if(!v.isObject()||!RemoteReadJson::fromJson(v.toObject(),&x,&e)){if(error)*error=e.message;return false;}out->append(x);}return true;});}

namespace {
template<class T,class Decoder> bool decodePage(const QJsonObject&o,RemoteReadDto::Page<T>*out,QString*error,Decoder decoder){const auto rows=o.value("rows");const int page=o.value("page").toInt();const int size=o.value("pageSize").toInt();const int total=o.value("totalRows").toInt(-1);if(!rows.isArray()||page<1||size<1||size>RemoteReadDto::MaximumPageSize||total<0||rows.toArray().size()>size)return false;out->page=page;out->pageSize=size;out->totalRows=total;for(const auto&v:rows.toArray()){T x;RemoteReadJson::DecodeError e;if(!v.isObject()||!decoder(v.toObject(),&x,&e)){if(error)*error=e.message;return false;}out->rows.append(x);}return true;}
QJsonObject buildPagePayload(qint64 workspace,qint64 id,const RemoteReadDto::PageRequest&p){return{{"workspaceId",double(workspace)},{"buildId",double(id)},{"page",p.page},{"pageSize",p.pageSize}};}
}
ReadRequestToken RemoteReadApplicationServices::buildRequirements(qint64 workspace,qint64 id,const RemoteReadDto::PageRequest&p,QObject*context,AsyncReadCompletion<RemoteReadDto::Page<RemoteReadDto::BuildRequirement>> completion)
{return request<RemoteReadDto::Page<RemoteReadDto::BuildRequirement>>("builds.requirements",buildPagePayload(workspace,id,p),context,std::move(completion),[](const auto&o,auto*out,QString*e){return decodePage(o,out,e,[](const auto&j,auto*x,auto*de){return RemoteReadJson::fromJson(j,x,de);});});}
ReadRequestToken RemoteReadApplicationServices::missingParts(qint64 workspace,qint64 id,const RemoteReadDto::PageRequest&p,QObject*context,AsyncReadCompletion<RemoteReadDto::Page<RemoteReadDto::MissingPart>> completion)
{return request<RemoteReadDto::Page<RemoteReadDto::MissingPart>>("builds.missingParts",buildPagePayload(workspace,id,p),context,std::move(completion),[](const auto&o,auto*out,QString*e){return decodePage(o,out,e,[](const auto&j,auto*x,auto*de){return RemoteReadJson::fromJson(j,x,de);});});}
ReadRequestToken RemoteReadApplicationServices::pulling(qint64 workspace,qint64 id,const RemoteReadDto::PageRequest&p,QObject*context,AsyncReadCompletion<RemoteReadDto::Page<RemoteReadDto::PullingRow>> completion)
{return request<RemoteReadDto::Page<RemoteReadDto::PullingRow>>("builds.pulling",buildPagePayload(workspace,id,p),context,std::move(completion),[](const auto&o,auto*out,QString*e){return decodePage(o,out,e,[](const auto&j,auto*x,auto*de){return RemoteReadJson::fromJson(j,x,de);});});}

ReadRequestToken RemoteReadApplicationServices::searchCollection(const RemoteReadDto::CollectionSearchRequest& r,QObject* context,AsyncReadCompletion<RemoteReadDto::Page<RemoteReadDto::CollectionSummary>> completion)
{
    return request<RemoteReadDto::Page<RemoteReadDto::CollectionSummary>>(QStringLiteral("collection.search"),{{"workspaceId",double(r.workspaceId)},{"text",r.text},{"type",r.type},{"state",r.state},{"condition",r.condition},{"completeness",r.completeness},{"storageId",double(r.storageId)},{"activeState",r.activeState},{"page",r.paging.page},{"pageSize",r.paging.pageSize}},context,std::move(completion),
        [](const QJsonObject& o,auto* out,QString* error){qint64 total=o.value("totalRows").toInteger(-1);int page=o.value("page").toInt();int size=o.value("pageSize").toInt();const auto rows=o.value("rows");if(total<0||page<1||size<1||size>RemoteReadDto::MaximumPageSize||!rows.isArray()||rows.toArray().size()>size)return false;out->page=page;out->pageSize=size;out->totalRows=int(qMin<qint64>(total,INT_MAX));for(const auto& v:rows.toArray()){RemoteReadDto::CollectionSummary row;RemoteReadJson::DecodeError e;if(!v.isObject()||!RemoteReadJson::fromJson(v.toObject(),&row,&e)){if(error)*error=e.message;return false;}out->rows.append(row);}return true;});
}
ReadRequestToken RemoteReadApplicationServices::getCollection(qint64 workspaceId,qint64 id,QObject*context,AsyncReadCompletion<RemoteReadDto::CollectionDetail> completion)
{return request<RemoteReadDto::CollectionDetail>("collection.get",{{"workspaceId",double(workspaceId)},{"collectionItemId",double(id)}},context,std::move(completion),[](const QJsonObject&o,auto*out,QString*error){RemoteReadJson::DecodeError e;const auto v=o.value("item");if(!v.isObject()||!RemoteReadJson::fromJson(v.toObject(),out,&e)){if(error)*error=e.message;return false;}return true;});}

ReadRequestToken RemoteReadApplicationServices::listPartReferenceCustomizations(QObject* context,AsyncReadCompletion<QList<RemoteReadDto::PartReferenceCustomization>> completion)
{
    return request<QList<RemoteReadDto::PartReferenceCustomization>>(QStringLiteral("partReference.customizations"),{},context,std::move(completion),
        [](const QJsonObject& o,auto* out,QString* error){const auto rows=o.value("rows");if(!rows.isArray()||rows.toArray().size()>RemoteReadDto::MaximumPageSize)return false;for(const auto& v:rows.toArray()){RemoteReadDto::PartReferenceCustomization row;RemoteReadJson::DecodeError e;if(!v.isObject()||!RemoteReadJson::fromJson(v.toObject(),&row,&e)){if(error)*error=e.message;return false;}out->append(row);}return true;});
}
