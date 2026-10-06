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

#include "RemoteBuildMutationApplicationService.h"
#include "RemoteMutationApplicationServices.h"

RemoteBuildMutationApplicationService::RemoteBuildMutationApplicationService(
    RemoteMutationApplicationServices& mutations,QObject* parent)
    : QObject(parent),m_mutations(mutations) {}

bool RemoteBuildMutationApplicationService::isAvailableFor(const QString& operation) const
{ return m_mutations.isAvailableFor(operation,operation); }

QString RemoteBuildMutationApplicationService::submit(
    const QString& operation,const RemoteBuildMutationDto::Request& request,QObject* context,
    Completion completion,Failure failure)
{
    const auto fallback=failure;
    return m_mutations.submit(operation,operation,
        RemoteBuildMutationDto::toMetadata(operation,request),context,
        [completion=std::move(completion),failure=fallback](const RemoteMutationDto::Result& source){
            RemoteBuildMutationDto::Result result;RemoteMutationDto::Error error;
            if(!RemoteBuildMutationDto::resultFromMutation(source,&result,&error)){
                error.outcome=RemoteMutationDto::Outcome::Unknown;error.mutationId=source.mutationId;
                if(failure)failure(error);
            }else if(completion)completion(result);
        },std::move(failure));
}
