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
    ok &= check(!settings.brickSuiteServerEnabled(), "server disabled by default");
    ok &= check(settings.brickSuiteServerBindAddress() == "127.0.0.1",
                "safe loopback bind default");
    ok &= check(settings.brickSuiteServerPort() == UserSettings::DefaultBrickSuiteServerPort,
                "server port default");
    ok &= check(settings.brickSuiteHostEndpoint() == "wss://localhost:47826",
                "secure Host endpoint default");
    ok &= check(settings.brickSuiteReconnectAutomatically(), "reconnect enabled by default");
    settings.setBrickSuiteServerEnabled(true);
    settings.setBrickSuiteServerBindAddress("192.0.2.10");
    settings.setBrickSuiteServerPort(50001);
    settings.setBrickSuiteHostEndpoint("wss://example.invalid:50001");
    settings.setBrickSuiteTrustedFingerprint(QString(64, 'A'));
    settings.setBrickSuiteReconnectAutomatically(false);
    ok &= check(settings.brickSuiteServerEnabled()
                && settings.brickSuiteServerBindAddress() == "192.0.2.10"
                && settings.brickSuiteServerPort() == 50001
                && settings.brickSuiteHostEndpoint() == "wss://example.invalid:50001"
                && settings.brickSuiteTrustedFingerprint() == QString(64, 'A')
                && !settings.brickSuiteReconnectAutomatically(),
                "network settings persist without schema changes");
    settings.setBrickSuiteServerPort(1);
    ok &= check(settings.brickSuiteServerPort() == UserSettings::DefaultBrickSuiteServerPort,
                "invalid server port fails safe");
    {
        QSettings raw;
        raw.beginGroup("BrickSuiteNetwork");
        ok &= check(!raw.contains("HostAccessToken") && !raw.contains("ClientAccessToken")
                    && !raw.contains("AccessToken"),
                    "network secrets absent from QSettings");
        raw.endGroup();
    }
    settings.setSharedDataSource(SharedDataSource::BrickSuiteHost);
    ok &= check(settings.sharedDataSource() == SharedDataSource::BrickSuiteHost,
                "Host selection persists");
    settings.setRememberedHostWorkspace("wss://host-a:47826|fingerprint-a", 7, "Workshop A");
    settings.setRememberedHostWorkspace("wss://host-b:47826|fingerprint-b", 7, "Workshop B");
    ok &= check(settings.rememberedHostWorkspaceId("wss://host-a:47826|fingerprint-a") == 7
                && settings.rememberedHostWorkspaceName("wss://host-a:47826|fingerprint-a") == "Workshop A"
                && settings.rememberedHostWorkspaceName("wss://host-b:47826|fingerprint-b") == "Workshop B",
                "remembered Workspace is scoped to exact Host identity");
    settings.clearRememberedHostWorkspace("wss://host-a:47826|fingerprint-a");
    ok &= check(settings.rememberedHostWorkspaceId("wss://host-a:47826|fingerprint-a") == 0
                && settings.rememberedHostWorkspaceId("wss://host-b:47826|fingerprint-b") == 7,
                "clearing one Host Workspace does not affect another Host");
    const QString trustedFingerprint(64, 'A');
    const QString configuredIdentity = QStringLiteral("wss://example.invalid:50001|")
        + trustedFingerprint.toLower();
    settings.setBrickSuiteTrustedFingerprint(trustedFingerprint);
    settings.setRememberedHostWorkspace(configuredIdentity, 9, "Configured Workshop");
    {
        QSettings raw;
        raw.beginGroup("BrickSuiteNetwork/RetainedDataEpoch");
        raw.setValue(trustedFingerprint, "11111111-1111-1111-1111-111111111111");
        raw.endGroup();
        raw.beginGroup("Appearance");
        raw.setValue("Theme", "dark");
    }
    settings.clearBrickSuiteHostTrustState("wss://example.invalid:50001",
                                           trustedFingerprint);
    ok &= check(settings.brickSuiteTrustedFingerprint().isEmpty()
                    && settings.rememberedHostWorkspaceId(configuredIdentity) == 0,
                "Forget Host clears pinned trust and remembered Host Workspace");
    {
        QSettings raw;
        raw.beginGroup("BrickSuiteNetwork/RetainedDataEpoch");
        const bool retainedEpochRemoved = !raw.contains(trustedFingerprint);
        raw.endGroup();
        raw.beginGroup("Appearance");
        const bool unrelatedPreferencePreserved = raw.value("Theme").toString() == "dark";
        ok &= check(retainedEpochRemoved && unrelatedPreferencePreserved,
                    "Forget Host clears retained epoch without unrelated preferences");
    }
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
