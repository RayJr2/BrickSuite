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

#include "RemotePartReferenceMutationApplicationService.h"
#include "RemoteMutationApplicationServices.h"

RemotePartReferenceMutationApplicationService::RemotePartReferenceMutationApplicationService(
    RemoteMutationApplicationServices& mutations,QObject* parent)
    : QObject(parent),m_mutations(mutations) {}
bool RemotePartReferenceMutationApplicationService::isAvailableFor(const QString& operation) const
{ return m_mutations.isAvailableFor(operation,operation); }
QString RemotePartReferenceMutationApplicationService::submit(
    const QString& operation,const RemotePartReferenceMutationDto::Request& request,QObject* context,
    Completion completion,Failure failure)
{
    const auto decodeFailure=failure;
    return m_mutations.submit(operation,operation,
        RemotePartReferenceMutationDto::toMetadata(operation,request),context,
        [completion=std::move(completion),failure=decodeFailure](const RemoteMutationDto::Result& source) {
            RemotePartReferenceMutationDto::Result result; RemoteMutationDto::Error error;
            if (!RemotePartReferenceMutationDto::resultFromMutation(source,&result,&error)) {
                error.outcome=RemoteMutationDto::Outcome::Unknown; error.mutationId=source.mutationId;
                if (failure) failure(error);
            } else if (completion) completion(result);
        },std::move(failure));
}
QString RemotePartReferenceMutationApplicationService::add(
    const RemotePartReferenceMutationDto::Request&r,QObject*c,Completion d,Failure f)
{ return submit(QStringLiteral("partReference.customizations.add"),r,c,std::move(d),std::move(f)); }
QString RemotePartReferenceMutationApplicationService::remove(
    const RemotePartReferenceMutationDto::Request&r,QObject*c,Completion d,Failure f)
{ return submit(QStringLiteral("partReference.customizations.remove"),r,c,std::move(d),std::move(f)); }
