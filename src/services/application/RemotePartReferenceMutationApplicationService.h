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

#include "dto/RemotePartReferenceMutationDtos.h"
#include <QObject>
#include <functional>

class RemoteMutationApplicationServices;
class RemotePartReferenceMutationApplicationService : public QObject
{
public:
    using Completion=std::function<void(const RemotePartReferenceMutationDto::Result&)>;
    using Failure=std::function<void(const RemoteMutationDto::Error&)>;
    explicit RemotePartReferenceMutationApplicationService(RemoteMutationApplicationServices&,
                                                            QObject* parent=nullptr);
    bool isAvailableFor(const QString& operation) const;
    QString add(const RemotePartReferenceMutationDto::Request&,QObject*,Completion,Failure);
    QString remove(const RemotePartReferenceMutationDto::Request&,QObject*,Completion,Failure);
private:
    QString submit(const QString&,const RemotePartReferenceMutationDto::Request&,QObject*,Completion,Failure);
    RemoteMutationApplicationServices& m_mutations;
};
