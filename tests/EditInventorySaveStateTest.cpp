#include "../src/ui/inventory/EditInventorySaveState.h"

#include <QCoreApplication>
#include <QDebug>

namespace {
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

    bool ok = true;
    ok &= require(!EditInventorySaveState::canSave(false, false, 3),
                  "An unloaded Inventory record was saveable.");
    ok &= require(!EditInventorySaveState::canSave(true, true, 3),
                  "Save remained enabled while known Colors were loading.");
    ok &= require(!EditInventorySaveState::canSave(true, true, 0),
                  "The loading placeholder was saveable.");
    ok &= require(!EditInventorySaveState::canSave(true, false, 0),
                  "An invalid loaded Color was saveable.");
    ok &= require(EditInventorySaveState::canSave(true, false, 3),
                  "A valid original Color did not enable Save after success.");
    ok &= require(EditInventorySaveState::canSave(true, false, 179),
                  "A valid fallback Color did not enable Save after failure.");

    if (!ok)
        return 1;

    qInfo() << "Edit Inventory Save-state validation passed.";
    return 0;
}
