#include "../src/database/DatabaseManager.h"
#include "../src/models/ExternalMappingStatus.h"
#include "../src/models/procurement/BrickLinkWantedListOptions.h"
#include "../src/models/procurement/ProcurementDraft.h"
#include "../src/repositories/ExternalPartIdentifierRepository.h"
#include "../src/repositories/ExternalPartMappingRepository.h"
#include "../src/repositories/PartRepository.h"
#include "../src/services/mappings/BrickLinkPartResolver.h"
#include "../src/services/procurement/BrickLinkWantedListXmlWriter.h"

#include <QCoreApplication>
#include <QDir>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QXmlStreamReader>

namespace
{
bool require(bool condition, const QString& message)
{
    if (!condition)
        qCritical().noquote() << message;
    return condition;
}

class Cleanup
{
public:
    explicit Cleanup(QString path) : m_path(std::move(path)) {}
    ~Cleanup()
    {
        DatabaseManager::instance().close();
        QDir(m_path).removeRecursively();
    }

private:
    QString m_path;
};

int partId(const QString& partNumber)
{
    const auto part = PartRepository().getByPartNumber(partNumber);
    return part ? part->id() : 0;
}

bool insertPart(QSqlQuery& query, const QString& partNumber)
{
    query.prepare(R"(
        INSERT INTO part
            (part_number, name, is_active, created_utc, modified_utc, material)
        VALUES
            (:part_number, :name, 1, :created_utc, :modified_utc, 'Plastic')
    )");
    query.bindValue(":part_number", partNumber);
    query.bindValue(":name", QStringLiteral("Test %1").arg(partNumber));
    query.bindValue(":created_utc", QStringLiteral("2026-01-01T00:00:00.000Z"));
    query.bindValue(":modified_utc", QStringLiteral("2026-01-01T00:00:00.000Z"));
    return query.exec();
}

ProcurementItem readyItem(const QString& itemId, const QString& colorId, int quantity)
{
    ProcurementItem item;
    item.partNumber = QStringLiteral("source");
    item.resolvedItemId = itemId;
    item.resolvedItemReady = true;
    item.resolvedColorId = colorId;
    item.resolvedColorReady = true;
    item.quantityNeeded = quantity;
    return item;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("RFStateSideTests"));
    QCoreApplication::setApplicationName(
        QStringLiteral("BrickLinkProcurement_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));

    const QString data = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    Cleanup cleanup(data);

    if (!require(DatabaseManager::instance().initialize(),
                 QStringLiteral("Database initialization failed.")))
        return 1;

    QSqlQuery query(DatabaseManager::instance().database());
    for (const QString& number : {
             QStringLiteral("32064b"),
             QStringLiteral("28621pr4116"),
             QStringLiteral("different"),
             QStringLiteral("same"),
             QStringLiteral("unique-identifier"),
             QStringLiteral("ambiguous"),
             QStringLiteral("user"),
             QStringLiteral("unknown"),
             QStringLiteral("unsupported")}) {
        if (!require(insertPart(query, number),
                     QStringLiteral("Unable to seed Part %1.").arg(number)))
            return 1;
    }

    BrickLinkPartResolver resolver;
    auto result = resolver.resolve(partId(QStringLiteral("32064b")),
                                   QStringLiteral("32064b"));
    if (!require(!result.canExport && result.itemId.isEmpty()
                     && result.status == BrickLinkPartResolver::ResolutionStatus::NotResolved,
                 QStringLiteral("A never-enriched canonical Part was treated as terminal.")))
        return 1;

    result = resolver.resolve(partId(QStringLiteral("28621pr4116")),
                              QStringLiteral("28621pr4116"));
    if (!require(!result.canExport && result.itemId.isEmpty()
                     && result.status == BrickLinkPartResolver::ResolutionStatus::NotResolved,
                 QStringLiteral("A decorated Part identity was guessed.")))
        return 1;

    ExternalPartIdentifierRepository identifiers;
    const int decoratedId = partId(QStringLiteral("28621pr4116"));
    if (!require(identifiers.setLookupStatus(
                     decoratedId,
                     QStringLiteral("Rebrickable"),
                     QStringLiteral("Unavailable")),
                 QStringLiteral("Unavailable lookup status seed failed.")))
        return 1;
    result = resolver.resolve(decoratedId, QStringLiteral("28621pr4116"));
    if (!require(!result.canExport && result.itemId.isEmpty()
                     && result.status == BrickLinkPartResolver::ResolutionStatus::Unavailable,
                 QStringLiteral("A terminal unavailable lookup remained retry-eligible.")))
        return 1;

    ExternalPartMappingRepository mappings;
    ExternalPartMapping mapping;
    mapping.provider = QStringLiteral("BrickLink");
    mapping.status = ExternalMappingStatus::Mapped;
    mapping.source = QStringLiteral("Rebrickable");

    mapping.partId = partId(QStringLiteral("different"));
    mapping.externalId = QStringLiteral("bricklink-different");
    if (!require(mappings.upsert(mapping), QStringLiteral("Differing mapping seed failed.")))
        return 1;
    result = resolver.resolve(mapping.partId, QStringLiteral("different"));
    if (!require(result.canExport && result.itemId == QStringLiteral("bricklink-different"),
                 QStringLiteral("Authoritative differing mapping was not used.")))
        return 1;

    mapping.partId = partId(QStringLiteral("same"));
    mapping.externalId = QStringLiteral("same");
    if (!require(mappings.upsert(mapping), QStringLiteral("Equal mapping seed failed.")))
        return 1;
    result = resolver.resolve(mapping.partId, QStringLiteral("same"));
    if (!require(result.canExport && result.itemId == QStringLiteral("same"),
                 QStringLiteral("Authoritative equal mapping was not used.")))
        return 1;

    const int uniqueId = partId(QStringLiteral("unique-identifier"));
    if (!require(identifiers.replaceProviderIds(
                     uniqueId,
                     {{QStringLiteral("BrickLink"), {QStringLiteral("unique-bl")}}},
                     QStringLiteral("Rebrickable")),
                 QStringLiteral("Unique identifier seed failed.")))
        return 1;
    result = resolver.resolve(uniqueId, QStringLiteral("unique-identifier"));
    if (!require(result.canExport && result.itemId == QStringLiteral("unique-bl")
                     && result.status == BrickLinkPartResolver::ResolutionStatus::ExternalId,
                 QStringLiteral("Unique authoritative identifier was not used.")))
        return 1;

    const int ambiguousId = partId(QStringLiteral("ambiguous"));
    if (!require(identifiers.replaceProviderIds(
                     ambiguousId,
                     {{QStringLiteral("BrickLink"),
                       {QStringLiteral("ambiguous-a"), QStringLiteral("ambiguous-b")}}},
                     QStringLiteral("Rebrickable")),
                 QStringLiteral("Ambiguous identifier seed failed.")))
        return 1;
    result = resolver.resolve(ambiguousId, QStringLiteral("ambiguous"));
    if (!require(!result.canExport && result.itemId.isEmpty()
                     && result.status == BrickLinkPartResolver::ResolutionStatus::Ambiguous,
                 QStringLiteral("Multiple authoritative identifiers were guessed.")))
        return 1;

    mapping.partId = partId(QStringLiteral("user"));
    mapping.externalId = QStringLiteral("user-confirmed");
    mapping.source = QStringLiteral("User");
    if (!require(mappings.upsert(mapping), QStringLiteral("User mapping seed failed.")))
        return 1;
    if (!require(identifiers.replaceProviderIds(
                     mapping.partId,
                     {{QStringLiteral("BrickLink"), {QStringLiteral("provider-value")}}},
                     QStringLiteral("Rebrickable")),
                 QStringLiteral("Provider identifier seed failed.")))
        return 1;
    result = resolver.resolve(mapping.partId, QStringLiteral("user"));
    if (!require(result.canExport && result.itemId == QStringLiteral("user-confirmed")
                     && result.status == BrickLinkPartResolver::ResolutionStatus::UserOverride,
                 QStringLiteral("User-confirmed mapping did not retain precedence.")))
        return 1;

    for (const auto& unresolved : {
             qMakePair(QStringLiteral("unknown"), ExternalMappingStatus::Unknown),
             qMakePair(QStringLiteral("unsupported"), ExternalMappingStatus::Unsupported)}) {
        mapping.partId = partId(unresolved.first);
        mapping.externalId.clear();
        mapping.status = unresolved.second;
        mapping.source = QStringLiteral("Rebrickable");
        if (!require(mappings.upsert(mapping), QStringLiteral("Unresolved mapping seed failed.")))
            return 1;
        result = resolver.resolve(mapping.partId, unresolved.first);
        if (!require(!result.canExport && result.itemId.isEmpty(),
                     QStringLiteral("Unknown/Unsupported mapping became exportable.")))
            return 1;
    }

    BrickLinkWantedListXmlWriter writer;
    BrickLinkWantedListOptions options;
    options.condition = QStringLiteral("N");
    options.remarksMode = QStringLiteral("Custom");
    options.customRemarks = QStringLiteral("A&B <test>");

    ProcurementDraft unresolvedDraft;
    ProcurementItem unresolvedItem;
    unresolvedItem.quantityNeeded = 1;
    unresolvedItem.resolvedColorId = QStringLiteral("5");
    unresolvedItem.resolvedColorReady = true;
    unresolvedDraft.items.append(unresolvedItem);
    if (!require(!writer.write(unresolvedDraft, options).success,
                 QStringLiteral("An unresolved item produced XML.")))
        return 1;

    ProcurementDraft sessionOverrideDraft;
    ProcurementItem sessionOverride;
    sessionOverride.itemOverride = QStringLiteral("manual-bricklink-id");
    sessionOverride.itemOverrideActive = true;
    sessionOverride.resolvedColorId = QStringLiteral("5");
    sessionOverride.resolvedColorReady = true;
    sessionOverride.quantityNeeded = 1;
    sessionOverrideDraft.items.append(sessionOverride);
    const auto sessionOverrideResult = writer.write(sessionOverrideDraft, {});
    if (!require(sessionOverrideResult.success
                     && sessionOverrideResult.xml.contains(
                         QStringLiteral("<ITEMID>manual-bricklink-id</ITEMID>")),
                 QStringLiteral("A valid session override was not exported.")))
        return 1;

    ProcurementItem pendingOverride;
    pendingOverride.itemOverride = QStringLiteral("manual-during-lookup");
    pendingOverride.itemOverrideActive = true;
    pendingOverride.resolvedItemId = QStringLiteral("automatic-provider-id");
    pendingOverride.resolvedItemReady = true;
    if (!require(pendingOverride.effectiveItemId()
                     == QStringLiteral("manual-during-lookup"),
                 QStringLiteral("A late automatic identity replaced a session override.")))
        return 1;
    pendingOverride.itemOverride.clear();
    pendingOverride.itemOverrideActive = false;
    if (!require(pendingOverride.effectiveItemId()
                     == QStringLiteral("automatic-provider-id"),
                 QStringLiteral("Clearing an override did not expose the automatic identity.")))
        return 1;

    ProcurementDraft draft;
    draft.items = {
        readyItem(QStringLiteral("shared-id"), QStringLiteral("5"), 2),
        readyItem(QStringLiteral("SHARED-ID"), QStringLiteral("05"), 3),
        readyItem(QStringLiteral("shared-id"), QStringLiteral("7"), 4)
    };
    const auto xmlResult = writer.write(draft, options);
    if (!require(xmlResult.success && xmlResult.itemRows == 2
                     && xmlResult.totalPieces == 9,
                 QStringLiteral("Provider-level aggregation totals are incorrect.")))
        return 1;
    if (!require(xmlResult.xml.count(QStringLiteral("<ITEMID>")) == 2
                     && xmlResult.xml.contains(QStringLiteral("<MINQTY>5</MINQTY>"))
                     && xmlResult.xml.contains(QStringLiteral("<MINQTY>4</MINQTY>"))
                     && xmlResult.xml.contains(QStringLiteral("<ITEMTYPE>P</ITEMTYPE>"))
                     && xmlResult.xml.contains(QStringLiteral("A&amp;B &lt;test&gt;")),
                 QStringLiteral("Serialized BrickLink XML content is incorrect.")))
        return 1;

    qInfo() << "Authoritative BrickLink procurement validation passed.";
    return 0;
}
