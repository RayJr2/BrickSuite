/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "BrickSuiteProtocol.h"
#include "HostRequestContext.h"

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
    void dispatchAsync(const BrickSuiteProtocol::Message& request, bool authenticated,
                       const HostRequestContext& context, Completion completion) const;
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
