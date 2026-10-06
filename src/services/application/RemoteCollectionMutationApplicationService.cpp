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

#include "RemoteCollectionMutationApplicationService.h"
#include "RemoteMutationApplicationServices.h"
RemoteCollectionMutationApplicationService::RemoteCollectionMutationApplicationService(RemoteMutationApplicationServices&m,QObject*p):QObject(p),m_mutations(m){}
bool RemoteCollectionMutationApplicationService::isAvailableFor(const QString&o)const{return m_mutations.isAvailableFor(o,o);}
QString RemoteCollectionMutationApplicationService::submit(const QString&o,const RemoteCollectionMutationDto::Request&r,QObject*c,Completion done,Failure failed){auto fallback=failed;return m_mutations.submit(o,o,RemoteCollectionMutationDto::toMetadata(o,r),c,[done=std::move(done),failed=fallback](const RemoteMutationDto::Result&s){RemoteCollectionMutationDto::Result x;RemoteMutationDto::Error e;if(!RemoteCollectionMutationDto::resultFromMutation(s,&x,&e)){e.outcome=RemoteMutationDto::Outcome::DefinitiveFailure;e.mutationId=s.mutationId;if(failed)failed(e);}else if(done)done(x);},std::move(failed));}
