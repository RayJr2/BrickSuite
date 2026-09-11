#include "../src/services/application/AsyncReadResult.h"
#include "../src/services/application/dto/RemoteReadJson.h"
#include "../src/services/application/LocalReferenceDecoration.h"
#include "../src/network/BrickSuiteOperationDispatcher.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QTimer>
#include <iostream>

namespace {
bool require(bool condition, const char* message)
{ if (!condition) std::cerr << message << '\n'; return condition; }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;

    const auto first = nextReadRequestToken();
    const auto second = nextReadRequestToken();
    ok &= require(second > first, "request tokens are not monotonic");
    const auto success = AsyncReadResult<int>::success(second, 7);
    ok &= require(success.succeeded() && *success.value == 7, "typed success failed");
    ok &= require(!AsyncReadResult<int>::failure(second, AsyncReadError::Timeout,
        QStringLiteral("timeout")).succeeded(), "typed failure failed");

    RemoteReadDto::InventoryRow row;
    row.inventoryRecordId = 44; row.workspaceId = 2;
    row.partNumber = QStringLiteral("3001"); row.partNameFallback = QStringLiteral("Brick 2 x 4");
    row.rebrickableCategoryId = 11;
    row.rebrickableColorId = 4; row.colorNameFallback = QStringLiteral("Red");
    row.quantity = 9; row.storageId = 8; row.storagePath = QStringLiteral("Shelf / Bin");
    row.manufacturerDisplay = QStringLiteral("LEGO"); row.condition = QStringLiteral("Used");
    row.ownershipType = QStringLiteral("Owned");
    const QJsonObject json = RemoteReadJson::toJson(row);
    ok &= require(!json.contains(QStringLiteral("partId")) && !json.contains(QStringLiteral("colorId")),
                  "local catalog primary key leaked");
    RemoteReadDto::InventoryRow decoded;
    RemoteReadJson::DecodeError decodeError;
    ok &= require(RemoteReadJson::fromJson(json, &decoded, &decodeError)
                  && decoded.partNumber == QStringLiteral("3001")
                  && decoded.rebrickableCategoryId == 11
                  && decoded.rebrickableColorId == 4, "portable inventory round trip failed");
    RemoteReadDto::LostInventoryRow lost;lost.partNumber="3001";lost.partNameFallback="Brick 2 x 4";
    lost.rebrickableColorId=4;lost.colorNameFallback="Red";lost.outstandingQuantity=3;
    lost.lastStorageId=8;lost.lastStoragePath="Shelf / Bin";lost.condition="Used";
    lost.ownershipType="Owned";lost.lastLostUtc=QDateTime::currentDateTimeUtc();
    RemoteReadDto::LostInventoryRow decodedLost;
    ok &= require(RemoteReadJson::fromJson(RemoteReadJson::toJson(lost),&decodedLost,&decodeError)
        && decodedLost.partNumber=="3001"&&decodedLost.rebrickableColorId==4
        && decodedLost.outstandingQuantity==3,"Lost Inventory DTO round trip failed");
    QJsonObject invalid = json; invalid.insert(QStringLiteral("partNumber"), QString(513, QLatin1Char('x')));
    ok &= require(!RemoteReadJson::fromJson(invalid, &decoded, &decodeError), "oversized text accepted");
    RemoteReadDto::StorageSummary storage;
    storage.storageId=8; storage.parentStorageId=2; storage.name="Drawer";
    storage.displayPath="Room / Cabinet / Drawer"; storage.typeName="Drawer";
    storage.sortOrder=4; storage.active=false; storage.allowsInventory=true;
    storage.allowsCollection=true;
    RemoteReadDto::StorageSummary decodedStorage;
    ok &= require(RemoteReadJson::fromJson(RemoteReadJson::toJson(storage),
        &decodedStorage,&decodeError) && decodedStorage.typeName==QStringLiteral("Drawer")
        && decodedStorage.sortOrder==4 && !decodedStorage.active
        && decodedStorage.allowsInventory && decodedStorage.allowsCollection,
        "Storage hierarchy projection round trip failed");
    QList<RemoteReadDto::InventoryRow> decorated{decoded};
    decorated[0] = row;
    LocalReferenceDecoration decoration{{{QStringLiteral("3001"), QStringLiteral("Local Brick")}},
                                        {{4, QStringLiteral("Local Red")}}};
    decoration.decorate(decorated);
    ok &= require(decorated[0].partNameFallback == QStringLiteral("Local Brick")
                  && decorated[0].inventoryRecordId == 44,
                  "batched local decoration changed identity or missed local data");
    RemoteReadDto::PageRequest paging;
    ok &= require(RemoteReadJson::pageRequest({{"page",1},{"pageSize",500}}, &paging), "valid paging rejected");
    ok &= require(!RemoteReadJson::pageRequest({{"page",0},{"pageSize",501}}, &paging), "invalid paging accepted");

    RemoteReadDto::BuildSummary build;
    build.buildId=9;build.workspaceId=2;build.buildType="Set";build.name="Set Build";
    build.setNumber="1000-1";build.inventoryMode="Stock";build.status="Planned";
    build.createdUtc=QDateTime::currentDateTimeUtc();build.modifiedUtc=build.createdUtc;
    RemoteReadDto::BuildSummary decodedBuild;
    QJsonObject buildJson=RemoteReadJson::toJson(build);
    ok &= require(RemoteReadJson::fromJson(buildJson,&decodedBuild,&decodeError)
        && decodedBuild.modifiedUtc.isValid(),"Build concurrency timestamp round trip failed");
    buildJson.remove(QStringLiteral("createdUtc"));
    buildJson.remove(QStringLiteral("modifiedUtc"));
    ok &= require(RemoteReadJson::fromJson(buildJson,&decodedBuild,&decodeError),
        "Protocol 1.2 Build read without G1 timestamps lost compatibility");

    RemoteReadDto::BuildRequirement requirement;
    requirement.requirementId=70; requirement.buildId=9; requirement.partNumber="3001";
    requirement.partNameFallback="Brick 2 x 4"; requirement.rebrickableColorId=4;
    requirement.colorNameFallback="Red"; requirement.quantityRequired=12;
    requirement.owned=7; requirement.thisRequirementAllocated=3;
    requirement.otherAllocated=2; requirement.available=2; requirement.missing=7;
    RemoteReadDto::BuildRequirement decodedRequirement;
    ok &= require(RemoteReadJson::fromJson(RemoteReadJson::toJson(requirement),
        &decodedRequirement, &decodeError) && decodedRequirement.partNumber=="3001"
        && decodedRequirement.owned==7 && decodedRequirement.thisRequirementAllocated==3
        && decodedRequirement.otherAllocated==2 && decodedRequirement.available==2
        && decodedRequirement.missing==7,
        "Build requirement DTO round trip failed");
    QJsonObject legacyRequirementJson=RemoteReadJson::toJson(requirement);
    legacyRequirementJson.remove(QStringLiteral("owned"));
    legacyRequirementJson.remove(QStringLiteral("thisRequirementAllocated"));
    legacyRequirementJson.remove(QStringLiteral("otherAllocated"));
    legacyRequirementJson.remove(QStringLiteral("available"));
    legacyRequirementJson.remove(QStringLiteral("missing"));
    RemoteReadDto::BuildRequirement legacyRequirement;
    ok &= require(RemoteReadJson::fromJson(legacyRequirementJson, &legacyRequirement, &decodeError)
        && legacyRequirement.owned==-1 && legacyRequirement.thisRequirementAllocated==-1
        && legacyRequirement.otherAllocated==-1 && legacyRequirement.available==-1
        && legacyRequirement.missing==-1,
        "Build requirement DTO rejected protocol-compatible missing availability fields");
    RemoteReadDto::PullingRow pulling; pulling.requirementId=70;pulling.allocationId=71;
    pulling.inventoryRecordId=44;pulling.storageId=8;pulling.partNumber="3001";
    pulling.rebrickableColorId=4;pulling.quantityRequired=12;pulling.quantityAllocated=3;
    RemoteReadDto::PullingRow decodedPulling;
    ok &= require(RemoteReadJson::fromJson(RemoteReadJson::toJson(pulling),
        &decodedPulling,&decodeError),"Pulling DTO round trip failed");
    RemoteReadDto::BuildCancellationReturnRow cancellationReturn;
    cancellationReturn.requirementId=70;cancellationReturn.partNumber="3001";
    cancellationReturn.partNameFallback="Brick 2 x 4";
    cancellationReturn.colorNameFallback="Red";cancellationReturn.manufacturerDisplay="LEGO";
    cancellationReturn.quantityPulled=3;cancellationReturn.spare=false;
    RemoteReadDto::BuildCancellationReturnRow decodedCancellationReturn;
    ok &= require(RemoteReadJson::fromJson(RemoteReadJson::toJson(cancellationReturn),
        &decodedCancellationReturn,&decodeError)
        && decodedCancellationReturn.requirementId==70
        && decodedCancellationReturn.manufacturerDisplay==QStringLiteral("LEGO")
        && decodedCancellationReturn.quantityPulled==3,
        "Build cancellation return DTO round trip failed");
    QJsonObject invalidCancellationReturn=RemoteReadJson::toJson(cancellationReturn);
    invalidCancellationReturn[QStringLiteral("quantityPulled")]=0;
    ok &= require(!RemoteReadJson::fromJson(invalidCancellationReturn,
        &decodedCancellationReturn,&decodeError),
        "Build cancellation return DTO accepted a non-positive pulled quantity");

    QElapsedTimer serializationTimer; serializationTimer.start();
    QJsonArray inventoryRows; for(int i=0;i<250;++i){row.inventoryRecordId=i+1;inventoryRows.append(RemoteReadJson::toJson(row));}
    const qsizetype inventoryBytes=QJsonDocument({{"rows",inventoryRows},{"page",1},{"pageSize",250},{"totalRows",250}}).toJson(QJsonDocument::Compact).size();
    QJsonArray requirementRows;for(int i=0;i<500;++i){requirement.requirementId=i+1;requirementRows.append(RemoteReadJson::toJson(requirement));}
    const qsizetype requirementBytes=QJsonDocument({{"rows",requirementRows},{"page",1},{"pageSize",500},{"totalRows",500}}).toJson(QJsonDocument::Compact).size();
    QJsonArray pullingRows;for(int i=0;i<500;++i){pulling.requirementId=i+1;pulling.allocationId=i+1;pullingRows.append(RemoteReadJson::toJson(pulling));}
    const qsizetype pullingBytes=QJsonDocument({{"rows",pullingRows},{"page",1},{"pageSize",500},{"totalRows",500}}).toJson(QJsonDocument::Compact).size();
    RemoteReadDto::InventoryHistoryRow history;history.movementId=1;history.movementType="Move";history.quantityChange=2;history.notes="Synthetic history";history.createdUtc=QDateTime::currentDateTimeUtc();
    QJsonArray historyRows;for(int i=0;i<250;++i){history.movementId=i+1;historyRows.append(RemoteReadJson::toJson(history));}
    const qsizetype historyBytes=QJsonDocument(QJsonObject{{"rows",historyRows}}).toJson(QJsonDocument::Compact).size();
    RemoteReadDto::MissingPart missing;missing.partNumber="3001";missing.partNameFallback="Brick 2 x 4";missing.rebrickableColorId=4;missing.colorNameFallback="Red";missing.required=10;missing.missing=3;
    QJsonArray missingRows;for(int i=0;i<250;++i)missingRows.append(RemoteReadJson::toJson(missing));
    const qsizetype missingBytes=QJsonDocument({{"rows",missingRows},{"page",1},{"pageSize",250},{"totalRows",250}}).toJson(QJsonDocument::Compact).size();
    RemoteReadDto::CollectionSummary collection;collection.collectionItemId=1;collection.workspaceId=1;collection.type="Set";collection.setNumber="10300-1";collection.referenceFallback="10300-1";collection.titleFallback="Synthetic Collection Item";collection.state="Assembled";collection.condition="Used";collection.completeness="Complete";collection.active=true;
    QJsonArray collectionRows;for(int i=0;i<250;++i){collection.collectionItemId=i+1;collectionRows.append(RemoteReadJson::toJson(collection));}
    const qsizetype collectionBytes=QJsonDocument({{"rows",collectionRows},{"page",1},{"pageSize",250},{"totalRows",250}}).toJson(QJsonDocument::Compact).size();
    RemoteReadDto::CollectionSummary decodedCollection;
    ok &= require(RemoteReadJson::fromJson(RemoteReadJson::toJson(collection),
        &decodedCollection, &decodeError) && decodedCollection.setNumber == QStringLiteral("10300-1"),
        "Collection portable identity round trip failed");
    RemoteReadDto::PartReferenceCustomization customization;
    customization.customizationId=7; customization.partNumber="3001";
    customization.catalog="Bricks"; customization.section="Basic"; customization.displayOrder=12;
    RemoteReadDto::PartReferenceCustomization decodedCustomization;
    const QJsonObject emptyCustomizationJson=RemoteReadJson::toJson(customization);
    ok &= require(emptyCustomizationJson.value("anchorPartNumber").isString()
        && emptyCustomizationJson.value("anchorPartNumber").toString().isEmpty()
        && !emptyCustomizationJson.contains("createdUtc")
        && !emptyCustomizationJson.contains("modifiedUtc")
        && RemoteReadJson::fromJson(emptyCustomizationJson,&decodedCustomization,&decodeError)
        && decodedCustomization.displayOrder==12 && decodedCustomization.anchorPartNumber.isEmpty()
        && decodedCustomization.notes.isEmpty(),
        "Part Reference empty optional strings round trip failed");
    customization.partNameFallback="Brick 2 x 4"; customization.representativeFor="3001";
    customization.notes="User note"; customization.placement="After";
    customization.anchorPartNumber="3002";
    customization.createdUtc=QDateTime::fromString("2026-01-01T00:00:00.000Z",Qt::ISODateWithMs);
    customization.modifiedUtc=QDateTime::fromString("2026-01-02T00:00:00.000Z",Qt::ISODateWithMs);
    ok &= require(RemoteReadJson::fromJson(RemoteReadJson::toJson(customization),
        &decodedCustomization,&decodeError)
        && decodedCustomization.partNameFallback==customization.partNameFallback
        && decodedCustomization.representativeFor==customization.representativeFor
        && decodedCustomization.notes==customization.notes
        && decodedCustomization.placement==customization.placement
        && decodedCustomization.anchorPartNumber==customization.anchorPartNumber
        && decodedCustomization.createdUtc==customization.createdUtc
        && decodedCustomization.modifiedUtc==customization.modifiedUtc,
        "Part Reference populated optional strings round trip failed");
    QJsonObject nullOptional=RemoteReadJson::toJson(customization);
    nullOptional["anchorPartNumber"]=QJsonValue::Null;
    ok &= require(RemoteReadJson::fromJson(nullOptional,&decodedCustomization,&decodeError)
        && decodedCustomization.anchorPartNumber.isEmpty(),
        "Part Reference null optional string compatibility failed");
    QJsonObject invalidRequired=RemoteReadJson::toJson(customization);
    invalidRequired["partNumber"]=QJsonValue::Null;
    ok &= require(!RemoteReadJson::fromJson(invalidRequired,&decodedCustomization,&decodeError),
        "Part Reference required string accepted null");
    invalidRequired=RemoteReadJson::toJson(customization);
    invalidRequired["catalog"]=42;
    ok &= require(!RemoteReadJson::fromJson(invalidRequired,&decodedCustomization,&decodeError),
        "Part Reference required string accepted wrong type");
    ok &= require(inventoryBytes<BrickSuiteProtocol::MaximumMessageBytes
        && requirementBytes<BrickSuiteProtocol::MaximumMessageBytes
        && pullingBytes<BrickSuiteProtocol::MaximumMessageBytes
        && historyBytes<BrickSuiteProtocol::MaximumMessageBytes
        && missingBytes<BrickSuiteProtocol::MaximumMessageBytes
        && collectionBytes<BrickSuiteProtocol::MaximumMessageBytes,
        "bounded pages exceed protocol message limit");
    std::cout << "Payload bytes inventory250=" << inventoryBytes
              << " requirements500=" << requirementBytes
              << " pulling500=" << pullingBytes
              << " history250=" << historyBytes
              << " missing250=" << missingBytes
              << " collection250=" << collectionBytes
              << " serializationMs=" << serializationTimer.elapsed() << '\n';

    BrickSuiteOperationDispatcher dispatcher;
    bool completed = false;
    dispatcher.registerAsyncOperation(QStringLiteral("workspace.list"), true,
        [](const BrickSuiteProtocol::Message& request, BrickSuiteOperationDispatcher::Completion completion) {
            QTimer::singleShot(0, [request, completion = std::move(completion)]() mutable {
                completion(BrickSuiteProtocol::response(request, {{"rows", QJsonArray{}}}));
            });
        });
    const auto request = BrickSuiteProtocol::request(QStringLiteral("workspace.list"));
    dispatcher.dispatchAsync(request, false, [&](BrickSuiteProtocol::Message response) {
        ok &= require(response.error.code == QStringLiteral("AUTH_REQUIRED"), "unauthenticated read allowed");
    });
    dispatcher.dispatchAsync(request, true, [&](BrickSuiteProtocol::Message response) {
        ok &= require(response.requestId == request.requestId && response.success,
                      "async correlation failed");
        completed = true; app.quit();
    });
    QTimer::singleShot(1000, &app, &QCoreApplication::quit);
    app.exec();
    ok &= require(completed, "async handler did not complete");
    return ok ? 0 : 1;
}
