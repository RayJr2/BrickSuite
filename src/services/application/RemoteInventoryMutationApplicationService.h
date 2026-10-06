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

#include "dto/RemoteInventoryMutationDtos.h"

#include <QObject>
#include <functional>

class RemoteMutationApplicationServices;

class RemoteInventoryMutationApplicationService : public QObject
{
public:
    using Completion = std::function<void(const RemoteInventoryMutationDto::Result&)>;
    using Failure = std::function<void(const RemoteMutationDto::Error&)>;

    explicit RemoteInventoryMutationApplicationService(RemoteMutationApplicationServices& mutations,
                                                        QObject* parent = nullptr);
    bool isAvailableFor(const QString& operation) const;
    QString add(const RemoteInventoryMutationDto::Request&, QObject*, Completion, Failure);
    QString edit(const RemoteInventoryMutationDto::Request&, QObject*, Completion, Failure);
    QString move(const RemoteInventoryMutationDto::Request&, QObject*, Completion, Failure);
    QString correct(const RemoteInventoryMutationDto::Request&, QObject*, Completion, Failure);
    QString remove(const RemoteInventoryMutationDto::Request&, QObject*, Completion, Failure);
    QString markLost(const RemoteInventoryMutationDto::Request&, QObject*, Completion, Failure);
    QString markFound(const RemoteInventoryMutationDto::Request&, QObject*, Completion, Failure);

private:
    QString submit(const QString&, const RemoteInventoryMutationDto::Request&, QObject*, Completion, Failure);
    RemoteMutationApplicationServices& m_mutations;
};
