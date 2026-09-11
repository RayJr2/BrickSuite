#include "../src/ui/collection/RemoteCollectionStorageFilter.h"

#include <QCoreApplication>
#include <cstdio>

namespace {
bool require(bool value, const char* message)
{
    if (!value) std::fprintf(stderr, "%s\n", message);
    return value;
}
RemoteReadDto::StorageSummary location(qint64 id, qint64 parent, bool active,
                                       bool collection, const QString& path)
{
    RemoteReadDto::StorageSummary value;
    value.storageId=id; value.parentStorageId=parent; value.active=active;
    value.allowsCollection=collection; value.displayPath=path;
    return value;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QList<RemoteReadDto::StorageSummary> source{
        location(1,0,true,true,"Room"), location(2,1,true,true,"Room / Shelf"),
        location(3,0,true,false,"Inventory"), location(4,0,false,true,"Archived"),
        location(5,0,true,true,"Display"), location(6,5,false,true,"Display / Old")};
    const auto result=RemoteCollectionStorageFilter::eligibleLeaves(source);
    bool ok=true;
    ok&=require(result.size()==2,"Expected two eligible Collection leaves.");
    ok&=require(result.at(0).storageId==2&&result.at(0).displayPath=="Room / Shelf",
                "Active child should be retained with its Host ID and path.");
    ok&=require(result.at(1).storageId==5,
                "An inactive child must not make its active parent non-leaf.");
    for(const auto& value:result)
        ok&=require(value.storageId!=1&&value.storageId!=3&&value.storageId!=4,
                    "An invalid destination was retained.");
    return ok?0:1;
}
