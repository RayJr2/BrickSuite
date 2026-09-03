#include "../src/database/DatabaseSchema.h"
#include "../src/services/storage/SessionStorageSelectionService.h"

#include <QCoreApplication>
#include <QHash>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
struct LocationState
{
    int workspaceId = 0;
    bool active = false;
    bool leaf = false;
};

bool require(bool condition, const QString& message)
{
    if (!condition) QTextStream(stderr) << "FAILED: " << message << '\n';
    return condition;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("BrickSuiteSessionStorageTest");
    QCoreApplication::setApplicationName("SessionStorageSelectionServiceTest");
    QTemporaryDir settingsDirectory;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       settingsDirectory.path());
    QSettings().clear();

    QHash<int, LocationState> locations{
        {10, {1, true, true}},
        {11, {1, false, true}},
        {12, {1, true, false}},
        {20, {2, true, true}}
    };
    const auto validator = [&locations](int workspaceId, int locationId, int excludedId) {
        const auto it = locations.constFind(locationId);
        return workspaceId > 0 && locationId > 0 && locationId != excludedId
               && it != locations.cend() && it->workspaceId == workspaceId
               && it->active && it->leaf;
    };

    bool ok = require(settingsDirectory.isValid(), "temporary settings directory");
    SessionStorageSelectionService service(validator);
    ok &= require(service.rememberedDestination(1) == 0, "fresh session has no destination");

    service.rememberDestination(1, 10);
    service.rememberDestination(2, 20);
    ok &= require(service.rememberedDestination(1) == 10,
                  "valid destination remembered within session");
    ok &= require(service.rememberedDestination(2) == 20,
                  "workspace destinations are isolated");
    ok &= require(service.rememberedDestination(1, 10) == 0,
                  "workflow exclusion rejects remembered source");
    ok &= require(service.rememberedDestination(1) == 10,
                  "workflow exclusion does not erase otherwise valid shared memory");

    service.clearWorkspace(1);
    service.rememberDestination(1, 20);
    ok &= require(service.rememberedDestination(1) == 0,
                  "wrong-workspace destination rejected");
    service.clearWorkspace(1);
    service.rememberDestination(1, 999);
    ok &= require(service.rememberedDestination(1) == 0, "missing destination rejected");
    service.clearWorkspace(1);
    service.rememberDestination(1, 11);
    ok &= require(service.rememberedDestination(1) == 0, "inactive destination rejected");
    service.clearWorkspace(1);
    service.rememberDestination(1, 12);
    ok &= require(service.rememberedDestination(1) == 0, "non-leaf destination rejected");

    service.rememberDestination(1, 10);
    locations[10].active = false;
    ok &= require(service.rememberedDestination(1) == 0,
                  "destination invalidated during session is discarded");
    locations[10].active = true;
    service.rememberDestination(1, 10);
    service.clearWorkspace(1);
    ok &= require(service.rememberedDestination(1) == 0
                      && service.rememberedDestination(2) == 20,
                  "clearWorkspace affects only one workspace");
    service.clearAll();
    ok &= require(service.rememberedDestination(2) == 0, "restore-style clearAll resets state");

    SessionStorageSelectionService restartedService(validator);
    ok &= require(restartedService.rememberedDestination(1) == 0,
                  "fresh process-equivalent service has no prior state");
    ok &= require(QSettings().allKeys().isEmpty(), "session selections do not use QSettings");
    ok &= require(DatabaseSchema::CurrentSchemaVersion == 29, "schema remains version 29");

    if (ok) QTextStream(stdout) << "SessionStorageSelectionServiceTest passed\n";
    return ok ? 0 : 1;
}
