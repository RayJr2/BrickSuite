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

#include "RemoteInventoryMutationApplicationService.h"
#include "RemoteMutationApplicationServices.h"
#include <QDebug>

RemoteInventoryMutationApplicationService::RemoteInventoryMutationApplicationService(
    RemoteMutationApplicationServices& mutations, QObject* parent)
    : QObject(parent), m_mutations(mutations) {}

bool RemoteInventoryMutationApplicationService::isAvailableFor(const QString& operation) const
{ return m_mutations.isAvailableFor(operation, operation); }

QString RemoteInventoryMutationApplicationService::submit(
    const QString& operation, const RemoteInventoryMutationDto::Request& request,
    QObject* context, Completion completion, Failure failure)
{
    const Failure decodeFailure=failure;
    return m_mutations.submit(operation, operation,
        RemoteInventoryMutationDto::toMetadata(request), context,
        [operation, completion=std::move(completion), failure=decodeFailure](const RemoteMutationDto::Result& source) {
            RemoteInventoryMutationDto::Result result; RemoteMutationDto::Error error;
            if (!RemoteInventoryMutationDto::resultFromMutation(source, &result, &error)) {
                error.outcome=RemoteMutationDto::Outcome::Unknown;
                error.mutationId=source.mutationId;
                qWarning() << "Remote Inventory mutation result could not be decoded"
                           << operation << source.mutationId.left(8) << error.message;
                if (failure) failure(error);
            } else {
                qDebug() << "Remote Inventory mutation result accepted" << operation
                         << source.mutationId.left(8) << "replayed" << source.replayed;
                if (completion) completion(result);
            }
        }, std::move(failure));
}

#define INVENTORY_METHOD(name, operation) \
QString RemoteInventoryMutationApplicationService::name( \
    const RemoteInventoryMutationDto::Request& request, QObject* context, \
    Completion completion, Failure failure) \
{ return submit(QStringLiteral(operation), request, context, std::move(completion), std::move(failure)); }

INVENTORY_METHOD(add, "inventory.add")
INVENTORY_METHOD(edit, "inventory.edit")
INVENTORY_METHOD(move, "inventory.move")
INVENTORY_METHOD(correct, "inventory.correct")
INVENTORY_METHOD(remove, "inventory.remove")
INVENTORY_METHOD(markLost, "inventory.markLost")
INVENTORY_METHOD(markFound, "inventory.markFound")
#undef INVENTORY_METHOD
