#include "../src/ui/inventory/AddInventoryColorSelection.h"

#include <QCoreApplication>
#include <QDebug>

namespace {
Color color(int id, int rebrickableId, const QString& name)
{
    Color value;
    value.setId(id);
    value.setRebrickableId(rebrickableId);
    value.setName(name);
    return value;
}

bool require(bool condition, const char* message)
{
    if (!condition)
        qCritical() << message;
    return condition;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const Color flatSilver = color(10, 100, QStringLiteral("Flat Silver"));
    const Color lightBluishGray = color(20, 200, QStringLiteral("Light Bluish Gray"));
    const Color black = color(30, 300, QStringLiteral("Black"));
    const QList<Color> catalog{flatSilver, lightBluishGray, black};

    const QList<Color> partA = AddInventoryColorSelection::visibleColors(
        catalog, {100}, false);
    if (!require(partA.size() == 1 && partA.first().id() == flatSilver.id(),
                 "Known-color filtering included a globally valid but Part-invalid Color."))
        return 1;

    const QList<Color> prefilledPartA = AddInventoryColorSelection::visibleColors(
        catalog, {100}, false);
    if (!require(prefilledPartA.size() == partA.size()
                     && prefilledPartA.first().id() == partA.first().id(),
                 "General and prefilled launch policies produced different known colors."))
        return 2;

    const QList<Color> all = AddInventoryColorSelection::visibleColors(
        catalog, {100}, true);
    if (!require(all.size() == catalog.size(),
                 "Show All Colors did not expose the complete Color catalog."))
        return 3;

    if (!require(AddInventoryColorSelection::retainedColorId(all, lightBluishGray.id())
                     == lightBluishGray.id()
                 && AddInventoryColorSelection::retainedColorId(partA, lightBluishGray.id()) == 0,
                 "An invalid prior selection survived the known-color filter."))
        return 4;

    const QList<Color> partB = AddInventoryColorSelection::visibleColors(
        catalog, {200, 300}, false);
    if (!require(partB.size() == 2
                     && AddInventoryColorSelection::retainedColorId(partB, flatSilver.id()) == 0
                     && AddInventoryColorSelection::retainedColorId(partB, black.id()) == black.id(),
                 "Part changes did not remove stale colors or retain a still-valid color."))
        return 5;

    if (!require(AddInventoryColorSelection::retainedColorId(partA, flatSilver.id())
                     == flatSilver.id(),
                 "Remember Part / Keep Open could not retain a valid known color."))
        return 6;

    qInfo() << "Add Inventory known-color selection validation passed.";
    return 0;
}
