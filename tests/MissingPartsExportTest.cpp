#include "../src/services/builds/MissingPartsCsvWriter.h"
#include "../src/services/builds/MissingPartsExportService.h"
#include "../src/services/builds/PickABrickCsvWriter.h"
#include "../src/services/builds/PickABrickExportService.h"
#include "../src/ui/builds/MissingPartsExportDialog.h"
#include "../src/ui/builds/MissingPartsExportPreparationState.h"

#include <QApplication>
#include <QComboBox>
#include <QSettings>
#include <QLineEdit>
#include <QTableWidget>
#include <QTemporaryDir>

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
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);
    QTemporaryDir settingsDirectory;
    if (!check(settingsDirectory.isValid(), "temporary settings directory")) return 1;
    QCoreApplication::setOrganizationName(QStringLiteral("BrickSuiteM35Test"));
    QCoreApplication::setApplicationName(QStringLiteral("MissingPartsExportTest"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       settingsDirectory.path());
    bool ok = true;

    MissingPartsExportPreparationState preparation;
    ok &= check(preparation.begin() && preparation.active(),
                "first export preparation did not start");
    ok &= check(!preparation.begin(),
                "duplicate export preparation was not suppressed");
    preparation.finish();
    ok &= check(!preparation.active() && preparation.begin(),
                "export preparation did not reset after completion");
    preparation.finish();
    ok &= check(preparation.begin(),
                "export preparation did not reset after a failure completion");
    preparation.finish();

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
    remotePart.pickABrickElementCandidates = {QStringLiteral("1234567"),
                                               QStringLiteral("7654321")};
    remotePart.rebrickablePartId = QStringLiteral("3001");
    remotePart.brickLinkPartIds = {QStringLiteral("BL-3001")};
    remotePart.brickLinkColorId = QStringLiteral("5");
    const auto remoteRows = MissingPartsExportService::createRemoteRows(remoteBuild, {remotePart});
    ok &= check(remoteRows.first().category == QStringLiteral("Host Bricks")
                    && remoteRows.first().manufacturer == QStringLiteral("Host Manufacturer")
                    && remoteRows.first().legoElementIds == remotePart.legoElementIds
                    && remoteRows.first().pickABrickElementCandidates
                        == remotePart.pickABrickElementCandidates,
                "Host-authoritative remote metadata was not preserved");

    remotePart.categoryName.clear();
    remotePart.legoElementIds.clear();
    remotePart.pickABrickElementCandidates.clear();
    remotePart.brickLinkPartIds.clear();
    const auto legacyRows = MissingPartsExportService::createRemoteRows(remoteBuild, {remotePart});
    ok &= check(legacyRows.first().partNumber == QStringLiteral("3001")
                    && legacyRows.first().missing == 3
                    && legacyRows.first().category.isEmpty(),
                "older Host payload did not retain basic export fields");

    MissingPartsExportRow unique = row;
    unique.missing = 2;
    unique.legoElementIds = {QStringLiteral("1234567")};
    unique.pickABrickElementCandidates = unique.legoElementIds;
    MissingPartsExportRow multiple = row;
    multiple.partNumber = QStringLiteral("3002");
    multiple.missing = 3;
    multiple.legoElementIds = {QStringLiteral("999999"), QStringLiteral("1000000"),
                               QStringLiteral("7654321"), QStringLiteral("2000000"),
                               QStringLiteral("1234567")};
    multiple.pickABrickElementCandidates = multiple.legoElementIds;
    MissingPartsExportRow unresolved = row;
    unresolved.partNumber = QStringLiteral("3003");
    unresolved.missing = 4;
    unresolved.legoElementIds.clear();
    unresolved.pickABrickElementCandidates.clear();
    auto pickRows = PickABrickExportService::createSourceRows({unique, multiple, unresolved});
    ok &= check(pickRows.at(0).selectedElementId == QStringLiteral("1234567")
                    && pickRows.at(1).selectedElementId == QStringLiteral("7654321")
                    && pickRows.at(1).elementCandidates
                        == QStringList({QStringLiteral("999999"), QStringLiteral("1000000"),
                                        QStringLiteral("1234567"), QStringLiteral("2000000"),
                                        QStringLiteral("7654321")})
                    && pickRows.at(2).selectedElementId.isEmpty(),
                "unique/multiple/unresolved Element candidate policy failed");
    auto pickProjection = PickABrickExportService::project(pickRows);
    ok &= check(!pickProjection.ready() && pickProjection.unresolvedIncludedRows == 1,
                "included unresolved row did not block Pick a Brick export");

    pickRows[2].included = false;
    pickRows[1].selectedElementId = QStringLiteral("1234567");
    pickProjection = PickABrickExportService::project(pickRows);
    ok &= check(pickProjection.ready() && pickProjection.excludedSourceRows == 1
                    && pickProjection.excludedPieces == 4
                    && pickProjection.rows.size() == 1
                    && pickProjection.rows.first().elementId == QStringLiteral("1234567")
                    && pickProjection.rows.first().quantity == 5
                    && pickProjection.includedPieces == 5,
                "override/exclusion/post-selection aggregation failed");
    const auto pickCsv = PickABrickCsvWriter::generate(pickProjection);
    ok &= check(pickCsv.success
                    && pickCsv.csv == QByteArray("elementId,quantity\r\n1234567,5\r\n")
                    && !pickCsv.csv.startsWith(QByteArray("\xEF\xBB\xBF")),
                "exact Pick a Brick CSV format failed");

    multiple.legoElementIds = {QStringLiteral("2000000"), QStringLiteral("3000000")};
    multiple.pickABrickElementCandidates = multiple.legoElementIds;
    multiple.missing = 1;
    auto differentRows = PickABrickExportService::createSourceRows({unique, multiple});
    const auto differentProjection = PickABrickExportService::project(differentRows);
    ok &= check(differentProjection.ready() && differentProjection.rows.size() == 2
                    && differentProjection.rows.at(0).elementId == QStringLiteral("1234567")
                    && differentProjection.rows.at(1).elementId == QStringLiteral("3000000"),
                "first-source deterministic target order failed");

    MissingPartsExportRow invalidQuantity = unique;
    invalidQuantity.missing = 0;
    const auto invalidProjection = PickABrickExportService::project(
        PickABrickExportService::createSourceRows({invalidQuantity}));
    ok &= check(!invalidProjection.ready() && !invalidProjection.error.isEmpty(),
                "non-positive included quantity was accepted");

    const auto generalAfterPick = MissingPartsExportService::project({row}, custom);
    ok &= check(generalAfterPick.headers == projection.headers
                    && generalAfterPick.rows == projection.rows,
                "Pick a Brick projection changed General CSV configuration");

    MissingPartsExportDialog dialog({unique, multiple, unresolved},
                                    QStringLiteral("missing.csv"));
    auto* preset = dialog.findChild<QComboBox*>(QStringLiteral("missingPartsExportPreset"));
    auto* controls = dialog.findChild<QWidget*>(QStringLiteral("missingPartsGeneralControls"));
    auto* preview = dialog.findChild<QTableWidget*>(QStringLiteral("missingPartsExportPreview"));
    ok &= check(preset && controls && preview && preset->currentText() == QStringLiteral("General CSV"),
                "General CSV was not the initial preset");
    preset->setCurrentIndex(1);
    ok &= check(controls->isHidden() && preview->columnCount() == 7
                    && preview->horizontalHeaderItem(5)->text() == QStringLiteral("Element ID"),
                "Pick a Brick preset did not switch to its fixed source preview");
    preset->setCurrentIndex(0);
    ok &= check(!controls->isHidden() && preview->horizontalHeaderItem(0)->text() == QStringLiteral("Build"),
                "switching back did not restore General CSV preview/configuration");

    MissingPartsExportRow unresolvedOverride = unresolved;
    unresolvedOverride.partNumber = QStringLiteral("32064b");
    unresolvedOverride.partName = QStringLiteral("Technic Brick Type 2");
    unresolvedOverride.colorName = QStringLiteral("White");
    unresolvedOverride.rebrickableColorId = 15;
    unresolvedOverride.missing = 4;
    QString requestedPart;
    int requestedColor = -1;
    MissingPartsExportDialog overrideDialog({unresolvedOverride},
        QStringLiteral("override.csv"), nullptr,
        [&](const QString& partNumber, int colorId, QObject*, auto completion) {
            requestedPart = partNumber;
            requestedColor = colorId;
            PickABrickPartResolution result;
            if (partNumber == QStringLiteral("32064")) {
                result.partFound = true;
                result.partNumber = partNumber;
                result.partName = QStringLiteral("Technic Brick 1 x 2 with Axle Hole");
                result.elementCandidates = {QStringLiteral("100"), QStringLiteral("900")};
            } else if (partNumber == QStringLiteral("valid-no-element")) {
                result.partFound = true;
                result.partNumber = partNumber;
                result.partName = QStringLiteral("Valid Part Without White Element");
            }
            completion(result);
        });
    auto* overridePreset = overrideDialog.findChild<QComboBox*>(
        QStringLiteral("missingPartsExportPreset"));
    overridePreset->setCurrentIndex(1);
    auto* overridePreview = overrideDialog.findChild<QTableWidget*>(
        QStringLiteral("missingPartsExportPreview"));
    auto* partOverride = overrideDialog.findChild<QLineEdit*>(
        QStringLiteral("pickABrickPartOverride_0"));
    ok &= check(partOverride && overridePreview
                    && overridePreview->item(0, 6)->text().contains(QStringLiteral("no exact")),
                "original unresolved Part was not blocked");
    partOverride->setText(QStringLiteral(" 32064 "));
    partOverride->setModified(true);
    QMetaObject::invokeMethod(partOverride, "editingFinished", Qt::DirectConnection);
    QCoreApplication::sendPostedEvents(); app.processEvents();
    auto* overrideCandidates = qobject_cast<QComboBox*>(overridePreview->cellWidget(0, 5));
    ok &= check(requestedPart == QStringLiteral("32064") && requestedColor == 15
                    && overridePreview->item(0, 2)->text()
                        == QStringLiteral("Technic Brick 1 x 2 with Axle Hole")
                    && overridePreview->item(0, 4)->text() == QStringLiteral("4")
                    && overrideCandidates && overrideCandidates->currentText() == QStringLiteral("900")
                    && overridePreview->item(0, 6)->text().contains(QStringLiteral("Part override")),
                "exact Part override did not preserve Color/quantity or candidate policy");

    partOverride = overrideDialog.findChild<QLineEdit*>(
        QStringLiteral("pickABrickPartOverride_0"));
    partOverride->setText(QStringLiteral("32064-base-guess"));
    partOverride->setModified(true);
    QMetaObject::invokeMethod(partOverride, "editingFinished", Qt::DirectConnection);
    QCoreApplication::sendPostedEvents(); app.processEvents();
    ok &= check(requestedPart == QStringLiteral("32064-base-guess")
                    && overridePreview->item(0, 6)->text().contains(QStringLiteral("Part not found")),
                "invalid override was guessed or did not remain unresolved");

    partOverride = overrideDialog.findChild<QLineEdit*>(
        QStringLiteral("pickABrickPartOverride_0"));
    partOverride->setText(QStringLiteral("valid-no-element"));
    partOverride->setModified(true);
    QMetaObject::invokeMethod(partOverride, "editingFinished", Qt::DirectConnection);
    QCoreApplication::sendPostedEvents(); app.processEvents();
    ok &= check(overridePreview->item(0, 6)->text().contains(QStringLiteral("no exact")),
                "valid Part without exact Color Element was not unresolved");

    partOverride = overrideDialog.findChild<QLineEdit*>(
        QStringLiteral("pickABrickPartOverride_0"));
    partOverride->clear();
    partOverride->setModified(true);
    QMetaObject::invokeMethod(partOverride, "editingFinished", Qt::DirectConnection);
    QCoreApplication::sendPostedEvents(); app.processEvents();
    QCoreApplication::sendPostedEvents(); app.processEvents();
    partOverride = overrideDialog.findChild<QLineEdit*>(
        QStringLiteral("pickABrickPartOverride_0"));
    ok &= check(partOverride->text() == QStringLiteral("32064b")
                    && overridePreview->item(0, 2)->text() == unresolvedOverride.partName
                    && overridePreview->item(0, 6)->text().contains(QStringLiteral("no exact")),
                "clearing the override did not restore the original source identity");

    PickABrickExportSourceRow overriddenCollision;
    overriddenCollision.source = unresolvedOverride;
    overriddenCollision.elementCandidates = {QStringLiteral("900")};
    overriddenCollision.selectedElementId = QStringLiteral("900");
    overriddenCollision.overridePartNumber = QStringLiteral("32064");
    PickABrickExportSourceRow existingTarget = overriddenCollision;
    existingTarget.source.partNumber = QStringLiteral("other");
    existingTarget.source.missing = 3;
    existingTarget.overridePartNumber.clear();
    const auto collision = PickABrickExportService::project(
        {overriddenCollision, existingTarget});
    ok &= check(collision.ready() && collision.rows.size() == 1
                    && collision.rows.first().quantity == 7,
                "Part override Element collision did not aggregate missing quantity");
    return ok ? 0 : 1;
}
