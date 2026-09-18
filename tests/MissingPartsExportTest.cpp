#include "../src/services/builds/MissingPartsCsvWriter.h"
#include "../src/services/builds/MissingPartsExportService.h"

#include <QCoreApplication>

#include <cstdio>

namespace {
bool check(bool value, const char* message)
{
    if (!value) std::fprintf(stderr, "FAILED: %s\n", message);
    return value;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;

    const auto defaults = MissingPartsExportService::defaultConfiguration();
    const QStringList expectedOrder = {
        "build", "buildReference", "partNumber", "partName", "color",
        "required", "pulled", "remaining", "available", "quantityMissing"
    };
    QStringList enabledDefaultOrder;
    for (const QString& id : defaults.fieldOrder)
        if (defaults.enabledFields.contains(id)) enabledDefaultOrder.append(id);
    ok &= check(enabledDefaultOrder == expectedOrder,
                "historical ten-field default order changed");

    MissingPartsExportRow row;
    row.buildName = QStringLiteral("Unicode ★ Build");
    row.buildReference = QStringLiteral("MOC-1");
    row.partNumber = QStringLiteral("3001pr0001");
    row.partName = QStringLiteral("Brick, \"Printed\"");
    row.category = QStringLiteral("Bricks");
    row.colorName = QStringLiteral("Bright Red");
    row.missing = 3;
    row.manufacturer = QStringLiteral("LEGO");
    row.legoElementIds = {QStringLiteral("111"), QStringLiteral("222")};
    row.rebrickablePartId = QStringLiteral("3001pr0001");
    row.rebrickableColorId = 4;
    row.brickLinkPartIds = {QStringLiteral("3001pb001"), QStringLiteral("3001pb002")};
    row.brickLinkColorId = QStringLiteral("5");
    row.required = 10;
    row.pulled = 2;
    row.remaining = 8;
    row.available = 5;

    const auto defaultProjection = MissingPartsExportService::project({row}, defaults);
    ok &= check(defaultProjection.headers == QStringList({
                    "Build", "Set Number", "Part Number", "Part Name", "Color",
                    "Required", "Pulled", "Remaining", "Available", "Missing"}),
                "historical CSV headers changed");

    MissingPartsExportConfiguration custom = defaults;
    custom.enabledFields = {"legoElementId", "brickLinkPartId", "partName", "manufacturer"};
    custom.fieldOrder = {"brickLinkPartId", "legoElementId", "partName", "manufacturer"};
    const auto projection = MissingPartsExportService::project({row}, custom);
    ok &= check(projection.headers == QStringList({"BrickLink Part ID", "LEGO Element ID",
                                                   "Part Name", "Manufacturer"})
                    && projection.rows.first().at(0) == QStringLiteral("3001pb001;3001pb002")
                    && projection.rows.first().at(1) == QStringLiteral("111;222"),
                "field selection/order or multi-identity representation failed");
    const auto csv = MissingPartsCsvWriter::generate(projection);
    ok &= check(csv.success && csv.csv.startsWith(QChar(0xFEFF))
                    && csv.csv.contains(QStringLiteral("\"Brick, \"\"Printed\"\"\"")),
                "CSV BOM/escaping failed");

    MissingPartsExportConfiguration reordered;
    reordered.enabledFields = {"quantityMissing", "build"};
    reordered.fieldOrder = {"quantityMissing", "build"};
    const auto reorderedProjection = MissingPartsExportService::project({row}, reordered);
    const auto reorderedCsv = MissingPartsCsvWriter::generate(reorderedProjection);
    ok &= check(reorderedProjection.headers == QStringList({"Missing", "Build"})
                    && reorderedProjection.rows.first()
                        == QStringList({"3", QStringLiteral("Unicode ★ Build")})
                    && reorderedCsv.csv.contains(QStringLiteral("3,\"Unicode ★ Build\"")),
                "preview/export projection parity failed");

    const auto normalized = MissingPartsExportService::normalizeConfiguration(
        {"unknown", "color", "build"}, {"unknown", "color"});
    ok &= check(!normalized.fieldOrder.contains("unknown")
                    && !normalized.enabledFields.contains("unknown")
                    && normalized.fieldOrder.first() == QStringLiteral("color")
                    && normalized.fieldOrder.at(1) == QStringLiteral("build"),
                "unknown saved field IDs were not ignored safely");

    RemoteReadDto::BuildDetail remoteBuild;
    remoteBuild.name = QStringLiteral("Host Build");
    remoteBuild.setNumber = QStringLiteral("HOST-1");
    remoteBuild.manufacturerDisplay = QStringLiteral("Host Manufacturer");
    RemoteReadDto::MissingPart remotePart;
    remotePart.partNumber = QStringLiteral("3001");
    remotePart.partNameFallback = QStringLiteral("Host Brick");
    remotePart.rebrickableColorId = 4;
    remotePart.colorNameFallback = QStringLiteral("Host Red");
    remotePart.required = 10;
    remotePart.remaining = 8;
    remotePart.available = 5;
    remotePart.missing = 3;
    remotePart.categoryName = QStringLiteral("Host Bricks");
    remotePart.legoElementIds = {QStringLiteral("host-1"), QStringLiteral("host-2")};
    remotePart.rebrickablePartId = QStringLiteral("3001");
    remotePart.brickLinkPartIds = {QStringLiteral("BL-3001")};
    remotePart.brickLinkColorId = QStringLiteral("5");
    const auto remoteRows = MissingPartsExportService::createRemoteRows(remoteBuild, {remotePart});
    ok &= check(remoteRows.first().category == QStringLiteral("Host Bricks")
                    && remoteRows.first().manufacturer == QStringLiteral("Host Manufacturer")
                    && remoteRows.first().legoElementIds == remotePart.legoElementIds,
                "Host-authoritative remote metadata was not preserved");

    remotePart.categoryName.clear();
    remotePart.legoElementIds.clear();
    remotePart.brickLinkPartIds.clear();
    const auto legacyRows = MissingPartsExportService::createRemoteRows(remoteBuild, {remotePart});
    ok &= check(legacyRows.first().partNumber == QStringLiteral("3001")
                    && legacyRows.first().missing == 3
                    && legacyRows.first().category.isEmpty(),
                "older Host payload did not retain basic export fields");
    return ok ? 0 : 1;
}
