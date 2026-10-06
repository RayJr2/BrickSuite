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
#include "MeshBooleanService.h"
#include "PrintPreparation.h"
#include "PrintPreparationCache.h"
#include "LDrawPrintGeometryBuilder.h"
#include <functional>
#include <memory>
namespace PrintGeometry {
class LDrawPrintPreparationService {
public:using BooleanServiceFactory=std::function<std::unique_ptr<MeshBooleanService>()>;
    using SemanticBuilderFunction=std::function<LDrawSemanticOperandBuilder::Result(const LDrawGeometry::LDrawLoadResult&)>;
    explicit LDrawPrintPreparationService(std::shared_ptr<PrintPreparationCache> cache={},BooleanServiceFactory factory={},SemanticBuilderFunction semanticBuilder={});
    PrintPreparationResult prepare(const PrintPreparationRequest&,const CancellationState* cancellation=nullptr,const PrintPreparationProgressCallback& progress={})const;
    std::shared_ptr<PrintPreparationCache> cache()const{return m_cache;}
private:std::shared_ptr<PrintPreparationCache>m_cache;BooleanServiceFactory m_factory;SemanticBuilderFunction m_semanticBuilder;
};
}
