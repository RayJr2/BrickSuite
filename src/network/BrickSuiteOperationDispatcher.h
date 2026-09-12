#pragma once

#include "BrickSuiteProtocol.h"

#include <QHash>
#include <functional>

class BrickSuiteOperationDispatcher
{
public:
    enum class AdmissionKind { None, Read, Write };
    using Handler = std::function<QJsonObject(const QJsonObject&)>;
    using Completion = std::function<void(BrickSuiteProtocol::Message)>;
    using AsyncHandler = std::function<void(const BrickSuiteProtocol::Message&, Completion)>;

    BrickSuiteOperationDispatcher();
    void setDataEpoch(const QString& epoch) { m_dataEpoch = epoch; }
    void registerOperation(const QString& name, bool authenticationRequired,
                           Handler handler);
    void registerAsyncOperation(const QString& name, bool authenticationRequired,
                                AsyncHandler handler, int minimumMinor = 0,
                                const QString& capability = {},
                                AdmissionKind admissionKind = AdmissionKind::Read);
    AdmissionKind admissionKind(const QString& operation) const;
    BrickSuiteProtocol::Message dispatch(
        const BrickSuiteProtocol::Message& request, bool authenticated) const;
    void dispatchAsync(const BrickSuiteProtocol::Message& request, bool authenticated,
                       Completion completion) const;
    QStringList operations() const;
    QStringList operations(int negotiatedMinor) const;
    QStringList capabilities(int negotiatedMinor) const;

private:
    struct Registration {
        bool authenticationRequired = true;
        Handler handler;
        AsyncHandler asyncHandler;
        int minimumMinor = 0;
        QString capability;
        AdmissionKind admissionKind = AdmissionKind::None;
    };
    QHash<QString, Registration> m_operations;
    QString m_dataEpoch;
};
