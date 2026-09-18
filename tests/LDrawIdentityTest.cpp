#include "../src/services/geometry/LDrawIdentitySelection.h"
#include <QCoreApplication>
int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);
    QList<ExternalPartIdentifier> rows;
    rows.append({1,2,"LDraw","32064d","test",true});
    rows.append({2,2,"LDraw","32064b","test",true});
    rows.append({3,2,"BrickLink","wrong","test",true});
    rows.append({4,2,"LDraw","inactive","test",false});
    if(LDrawIdentitySelection::exactCandidates(rows)!=QStringList({"32064b","32064d"}))return 1;
    if(!LDrawIdentitySelection::exactCandidates({}).isEmpty())return 1;
    return 0;
}
