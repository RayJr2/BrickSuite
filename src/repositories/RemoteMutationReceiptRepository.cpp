#include "RemoteMutationReceiptRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
QString repositoryError(const QSqlError& error)
{
    const QString details = QStringLiteral("%1 %2 %3")
                                .arg(error.nativeErrorCode(), error.driverText(), error.databaseText());
    const QString nativeCode = error.nativeErrorCode().trimmed();
    const bool busy = details.contains(QStringLiteral("locked"), Qt::CaseInsensitive)
                      || details.contains(QStringLiteral("busy"), Qt::CaseInsensitive)
                      || nativeCode == QStringLiteral("5")
                      || nativeCode == QStringLiteral("6");
    return busy ? QStringLiteral("SQLITE_BUSY") : error.text();
}
}

std::optional<RemoteMutationReceipt> RemoteMutationReceiptRepository::find(
    const QString& mutationId, QString* error) const
{
    QSqlQuery query(repositoryDatabase());
    if (!query.prepare(QStringLiteral("SELECT mutation_id,operation,workspace_id,request_hash,"
                                      "result_code,result_json,committed_utc,client_identity "
                                      "FROM remote_mutation_receipt WHERE mutation_id=:id"))) {
        if (error) *error = repositoryError(query.lastError());
        return std::nullopt;
    }
    query.bindValue(QStringLiteral(":id"), mutationId);
    if (!query.exec()) {
        if (error) *error = repositoryError(query.lastError());
        return std::nullopt;
    }
    if (!query.next()) return std::nullopt;
    RemoteMutationReceipt value;
    value.mutationId = query.value(0).toString();
    value.operation = query.value(1).toString();
    value.workspaceId = query.value(2).toLongLong();
    value.requestHash = query.value(3).toString();
    value.resultCode = query.value(4).toString();
    value.resultJson = query.value(5).toString();
    value.committedUtc = QDateTime::fromString(query.value(6).toString(), Qt::ISODateWithMs);
    value.clientIdentity = query.value(7).toString();
    return value;
}

bool RemoteMutationReceiptRepository::insert(const RemoteMutationReceipt& receipt,
                                               QString* error) const
{
    QSqlQuery query(repositoryDatabase());
    if (!query.prepare(QStringLiteral("INSERT INTO remote_mutation_receipt "
        "(mutation_id,operation,workspace_id,request_hash,result_code,result_json,"
        "committed_utc,client_identity) VALUES (:id,:operation,:workspace,:hash,:code,"
        ":json,:utc,:client)"))) {
        if (error) *error = repositoryError(query.lastError());
        return false;
    }
    query.bindValue(QStringLiteral(":id"), receipt.mutationId);
    query.bindValue(QStringLiteral(":operation"), receipt.operation);
    query.bindValue(QStringLiteral(":workspace"), receipt.workspaceId);
    query.bindValue(QStringLiteral(":hash"), receipt.requestHash);
    query.bindValue(QStringLiteral(":code"), receipt.resultCode);
    query.bindValue(QStringLiteral(":json"), receipt.resultJson);
    query.bindValue(QStringLiteral(":utc"), receipt.committedUtc.toUTC().toString(Qt::ISODateWithMs));
    query.bindValue(QStringLiteral(":client"), receipt.clientIdentity.left(128));
    if (query.exec()) return true;
    if (error) *error = repositoryError(query.lastError());
    return false;
}

int RemoteMutationReceiptRepository::removeCommittedBefore(const QDateTime& cutoffUtc,
                                                             int maximumRows,
                                                             QString* error) const
{
    if (maximumRows <= 0) return 0;
    QSqlQuery query(repositoryDatabase());
    if (!query.prepare(QStringLiteral("DELETE FROM remote_mutation_receipt WHERE mutation_id IN "
        "(SELECT mutation_id FROM remote_mutation_receipt WHERE committed_utc < :cutoff "
        "ORDER BY committed_utc LIMIT :limit)"))) {
        if (error) *error = repositoryError(query.lastError());
        return -1;
    }
    query.bindValue(QStringLiteral(":cutoff"), cutoffUtc.toUTC().toString(Qt::ISODateWithMs));
    query.bindValue(QStringLiteral(":limit"), maximumRows);
    if (!query.exec()) {
        if (error) *error = repositoryError(query.lastError());
        return -1;
    }
    return query.numRowsAffected();
}
