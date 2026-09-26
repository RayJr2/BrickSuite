#include "src/services/geometry/LDrawLibraryService.h"
#include "src/services/geometry/print/LDrawPrintGeometryBuilder.h"
#include "src/services/geometry/print/RoundTechnicPassageSemantic.h"
#include <QCoreApplication>
#include <cstdio>
int main(int argc,char**argv){
    QCoreApplication app(argc,argv);
    auto source=LDrawLibraryService::loadPart("D:/LDraw","23422");if(!source.ok())return 1;
    auto result=PrintGeometry::LDrawSemanticOperandBuilder::build(source);
    int count=0;for(const auto& op:result.operands)count+=op.functionalFeatures.size();
    printf("Builder ready=%d; generated operands=%lld; emitted functional features=%d; RoundTechnicPassage recognizer=%lld\n",result.ok(),static_cast<long long>(result.operands.size()),count,static_cast<long long>(PrintGeometry::RoundTechnicPassageSemantic::recognize(source).size()));
    for(const auto& d:result.diagnostics)printf("%s\n",qPrintable(d));
}
