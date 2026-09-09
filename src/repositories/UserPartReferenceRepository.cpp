/* BrickSuite - The Digital Twin Platform for Your Brick Workshop */
#include "UserPartReferenceRepository.h"
#include "../database/DatabaseManager.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
QString placementName(PartReferencePlacement value)
{
    if (value == PartReferencePlacement::Before) return QStringLiteral("Before");
    if (value == PartReferencePlacement::After) return QStringLiteral("After");
    return QStringLiteral("Append");
}
PartReferencePlacement placementFromName(const QString& value)
{
    if (value == QStringLiteral("Before")) return PartReferencePlacement::Before;
    if (value == QStringLiteral("After")) return PartReferencePlacement::After;
    return PartReferencePlacement::Append;
}
}

QList<UserPartReferenceEntry> UserPartReferenceRepository::getAll(bool* ok) const
{
    if (ok) *ok = false;
    QList<UserPartReferenceEntry> result;
    QSqlQuery query(repositoryDatabase());
    if (!query.exec("SELECT id,part_id,catalog,section,anchor_part_number,placement_mode,"
                    "created_utc,modified_utc FROM user_part_reference_entry ORDER BY id")) {
        qCritical() << "Unable to load user Part Reference entries:" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        UserPartReferenceEntry entry;
        entry.id = query.value(0).toInt(); entry.partId = query.value(1).toInt();
        entry.catalog = query.value(2).toString(); entry.section = query.value(3).toString();
        entry.anchorPartNumber = query.value(4).toString();
        entry.placement = placementFromName(query.value(5).toString());
        entry.createdUtc = QDateTime::fromString(query.value(6).toString(), Qt::ISODateWithMs);
        entry.modifiedUtc = QDateTime::fromString(query.value(7).toString(), Qt::ISODateWithMs);
        result.append(entry);
    }
    if (ok) *ok = true;
    return result;
}

bool UserPartReferenceRepository::create(UserPartReferenceEntry& entry, QString* errorMessage) const
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QSqlQuery query(repositoryDatabase());
    query.prepare("INSERT INTO user_part_reference_entry(part_id,catalog,section,anchor_part_number,"
                  "placement_mode,created_utc,modified_utc) VALUES(?,?,?,?,?,?,?)");
    query.addBindValue(entry.partId); query.addBindValue(entry.catalog.trimmed());
    query.addBindValue(entry.section.trimmed());
    query.addBindValue(entry.anchorPartNumber.trimmed().isEmpty() ? QVariant() : entry.anchorPartNumber.trimmed());
    query.addBindValue(placementName(entry.placement));
    query.addBindValue(now.toString(Qt::ISODateWithMs)); query.addBindValue(now.toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        qCritical() << "Unable to create user Part Reference entry:" << query.lastError().text();
        return false;
    }
    entry.id = query.lastInsertId().toInt(); entry.createdUtc = now; entry.modifiedUtc = now;
    return true;
}

bool UserPartReferenceRepository::remove(int id, QString* errorMessage) const
{
    QSqlQuery query(repositoryDatabase());
    query.prepare("DELETE FROM user_part_reference_entry WHERE id=?"); query.addBindValue(id);
    if (!query.exec() || query.numRowsAffected() != 1) {
        if (errorMessage) *errorMessage = query.lastError().text().isEmpty()
            ? QStringLiteral("The customization no longer exists.") : query.lastError().text();
        return false;
    }
    return true;
}
