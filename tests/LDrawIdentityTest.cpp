#include "../src/services/geometry/LDrawIdentitySelection.h"
#include "../src/ui/parts/LDrawModelViewerSelection.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
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
    QTemporaryDir library;
    if(!library.isValid()||!QDir().mkpath(library.path()+QStringLiteral("/parts")))return 1;
    const auto addModel=[&](const QString&model){
        QFile file(library.path()+QStringLiteral("/parts/")+model+QStringLiteral(".dat"));
        return file.open(QIODevice::WriteOnly);
    };
    if(!addModel(QStringLiteral("4488"))||!addModel(QStringLiteral("other"))||
       !addModel(QStringLiteral("MiXeD")))return 1;
    const QStringList multiple={QStringLiteral("10313"),QStringLiteral("4488"),QStringLiteral("other")};
    if(LDrawModelViewerSelection::initialIndex(QStringLiteral("4488"),multiple,library.path())!=1)return 1;
    if(LDrawModelViewerSelection::initialIndex(QStringLiteral("4488.dat"),multiple,library.path())!=1)return 1;
    if(LDrawModelViewerSelection::initialIndex(QStringLiteral("absent"),multiple,library.path())!=1)return 1;
    if(LDrawModelViewerSelection::initialIndex(QStringLiteral("other"),multiple,library.path())!=2)return 1;
    if(LDrawModelViewerSelection::initialIndex(QStringLiteral("4488"),{QStringLiteral("4488")},library.path())!=0)return 1;
    if(LDrawModelViewerSelection::initialIndex(QStringLiteral("4488"),{QStringLiteral("10313")},library.path())!=0)return 1;
    if(LDrawModelViewerSelection::initialIndex(QStringLiteral("4488"),{},library.path())!=-1)return 1;
    if(LDrawModelViewerSelection::initialIndex(QStringLiteral("absent"),{QStringLiteral("10313"),QStringLiteral("missing")},library.path())!=0)return 1;
    if(LDrawModelViewerSelection::initialIndex(QStringLiteral("4488"),{QStringLiteral("../4488"),QStringLiteral("4488")},library.path())!=1)return 1;
    if(LDrawModelViewerSelection::initialIndex(QStringLiteral("mixed"),{QStringLiteral("missing"),QStringLiteral("mixed")},library.path())!=1)return 1;
    if(app.arguments().size()==2&&
       LDrawModelViewerSelection::initialIndex(QStringLiteral("4488"),
           {QStringLiteral("10313"),QStringLiteral("4488")},app.arguments()[1])!=1)return 1;
    return 0;
}
