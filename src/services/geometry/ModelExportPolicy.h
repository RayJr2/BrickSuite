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
#include <QString>
enum class ModelPreparationStatus { NotPrepared, Ready, Unsupported, Ambiguous, Failed };
struct ModelExportSelectionPolicy {
    bool sourceAvailable=true;
    bool preparedAvailable=false;
    bool preparedDefault=false;
    QString status;
    static ModelExportSelectionPolicy forStatus(ModelPreparationStatus state,bool validPreparedMesh=false) {
        ModelExportSelectionPolicy p;
        p.preparedAvailable=validPreparedMesh;
        p.preparedDefault=validPreparedMesh;
        if(validPreparedMesh)p.status=QStringLiteral("Prepared Mesh is Ready for Printing. Source Mesh remains available.");
        else if(state==ModelPreparationStatus::Unsupported||state==ModelPreparationStatus::Ambiguous)
            p.status=QStringLiteral("BrickSuite could not create a validated Prepared Mesh for this LDraw construction. The Source Mesh can still be exported and may be analyzed or repaired by your slicer.");
        else if(state==ModelPreparationStatus::Failed)p.status=QStringLiteral("Built-in preparation did not produce a validated Prepared Mesh. Source export is still available and may require slicer repair.");
        else p.status=QStringLiteral("Prepare for Printing to create a validated Prepared Mesh. Source export is not a BrickSuite printability guarantee.");
        return p;
    }
};
