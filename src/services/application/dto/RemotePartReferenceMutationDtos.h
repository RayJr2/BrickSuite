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

#include "RemoteMutationDtos.h"
#include "../../parts/PartReferenceCustomizationService.h"

namespace RemotePartReferenceMutationDto {

struct ExpectedState {
    QString modifiedUtc;
    QString partNumber;
    QString catalog;
    QString section;
    PartReferencePlacement placement = PartReferencePlacement::Append;
    QString anchorPartNumber;
};

struct Request {
    qint64 workspaceId = 0; // Mutation-envelope context only; customizations are Host-global.
    QString mutationId;
    qint64 customizationId = 0;
    QString partNumber;
    QString catalog;
    QString section;
    PartReferencePlacement placement = PartReferencePlacement::Append;
    QString anchorPartNumber;
    ExpectedState expected;
};

struct Result {
    QString mutationId;
    QString operation;
    bool replayed = false;
    qint64 customizationId = 0;
    QString partNumber;
    QString catalog;
    QString section;
    PartReferencePlacement placement = PartReferencePlacement::Append;
    QString anchorPartNumber;
    QString createdUtc;
    QString modifiedUtc;
};

RemoteMutationDto::Metadata toMetadata(const QString& operation, const Request& request);
bool fromMetadata(const QString& operation, const RemoteMutationDto::Metadata& metadata,
                  Request* result, RemoteMutationDto::Error* error = nullptr);
bool resultFromMutation(const RemoteMutationDto::Result& source, Result* result,
                        RemoteMutationDto::Error* error = nullptr);
QString placementName(PartReferencePlacement placement);

} // namespace RemotePartReferenceMutationDto
