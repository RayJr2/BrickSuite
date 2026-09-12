#pragma once

#include "dto/RemoteMutationDtos.h"

#include <QObject>
#include <QHash>
#include <functional>

class BrickSuiteWebSocketClient;

class RemoteMutationApplicationServices : public QObject
{
public:
    explicit RemoteMutationApplicationServices(BrickSuiteWebSocketClient& client,
                                                QObject* parent = nullptr);
    bool isAvailableFor(const QString& operation, const QString& capability) const;
    QString submit(const QString& operation, const QString& capability,
                   const RemoteMutationDto::Metadata& metadata, QObject* context,
                   std::function<void(const RemoteMutationDto::Result&)> completion,
                   std::function<void(const RemoteMutationDto::Error&)> failure);

private:
    BrickSuiteWebSocketClient& m_client;
    QHash<QString, QString> m_epochByUnknownMutation;
};
