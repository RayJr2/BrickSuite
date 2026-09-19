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
