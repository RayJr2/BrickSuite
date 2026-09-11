#include "../src/services/application/dto/RemoteBuildMutationDtos.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>

namespace {
bool require(bool condition, const char* message)
{
    if (!condition) qCritical() << message;
    return condition;
}

RemoteBuildMutationDto::Request existingRequest()
{
    RemoteBuildMutationDto::Request value;
    value.workspaceId = 4; value.buildId = 9;
    value.mutationId = RemoteMutationDto::newMutationId();
    value.expected.modifiedUtc = QStringLiteral("2026-09-11T12:00:00.000Z");
    value.expected.buildType = QStringLiteral("Set");
    value.expected.reference = QStringLiteral("77244-1");
    value.expected.inventoryMode = QStringLiteral("Stock");
    value.expected.name = QStringLiteral("Race Car");
    value.expected.status = QStringLiteral("Pulling");
    value.expected.active = true;
    return value;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    RemoteMutationDto::Error error;

    auto add = existingRequest();
    add.buildType = QStringLiteral("Set"); add.reference = QStringLiteral("77244-1");
    add.inventoryMode = QStringLiteral("Stock"); add.initialStatus = QStringLiteral("Planned");
    add.name = QStringLiteral("Race Car");
    RemoteBuildMutationDto::Request decoded;
    ok &= require(RemoteBuildMutationDto::fromMetadata(QStringLiteral("builds.add"),
        RemoteBuildMutationDto::toMetadata(QStringLiteral("builds.add"), add), &decoded, &error)
        && decoded.reference == add.reference && decoded.initialStatus == QStringLiteral("Planned"),
        "Build Add DTO did not round-trip");

    auto edit = existingRequest(); edit.name = QStringLiteral("Renamed"); edit.notes = QStringLiteral("Note");
    edit.manufacturer = QStringLiteral("Custom Bricks");
    ok &= require(RemoteBuildMutationDto::fromMetadata(QStringLiteral("builds.edit"),
        RemoteBuildMutationDto::toMetadata(QStringLiteral("builds.edit"), edit), &decoded, &error)
        && decoded.expected.modifiedUtc == edit.expected.modifiedUtc
        && decoded.name == edit.name
        && decoded.manufacturer == QStringLiteral("Custom Bricks"),
        "Build Edit portable manufacturer name did not round-trip");

    auto cancel = existingRequest();
    cancel.returns.append({17, QStringLiteral("LEGO"), 22, 3, false});
    ok &= require(RemoteBuildMutationDto::fromMetadata(QStringLiteral("builds.cancel"),
        RemoteBuildMutationDto::toMetadata(QStringLiteral("builds.cancel"), cancel), &decoded, &error)
        && decoded.returns.size() == 1 && decoded.returns.first().quantity == 3,
        "Build cancellation return plan did not round-trip");

    auto duplicate = RemoteBuildMutationDto::toMetadata(QStringLiteral("builds.cancel"), cancel);
    QJsonArray rows = duplicate.mutation.value(QStringLiteral("returns")).toArray();
    rows.append(rows.first()); duplicate.mutation.insert(QStringLiteral("returns"), rows);
    ok &= require(!RemoteBuildMutationDto::fromMetadata(QStringLiteral("builds.cancel"),
        duplicate, &decoded, &error), "Duplicate cancellation return row was accepted");

    auto stale = RemoteBuildMutationDto::toMetadata(QStringLiteral("builds.complete"), existingRequest());
    stale.expected.insert(QStringLiteral("modifiedUtc"), QStringLiteral("not-a-date"));
    ok &= require(!RemoteBuildMutationDto::fromMetadata(QStringLiteral("builds.complete"),
        stale, &decoded, &error), "Invalid optimistic-concurrency timestamp was accepted");

    RemoteBuildMutationDto::Request requirement;
    requirement.workspaceId=4;requirement.buildId=9;requirement.mutationId=RemoteMutationDto::newMutationId();
    requirement.partNumber=QStringLiteral("3001");requirement.rebrickableColorId=5;
    requirement.substitutePartNumber=QStringLiteral("3001old");requirement.substituteRebrickableColorId=1;
    requirement.quantityRequired=7;requirement.spare=true;
    ok &= require(RemoteBuildMutationDto::fromMetadata(QStringLiteral("builds.requirements.add"),RemoteBuildMutationDto::toMetadata(QStringLiteral("builds.requirements.add"),requirement),&decoded,&error)&&decoded.partNumber==requirement.partNumber&&decoded.rebrickableColorId==5&&decoded.quantityRequired==7&&decoded.spare,"Requirement Add portable identity did not round-trip");
    requirement.requirementId=21;requirement.expectedRequirement={21,9,QStringLiteral("2026-09-11T12:30:00.000Z"),QStringLiteral("3001"),5,QStringLiteral("3001old"),1,7,0,0,true};
    ok &= require(RemoteBuildMutationDto::fromMetadata(QStringLiteral("builds.requirements.edit"),RemoteBuildMutationDto::toMetadata(QStringLiteral("builds.requirements.edit"),requirement),&decoded,&error)&&decoded.expectedRequirement.requirementId==21&&decoded.substitutePartNumber==QStringLiteral("3001old"),"Requirement Edit expected state did not round-trip");
    auto noSubstitution=requirement;
    noSubstitution.substitutePartNumber.clear();noSubstitution.substituteRebrickableColorId=-1;
    noSubstitution.expectedRequirement.substitutePartNumber.clear();
    noSubstitution.expectedRequirement.substituteRebrickableColorId=-1;
    ok &= require(RemoteBuildMutationDto::fromMetadata(QStringLiteral("builds.requirements.edit"),RemoteBuildMutationDto::toMetadata(QStringLiteral("builds.requirements.edit"),noSubstitution),&decoded,&error)
        && decoded.expectedRequirement.modifiedUtc==noSubstitution.expectedRequirement.modifiedUtc
        && decoded.expectedRequirement.substitutePartNumber.isEmpty()
        && decoded.expectedRequirement.substituteRebrickableColorId==-1,
        "Unsubstituted requirement expected state did not preserve its canonical sentinel");
    ok &= require(RemoteBuildMutationDto::fromMetadata(QStringLiteral("builds.requirements.remove"),RemoteBuildMutationDto::toMetadata(QStringLiteral("builds.requirements.remove"),requirement),&decoded,&error),"Requirement Remove expected state did not round-trip");
    requirement.allocations={{31,41,3,2,QStringLiteral("2026-09-11T12:31:00.000Z"),QStringLiteral("2026-09-11T12:32:00.000Z"),10},{0,42,1,0,QString(),QStringLiteral("2026-09-11T12:32:00.000Z"),5}};
    ok &= require(RemoteBuildMutationDto::fromMetadata(QStringLiteral("builds.allocations.set"),RemoteBuildMutationDto::toMetadata(QStringLiteral("builds.allocations.set"),requirement),&decoded,&error)&&decoded.allocations.size()==2&&decoded.allocations.first().expectedQuantity==2,"Allocation-set state did not round-trip");
    auto duplicateAlloc=RemoteBuildMutationDto::toMetadata(QStringLiteral("builds.allocations.set"),requirement);auto allocationRows=duplicateAlloc.mutation.value("allocations").toArray();allocationRows.append(allocationRows.first());duplicateAlloc.mutation["allocations"]=allocationRows;
    ok &= require(!RemoteBuildMutationDto::fromMetadata(QStringLiteral("builds.allocations.set"),duplicateAlloc,&decoded,&error),"Duplicate allocation Inventory row was accepted");
    auto automatic=existingRequest();automatic.preferredStorageId=12;
    ok &= require(RemoteBuildMutationDto::fromMetadata(QStringLiteral("builds.allocateAvailable"),RemoteBuildMutationDto::toMetadata(QStringLiteral("builds.allocateAvailable"),automatic),&decoded,&error)&&decoded.preferredStorageId==12,"Allocate Available request did not round-trip");

    qInfo() << "Remote Build mutation DTO tests passed.";
    return ok ? 0 : 1;
}
