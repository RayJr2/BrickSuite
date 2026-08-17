#include "WorkspaceRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool WorkspaceRepository::create(Workspace& workspace)
{
    QSqlDatabase database =
        DatabaseManager::instance().database();

    if (!database.isOpen())
    {
        qCritical() << "WorkspaceRepository: database is not open.";
        return false;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();

    QSqlQuery query(database);

    query.prepare(R"(
        INSERT INTO workspace
        (
            name,
            description,
            created_utc,
            modified_utc,
            is_active
        )
        VALUES
        (
            :name,
            :description,
            :created_utc,
            :modified_utc,
            :is_active
        )
    )");

    query.bindValue(":name", workspace.name());
    query.bindValue(":description", workspace.description());
    query.bindValue(
        ":created_utc",
        now.toString(Qt::ISODateWithMs));
    query.bindValue(
        ":modified_utc",
        now.toString(Qt::ISODateWithMs));
    query.bindValue(
        ":is_active",
        workspace.isActive() ? 1 : 0);

    if (!query.exec())
    {
        qCritical()
            << "Unable to create workspace:"
            << query.lastError().text();

        return false;
    }

    workspace.setId(
        query.lastInsertId().toInt());

    workspace.setCreatedUtc(now);
    workspace.setModifiedUtc(now);

    qInfo() << "Workspace created."
            << "WorkspaceId:" << workspace.id()
            << "Name:" << workspace.name();

    return true;
}

QList<Workspace> WorkspaceRepository::getAll() const
{
    QList<Workspace> workspaces;

    QSqlDatabase database =
        DatabaseManager::instance().database();

    QSqlQuery query(database);

    if (!query.exec(R"(
        SELECT
            id,
            name,
            description,
            created_utc,
            modified_utc,
            is_active
        FROM workspace
        ORDER BY name
    )"))
    {
        qCritical()
            << "Unable to retrieve workspaces:"
            << query.lastError().text();

        return workspaces;
    }

    while (query.next())
    {
        workspaces.append(
            workspaceFromQuery(query));
    }

    return workspaces;
}

std::optional<Workspace>
WorkspaceRepository::getById(int id) const
{
    QSqlDatabase database =
        DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            name,
            description,
            created_utc,
            modified_utc,
            is_active
        FROM workspace
        WHERE id = :id
    )");

    query.bindValue(":id", id);

    if (!query.exec())
    {
        qCritical()
            << "Unable to retrieve workspace:"
            << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return workspaceFromQuery(query);
}

bool WorkspaceRepository::update(
    const Workspace& workspace)
{
    QSqlDatabase database =
        DatabaseManager::instance().database();

    if (!database.isOpen()) {
        qCritical() << "Workspace update failed: database is not open."
                    << "WorkspaceId:" << workspace.id();
        return false;
    }

    const QDateTime now =
        QDateTime::currentDateTimeUtc();

    QSqlQuery query(database);

    query.prepare(R"(
        UPDATE workspace
        SET
            name = :name,
            description = :description,
            modified_utc = :modified_utc,
            is_active = :is_active
        WHERE id = :id
    )");

    query.bindValue(
        ":name",
        workspace.name());

    query.bindValue(
        ":description",
        workspace.description());

    query.bindValue(
        ":modified_utc",
        now.toString(Qt::ISODateWithMs));

    query.bindValue(
        ":is_active",
        workspace.isActive() ? 1 : 0);

    query.bindValue(
        ":id",
        workspace.id());

    if (!query.exec())
    {
        qCritical()
            << "Unable to update workspace:"
            << query.lastError().text();

        return false;
    }

    return query.numRowsAffected() > 0;
}

Workspace WorkspaceRepository::workspaceFromQuery(
    const QSqlQuery& query) const
{
    Workspace workspace;

    workspace.setId(
        query.value("id").toInt());

    workspace.setName(
        query.value("name").toString());

    workspace.setDescription(
        query.value("description").toString());

    workspace.setCreatedUtc(
        QDateTime::fromString(
            query.value("created_utc").toString(),
            Qt::ISODateWithMs));

    workspace.setModifiedUtc(
        QDateTime::fromString(
            query.value("modified_utc").toString(),
            Qt::ISODateWithMs));

    workspace.setIsActive(
        query.value("is_active").toInt() != 0);

    return workspace;
}