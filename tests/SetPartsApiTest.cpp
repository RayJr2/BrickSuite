#include "../src/api/rebrickable/RebrickableService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>

namespace { bool require(bool ok, const QString& text) { if (!ok) qCritical().noquote() << text; return ok; } }

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QByteArray first = R"({"count":3,"next":"https://rebrickable.com/api/v3/lego/sets/1234-1/parts/?page=2&page_size=2&inc_minifig_parts=1","results":[{"part":{"part_num":"a"},"color":{"id":1},"quantity":2,"is_spare":false},{"part":{"part_num":"b"},"color":{"id":0},"quantity":1,"is_spare":true}]})";
    const QByteArray second = R"({"count":3,"next":null,"results":[{"part":{"part_num":"c"},"color":{"id":5},"quantity":4,"is_spare":false}]})";
    RebrickableService::SetPartsPage page1, page2; QString error;
    if (!require(RebrickableService::parseSetPartsPage(first, page1, error)
                 && page1.totalCount == 3 && page1.parts.size() == 2
                 && page1.parts.at(1).isSpare
                 && RebrickableService::isTrustedSetPartsNextUrl(page1.nextUrl, "1234-1"),
                 "Valid Set page failed: " + error)) return 1;
    if (!require(RebrickableService::parseSetPartsPage(second, page2, error)
                 && page2.parts.size() == 1 && page2.nextUrl.isEmpty(),
                 "Terminal Set page failed: " + error)) return 1;
    QList<RebrickableService::SetPart> combined = page1.parts; combined.append(page2.parts);
    if (!require(combined.size() == page1.totalCount, "Multi-page aggregation failed.")) return 1;

    const QString good = "https://rebrickable.com/api/v3/lego/sets/1234-1/parts/?page=2&inc_minifig_parts=1";
    const QStringList unsafe = {
        "http://rebrickable.com/api/v3/lego/sets/1234-1/parts/?inc_minifig_parts=1",
        "https://example.com/api/v3/lego/sets/1234-1/parts/?inc_minifig_parts=1",
        "https://rebrickable.com:444/api/v3/lego/sets/1234-1/parts/?inc_minifig_parts=1",
        "https://rebrickable.com/api/v3/lego/sets/9999-1/parts/?inc_minifig_parts=1",
        "https://rebrickable.com/api/v3/lego/sets/1234-1/parts/?page=2"
    };
    if (!require(RebrickableService::isTrustedSetPartsNextUrl(good, "1234-1"), "Valid pagination rejected.")) return 1;
    for (const QString& url : unsafe)
        if (!require(!RebrickableService::isTrustedSetPartsNextUrl(url, "1234-1"), "Unsafe pagination accepted: " + url)) return 1;

    const QList<QByteArray> malformed = {
        R"([])", R"({"count":"1","next":null,"results":[]})",
        R"({"count":1,"next":null,"results":[{"part":{},"color":{"id":1},"quantity":1,"is_spare":false}]})",
        R"({"count":1,"next":null,"results":[{"part":{"part_num":"a"},"color":{"id":"1"},"quantity":1,"is_spare":false}]})",
        R"({"count":1,"next":null,"results":[{"part":{"part_num":"a"},"color":{"id":1},"quantity":0,"is_spare":false}]})",
        R"({"count":1,"next":null,"results":[{"part":{"part_num":"a"},"color":{"id":1},"quantity":1,"is_spare":0}]})"
    };
    for (const QByteArray& input : malformed) {
        RebrickableService::SetPartsPage page; error.clear();
        if (!require(!RebrickableService::parseSetPartsPage(input, page, error) && !error.isEmpty(),
                     "Malformed Set response accepted.")) return 1;
    }

    QJsonArray fiveHundredRows;
    for (int index = 0; index < 500; ++index) {
        fiveHundredRows.append(QJsonObject{
            {"part", QJsonObject{{"part_num", QString("part-%1").arg(index)}}},
            {"color", QJsonObject{{"id", index % 10}}},
            {"quantity", 1}, {"is_spare", false}});
    }
    const QByteArray fullPage = QJsonDocument(QJsonObject{
        {"count", 501}, {"next", good}, {"results", fiveHundredRows}}).toJson();
    RebrickableService::SetPartsPage parsedFullPage;
    error.clear();
    if (!require(RebrickableService::parseSetPartsPage(fullPage, parsedFullPage, error)
                 && parsedFullPage.parts.size() == 500 && parsedFullPage.totalCount == 501,
                 "A full 500-row non-terminal page was truncated or rejected: " + error)) return 1;

    const QUrl initial = RebrickableService::setCatalogPartsInitialUrl("1234-1");
    if (!require(QUrlQuery(initial).queryItemValue("inc_minifig_parts") == "1",
                 "Catalog API fixture does not request Minifig constituent parts.")) return 1;
    qInfo() << "M23.10.1 Set parts API validation passed.";
    return 0;
}
