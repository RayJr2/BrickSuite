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

    qInfo() << "Remote Build mutation DTO tests passed.";
    return ok ? 0 : 1;
}
