#include "../src/settings/UserSettings.h"

#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <cstdio>

namespace {
bool check(bool value, const char* message)
{
    if (!value) std::fprintf(stderr, "FAILED: %s\n", message);
    return value;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("BrickSuiteM26Test");
    QCoreApplication::setApplicationName("SharedDataSourceSettingsTest");
    QTemporaryDir directory;
    if (!check(directory.isValid(), "temporary settings directory")) return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());

    UserSettings& settings = UserSettings::instance();
    bool ok = true;
    ok &= check(settings.sharedDataSource() == SharedDataSource::ThisComputer,
                "default is This Computer");
    settings.setSharedDataSource(SharedDataSource::BrickSuiteHost);
    ok &= check(settings.sharedDataSource() == SharedDataSource::BrickSuiteHost,
                "Host selection persists");
    {
        QSettings raw;
        raw.beginGroup("General");
        ok &= check(raw.value("SharedDataSource").toString() == "bricksuite-host",
                    "stable Host serialization");
        raw.setValue("SharedDataSource", "corrupt-future-value");
        raw.endGroup();
        raw.sync();
    }
    ok &= check(settings.sharedDataSource() == SharedDataSource::ThisComputer,
                "unknown value fails safe to This Computer");

    const QString connectionName = "m26-settings-db-independence";
    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    database.setDatabaseName(directory.filePath("dormant.db"));
    ok &= check(database.open(), "temporary dormant-data database opens");
    QSqlQuery query(database);
    ok &= check(query.exec("CREATE TABLE dormant_operational_data(value TEXT)")
                    && query.exec("INSERT INTO dormant_operational_data VALUES('LOCAL_POISON_WORKSPACE'),('LOCAL_POISON_INVENTORY'),('LOCAL_POISON_BUILD'),('LOCAL_POISON_COLLECTION'),('LOCAL_POISON_PART_REFERENCE')"),
                "dormant local operational rows seeded");
    settings.setSharedDataSource(SharedDataSource::ThisComputer);
    ok &= check(settings.sharedDataSource() == SharedDataSource::ThisComputer,
                "switch back persists");
    ok &= check(query.exec("SELECT COUNT(*) FROM dormant_operational_data")
                    && query.next() && query.value(0).toInt() == 5,
                "mode switching does not mutate dormant local rows");
    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
    return ok ? 0 : 1;
}
