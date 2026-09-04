#include "../src/database/DatabaseManager.h"
#include "../src/models/PartRelationship.h"
#include "../src/repositories/ExternalPartIdentifierRepository.h"
#include "../src/repositories/PartRelationshipRepository.h"
#include "../src/repositories/PartRepository.h"
#include "../src/services/parts/BrickLinkCandidateDiscoveryService.h"
#include "../src/services/parts/PartResolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QUuid>

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

Part createPart(const QString& number)
{
    Part part;
    part.setPartNumber(number);
    part.setName(number);
    part.setMaterial(QStringLiteral("Plastic"));
    part.setIsActive(true);
    PartRepository().create(part);
    return part;
}

bool addRelationship(const Part& parent, const Part& child)
{
    PartRelationship relationship;
    relationship.parentPartId = parent.id();
    relationship.childPartId = child.id();
    relationship.relationshipType = PartRelationshipType::Print;
    relationship.sourceRelationshipType = QStringLiteral("P");
    relationship.source = QStringLiteral("Test");
    return PartRelationshipRepository().upsert(relationship);
}

bool contains(const QList<Part>& parts, const QString& number)
{
    for (const Part& part : parts) {
        if (part.partNumber() == number)
            return true;
    }
    return false;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("RFStateSideTests"));
    QCoreApplication::setApplicationName(
        QStringLiteral("BrickLinkCandidates_")
        + QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString dataPath =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    Cleanup cleanup(dataPath);
    if (!require(DatabaseManager::instance().initialize(),
                 QStringLiteral("Database initialization failed."))) return 1;

    BrickLinkCandidateDiscoveryService discovery;
    const Part base15625 = createPart(QStringLiteral("15625"));
    QList<Part> family15625;
    for (int index = 1; index <= 17; ++index) {
        const Part child = createPart(
            QStringLiteral("15625pr%1").arg(index, 4, 10, QLatin1Char('0')));
        family15625.append(child);
        if (!require(addRelationship(base15625, child),
                     QStringLiteral("Unable to seed 15625 relationship."))) return 1;
    }

    const auto known = discovery.discover(QStringLiteral("15625pb025"));
    if (!require(known.status == BrickLinkCandidateDiscoveryService::Status::Candidates
                 && known.baseHint == QStringLiteral("15625")
                 && known.candidates.size() == 17
                 && contains(known.candidates, QStringLiteral("15625pr0015")),
                 QStringLiteral("15625 candidate discovery failed."))) return 1;
    if (!require(BrickLinkCandidateDiscoveryService::baseHint(
                     QStringLiteral("15625pb025")) == QStringLiteral("15625"),
                 QStringLiteral("BrickLink base parsing failed."))) return 1;

    const Part relationBase = createPart(QStringLiteral("relbase"));
    const Part relationshipOnly = createPart(QStringLiteral("decorated-other-name"));
    if (!require(addRelationship(relationBase, relationshipOnly),
                 QStringLiteral("Unable to seed relationship-only candidate."))) return 1;
    const auto relationshipResult = discovery.discover(QStringLiteral("relbasepb01"));
    if (!require(relationshipResult.candidates.size() == 1
                 && contains(relationshipResult.candidates, relationshipOnly.partNumber()),
                 QStringLiteral("Relationship-only discovery failed."))) return 1;

    createPart(QStringLiteral("prefixbase"));
    createPart(QStringLiteral("prefixbasepr0001"));
    const auto prefixResult = discovery.discover(QStringLiteral("prefixbasepb99"));
    if (!require(prefixResult.candidates.size() == 1
                 && contains(prefixResult.candidates, QStringLiteral("prefixbasepr0001")),
                 QStringLiteral("Prefix-only discovery failed."))) return 1;

    // Every 15625 child is present through both sources but appears once.
    if (!require(known.candidates.size() == 17,
                 QStringLiteral("Combined candidates were not deduplicated."))) return 1;

    createPart(QStringLiteral("broad"));
    for (int index = 1; index <= 21; ++index)
        createPart(QStringLiteral("broadpr%1").arg(index, 4, 10, QLatin1Char('0')));
    if (!require(discovery.discover(QStringLiteral("broadpb01")).status
                     == BrickLinkCandidateDiscoveryService::Status::TooBroad,
                 QStringLiteral("Over-limit family was not rejected."))) return 1;

    if (!require(discovery.discover(QStringLiteral("nonepb01")).status
                     == BrickLinkCandidateDiscoveryService::Status::NoCandidates,
                 QStringLiteral("Zero-candidate result was incorrect."))) return 1;
    for (const QString& unsupported : {QStringLiteral("15625"), QStringLiteral("pb025"),
                                       QStringLiteral("15625pr0015"), QStringLiteral("bad pb1")}) {
        if (!require(discovery.discover(unsupported).status
                         == BrickLinkCandidateDiscoveryService::Status::Unsupported,
                     QStringLiteral("Malformed ID was accepted: ") + unsupported)) return 1;
    }

    createPart(QStringLiteral("3070bpr0001"));
    if (!require(discovery.discover(QStringLiteral("3070pb104")).status
                     == BrickLinkCandidateDiscoveryService::Status::NoCandidates,
                 QStringLiteral("3070b was invented from the 3070 hint."))) return 1;

    const Part direct = createPart(QStringLiteral("direct-1"));
    const auto directResult = PartResolver().resolve(QStringLiteral("direct-1"));
    if (!require(directResult.hasResolvedPart && directResult.part.id() == direct.id(),
                 QStringLiteral("Direct Rebrickable resolution changed."))) return 1;

    ExternalPartIdentifierRepository identifiers;
    const Part target = family15625.at(14);
    const auto checkboxOff = PartResolver().resolve(QStringLiteral("15625pb025"));
    if (!require(!checkboxOff.hasResolvedPart,
                 QStringLiteral("Normal checkbox-off Part resolution unexpectedly changed."))) return 1;
    if (!require(identifiers.findByProviderAndExternalId(
                     QStringLiteral("BrickLink"), QStringLiteral("15625pb025")).isEmpty(),
                 QStringLiteral("Candidate hint became authoritative prematurely."))) return 1;
    if (!require(identifiers.replaceProviderIds(
                     target.id(), {{QStringLiteral("BrickLink"), {QStringLiteral("15625pb025")}}},
                     QStringLiteral("Rebrickable")),
                 QStringLiteral("Simulated enrichment persistence failed."))) return 1;
    const auto exact = identifiers.findByProviderAndExternalId(
        QStringLiteral("BrickLink"), QStringLiteral("15625pb025"));
    if (!require(exact.size() == 1 && exact.first().partId == target.id(),
                 QStringLiteral("Exact authoritative match failed after enrichment."))) return 1;

    const Part ambiguous = createPart(QStringLiteral("ambiguouspr0001"));
    if (!require(identifiers.replaceProviderIds(
                     ambiguous.id(), {{QStringLiteral("BrickLink"), {QStringLiteral("15625pb025")}}},
                     QStringLiteral("Rebrickable")),
                 QStringLiteral("Unable to seed ambiguity."))) return 1;
    if (!require(identifiers.findByProviderAndExternalId(
                     QStringLiteral("BrickLink"), QStringLiteral("15625pb025")).size() == 2,
                 QStringLiteral("Authoritative ambiguity was hidden."))) return 1;

    BrickLinkCandidateLookupSession session;
    session.begin(QStringLiteral("15625pb025"), {target.id(), ambiguous.id()});
    if (!require(session.accepts(QStringLiteral("15625PB025"), target.id())
                 && !session.accepts(QStringLiteral("changedpb01"), target.id())
                 && !session.accepts(QStringLiteral("15625pb025"), direct.id()),
                 QStringLiteral("Pending lookup accepted stale or unrelated completion."))) return 1;
    if (!require(!session.finish(target.id()) && session.finish(ambiguous.id()),
                 QStringLiteral("Pending lookup completion tracking failed."))) return 1;
    session.clear();
    if (!require(!session.isActive()
                 && !session.accepts(QStringLiteral("15625pb025"), target.id()),
                 QStringLiteral("Closed/cleared lookup accepted a late completion."))) return 1;

    qInfo() << "Uncached BrickLink candidate resolution validation passed.";
    return 0;
}
