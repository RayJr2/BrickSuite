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

#include "RemotePullingApplicationService.h"
#include "RemoteMutationApplicationServices.h"

RemotePullingApplicationService::RemotePullingApplicationService(
    RemoteMutationApplicationServices& mutations, QObject* parent)
    : QObject(parent), m_mutations(mutations) {}

bool RemotePullingApplicationService::isAvailable() const
{
    return m_mutations.isAvailableFor(QStringLiteral("builds.pulling.record"),
                                      QStringLiteral("builds.pulling.write"));
}

QString RemotePullingApplicationService::record(
    const RemotePullingMutationDto::Request& request, QObject* context,
    std::function<void(const RemotePullingMutationDto::Result&)> completion,
    std::function<void(const RemoteMutationDto::Error&)> failure)
{
    return m_mutations.submit(QStringLiteral("builds.pulling.record"),
        QStringLiteral("builds.pulling.write"), RemotePullingMutationDto::toMetadata(request), context,
        [completion=std::move(completion), failure](const RemoteMutationDto::Result& source) {
            RemotePullingMutationDto::Result result;
            RemoteMutationDto::Error error;
            if (!RemotePullingMutationDto::resultFromMutation(source, &result, &error)) {
                if (failure) failure(error);
            } else if (completion) completion(result);
        }, std::move(failure));
}
