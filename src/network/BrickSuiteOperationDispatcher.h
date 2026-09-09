#pragma once

#include "BrickSuiteProtocol.h"

#include <QHash>
#include <functional>

class BrickSuiteOperationDispatcher
{
public:
    using Handler = std::function<QJsonObject(const QJsonObject&)>;

    BrickSuiteOperationDispatcher();
    void registerOperation(const QString& name, bool authenticationRequired,
                           Handler handler);
    BrickSuiteProtocol::Message dispatch(
        const BrickSuiteProtocol::Message& request, bool authenticated) const;
    QStringList operations() const;

private:
    struct Registration {
        bool authenticationRequired = true;
        Handler handler;
    };
    QHash<QString, Registration> m_operations;
};
