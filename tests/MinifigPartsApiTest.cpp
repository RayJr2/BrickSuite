#include "../src/api/rebrickable/RebrickableService.h"

#include <QCoreApplication>
#include <QDebug>

namespace {
bool require(bool condition, const QString& message)
{
    if (!condition) qCritical().noquote() << message;
    return condition;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QByteArray pageOne = R"({"count":3,"next":"https://rebrickable.com/api/v3/lego/minifigs/fig-000852/parts/?page=2&page_size=2","results":[{"part":{"part_num":"a/1"},"color":{"id":1},"quantity":2,"is_spare":false},{"part":{"part_num":"b"},"color":{"id":0},"quantity":1,"is_spare":true}]})";
    const QByteArray pageTwo = R"({"count":3,"next":null,"results":[{"part":{"part_num":"c"},"color":{"id":5},"quantity":4,"is_spare":false}]})";
    RebrickableService::MinifigPartsPage first;
    RebrickableService::MinifigPartsPage second;
    QString error;
    if (!require(RebrickableService::parseMinifigPartsPage(pageOne, first, error)
                 && first.totalCount == 3 && first.parts.size() == 2
                 && first.parts.at(1).isSpare
                 && RebrickableService::isTrustedMinifigPartsNextUrl(first.nextUrl),
                 "Valid first page failed: " + error)) return 1;
    if (!require(RebrickableService::parseMinifigPartsPage(pageTwo, second, error)
                 && second.parts.size() == 1 && second.nextUrl.isEmpty(),
                 "Valid terminal page failed: " + error)) return 1;
    QList<RebrickableService::MinifigPart> aggregate = first.parts;
    aggregate.append(second.parts);
    if (!require(aggregate.size() == first.totalCount, "Multi-page aggregation fixture failed.")) return 1;
    if (!require(!RebrickableService::isTrustedMinifigPartsNextUrl("https://example.com/api/v3/lego/minifigs/fig-000852/parts/")
                 && !RebrickableService::isTrustedMinifigPartsNextUrl("http://rebrickable.com/api/v3/lego/minifigs/fig-000852/parts/")
                 && !RebrickableService::isTrustedMinifigPartsNextUrl("https://rebrickable.com:444/api/v3/lego/minifigs/fig-000852/parts/")
                 && !RebrickableService::isTrustedMinifigPartsNextUrl("https://rebrickable.com/api/v3/lego/sets/x/parts/"),
                 "Unsafe pagination URL was trusted.")) return 1;

    const QList<QByteArray> malformed = {
        R"([])", R"({"count":"3","next":null,"results":[]})",
        R"({"count":1,"next":null,"results":[{"part":{},"color":{"id":1},"quantity":1,"is_spare":false}]})",
        R"({"count":1,"next":null,"results":[{"part":{"part_num":"a"},"color":{"id":"1"},"quantity":1,"is_spare":false}]})",
        R"({"count":1,"next":null,"results":[{"part":{"part_num":"a"},"color":{"id":1},"quantity":0,"is_spare":false}]})",
        R"({"count":1,"next":null,"results":[{"part":{"part_num":"a"},"color":{"id":1},"quantity":1,"is_spare":0}]})"
    };
    for (const QByteArray& input : malformed) {
        RebrickableService::MinifigPartsPage page;
        error.clear();
        if (!require(!RebrickableService::parseMinifigPartsPage(input, page, error)
                     && !error.isEmpty(), "Malformed response was accepted.")) return 1;
    }
    qInfo() << "M23.7.5 Minifig parts API parsing validation passed.";
    return 0;
}
