#pragma once

#include <QDateTime>
#include <QString>

struct RemoteMutationReceipt
{
    QString mutationId;
    QString operation;
    qint64 workspaceId = 0;
    QString requestHash;
    QString resultCode;
    QString resultJson;
    QDateTime committedUtc;
    QString clientIdentity;
};
