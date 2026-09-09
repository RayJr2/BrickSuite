/* BrickSuite - The Digital Twin Platform for Your Brick Workshop */
#include "PartReferenceCustomizationService.h"
#include "../../database/DatabaseManager.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/UserPartReferenceRepository.h"
#include <QHash>
#include <QSet>
#include <algorithm>
#include <functional>
#include <QSqlDatabase>
#include <QThread>

namespace {
QString key(const QString& value) { return value.trimmed().toLower(); }
QString destinationKey(const QString& catalog, const QString& section)
{ return key(catalog) + QChar(0x1f) + key(section); }
}

PartReferenceCustomizationService::PartReferenceCustomizationService(const PartReferenceManifest& manifest)
    : PartReferenceCustomizationService(manifest, DatabaseManager::instance().database()) {}

PartReferenceCustomizationService::PartReferenceCustomizationService(
    const PartReferenceManifest& manifest, const QSqlDatabase& database)
    : m_manifest(manifest), m_connectionName(database.connectionName()),
      m_ownerThread(QThread::currentThread())
{
    Q_ASSERT(database.isValid());
}

QSqlDatabase PartReferenceCustomizationService::serviceDatabase() const
{
    Q_ASSERT(QThread::currentThread() == m_ownerThread);
    return QSqlDatabase::database(m_connectionName, false);
}

bool PartReferenceCustomizationService::isStructuredCatalog(const QString& catalog)
{
    static const QSet<QString> names = {
        QStringLiteral("bricks"), QStringLiteral("plates"), QStringLiteral("tiles"),
        QStringLiteral("technic beams & liftarms")
    };
    return names.contains(key(catalog));
}

QList<PartReferenceDestination> PartReferenceCustomizationService::destinations() const
{
    QList<PartReferenceDestination> result;
    QSet<QString> seen;
    for (const PartReferenceEntry& entry : m_manifest.entries()) {
        const QString id = destinationKey(entry.catalog, entry.section);
        if (seen.contains(id)) continue;
        seen.insert(id);
        result.append({entry.catalog, entry.section, isStructuredCatalog(entry.catalog)});
    }
    return result;
}

QList<PartReferenceEntry> PartReferenceCustomizationService::effectiveEntries(QString* errorMessage) const
{
    QList<PartReferenceEntry> builtIns = m_manifest.entries();
    QSet<QString> builtInParts;
    QSet<QString> validDestinations;
    for (const PartReferenceEntry& entry : builtIns) {
        builtInParts.insert(key(entry.partNumber));
        validDestinations.insert(destinationKey(entry.catalog, entry.section));
    }

    bool loaded = false;
    const QList<UserPartReferenceEntry> stored =
        UserPartReferenceRepository(serviceDatabase()).getAll(&loaded);
    if (!loaded) {
        if (errorMessage) *errorMessage = QStringLiteral("Unable to load Part Reference customizations.");
        return builtIns;
    }

    QHash<QString, QList<PartReferenceEntry>> usersByDestination;
    QHash<QString, UserPartReferenceEntry> placementByPart;
    PartRepository parts(serviceDatabase());
    for (const UserPartReferenceEntry& storedEntry : stored) {
        if (!validDestinations.contains(destinationKey(storedEntry.catalog, storedEntry.section))) {
            qWarning() << "Suppressing user Part Reference entry with invalid destination:" << storedEntry.id;
            continue;
        }
        const auto part = parts.getById(storedEntry.partId);
        if (!part || !part->isActive()) {
            qWarning() << "Suppressing user Part Reference entry with unavailable Part:" << storedEntry.id;
            continue;
        }
        if (builtInParts.contains(key(part->partNumber())))
            continue; // A future built-in definition wins; the user row remains intact.

        PartReferenceEntry entry;
        entry.userEntryId = storedEntry.id; entry.partId = part->id();
        entry.origin = PartReferenceEntry::Origin::User;
        entry.partNumber = part->partNumber(); entry.partName = part->name();
        entry.catalog = storedEntry.catalog; entry.section = storedEntry.section;
        entry.material = part->material();
        usersByDestination[destinationKey(entry.catalog, entry.section)].append(entry);
        placementByPart.insert(key(entry.partNumber), storedEntry);
    }

    QList<PartReferenceEntry> result;
    for (const QString& catalog : m_manifest.catalogs()) {
        QList<PartReferenceEntry> catalogEntries = m_manifest.entriesForCatalog(catalog);
        QStringList sections;
        for (const PartReferenceEntry& entry : catalogEntries)
            if (!sections.contains(entry.section)) sections.append(entry.section);
        for (const QString& section : sections) {
            QList<PartReferenceEntry> roots;
            QHash<QString, PartReferenceEntry> nodes;
            for (const PartReferenceEntry& entry : catalogEntries) {
                if (entry.section == section) { roots.append(entry); nodes.insert(key(entry.partNumber), entry); }
            }
            const QList<PartReferenceEntry> users = usersByDestination.value(destinationKey(catalog, section));
            for (const PartReferenceEntry& entry : users) nodes.insert(key(entry.partNumber), entry);

            QHash<QString, QList<PartReferenceEntry>> before;
            QHash<QString, QList<PartReferenceEntry>> after;
            QList<PartReferenceEntry> appended;
            for (const PartReferenceEntry& entry : users) {
                const UserPartReferenceEntry placement = placementByPart.value(key(entry.partNumber));
                const QString anchor = key(placement.anchorPartNumber);
                const bool usableAnchor = !anchor.isEmpty() && nodes.contains(anchor)
                    && destinationKey(nodes.value(anchor).catalog, nodes.value(anchor).section)
                           == destinationKey(catalog, section);
                if (!usableAnchor || placement.placement == PartReferencePlacement::Append
                    || isStructuredCatalog(catalog)) {
                    appended.append(entry);
                } else if (placement.placement == PartReferencePlacement::Before) {
                    before[anchor].append(entry);
                } else {
                    after[anchor].append(entry);
                }
            }
            QSet<QString> emitted;
            std::function<void(const PartReferenceEntry&)> emitEntry = [&](const PartReferenceEntry& entry) {
                const QString partKey = key(entry.partNumber);
                if (emitted.contains(partKey)) return;
                emitted.insert(partKey);
                for (const PartReferenceEntry& child : before.value(partKey)) emitEntry(child);
                result.append(entry);
                for (const PartReferenceEntry& child : after.value(partKey)) emitEntry(child);
            };
            for (const PartReferenceEntry& root : roots) emitEntry(root);
            for (const PartReferenceEntry& entry : appended) emitEntry(entry);
            // Cycles or otherwise unusable chains safely fall back to append.
            for (const PartReferenceEntry& entry : users) emitEntry(entry);
        }
    }
    return result;
}

PartReferenceCustomizationResult PartReferenceCustomizationService::add(
    int partId, const QString& catalog, const QString& section,
    PartReferencePlacement placement, const QString& anchorPartNumber) const
{
    const auto part = PartRepository(serviceDatabase()).getById(partId);
    if (!part || !part->isActive())
        return {false, QStringLiteral("Select an active local catalog Part."), 0};

    const QList<PartReferenceEntry> effective = effectiveEntries();
    for (const PartReferenceEntry& entry : effective) {
        if (key(entry.partNumber) == key(part->partNumber())) {
            return {false, QStringLiteral("Part %1 is already in Part Reference (%2 / %3).")
                               .arg(part->partNumber(), entry.catalog, entry.section), 0};
        }
    }
    bool validDestination = false;
    for (const PartReferenceDestination& destination : destinations()) {
        if (destinationKey(destination.catalog, destination.section)
            == destinationKey(catalog, section)) { validDestination = true; break; }
    }
    if (!validDestination)
        return {false, QStringLiteral("The selected Part Reference destination is no longer valid."), 0};

    if (isStructuredCatalog(catalog)) placement = PartReferencePlacement::Append;
    if (placement != PartReferencePlacement::Append) {
        bool validAnchor = false;
        for (const PartReferenceEntry& entry : effective) {
            if (key(entry.partNumber) == key(anchorPartNumber)
                && destinationKey(entry.catalog, entry.section) == destinationKey(catalog, section)) {
                validAnchor = true; break;
            }
        }
        if (!validAnchor)
            return {false, QStringLiteral("Choose an existing entry in the selected family as the anchor."), 0};
    }

    UserPartReferenceEntry entry;
    entry.partId = partId; entry.catalog = catalog; entry.section = section;
    entry.placement = placement; entry.anchorPartNumber = anchorPartNumber;
    QString error;
    if (!UserPartReferenceRepository(serviceDatabase()).create(entry, &error))
        return {false, QStringLiteral("Unable to save the Part Reference customization: %1").arg(error), 0};
    return {true, QStringLiteral("Part %1 was added to Part Reference.").arg(part->partNumber()), entry.id};
}

PartReferenceCustomizationResult PartReferenceCustomizationService::remove(int userEntryId) const
{
    if (userEntryId <= 0)
        return {false, QStringLiteral("Built-in Part Reference entries cannot be removed."), 0};
    QString error;
    if (!UserPartReferenceRepository(serviceDatabase()).remove(userEntryId, &error))
        return {false, QStringLiteral("Unable to remove the customization: %1").arg(error), 0};
    return {true, QStringLiteral("The user Part Reference entry was removed."), userEntryId};
}
