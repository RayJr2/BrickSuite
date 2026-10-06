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

#include "HostWriteExecutor.h"

#include <QObject>
#include <functional>

class BrickSuiteOperationDispatcher;

// Domain-neutral registration boundary. Production operational mutations are
// intentionally not registered until their application services exist.
class HostMutationProtocolService : public QObject
{
public:
    explicit HostMutationProtocolService(const QString& databasePath,
                                          HostWriteExecutor::Publisher publisher = {},
                                          const QString& dataEpoch = QString(),
                                          QObject* parent = nullptr);
    HostWriteExecutor& executor();

    void registerInternalOperation(BrickSuiteOperationDispatcher& dispatcher,
        const QString& operation, const QString& capability,
        HostWriteExecutor::Mutation mutation);
    using MutationFactory = std::function<HostWriteExecutor::Mutation(
        const RemoteMutationDto::Metadata&, RemoteMutationDto::Error*)>;
    void registerOperation(BrickSuiteOperationDispatcher& dispatcher,
        const QString& operation, const QString& capability, MutationFactory factory,
        int minimumMinor = 2);

private:
    HostWriteExecutor m_executor;
    QString m_dataEpoch;
};
