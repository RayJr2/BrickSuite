#include "../src/ui/database/DatabaseStatusDialog.h"
#include "../src/ui/help/HelpManager.h"
#include "../src/ui/help/HelpTopic.h"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QGroupBox>
#include <QLabel>
#include <QKeyEvent>
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <QTextStream>
#include <QTextBrowser>

namespace
{
bool ambiguousShortcutWarning = false;

void testMessageHandler(QtMsgType, const QMessageLogContext&, const QString& message)
{
    if (message.contains("ambiguous shortcut", Qt::CaseInsensitive))
        ambiguousShortcutWarning = true;
}

bool require(bool condition, const QString& message)
{
    if (!condition) QTextStream(stderr) << "FAILED: " << message << '\n';
    return condition;
}

bool createFixture(const QString& path)
{
    const QString name = "recovery_guidance_fixture";
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(path);
        if (!db.open()) return false;
        QSqlQuery query(db);
        const QStringList statements = {
            "CREATE TABLE schema_version(version INTEGER)", "INSERT INTO schema_version VALUES(29)",
            "CREATE TABLE part(id INTEGER)", "CREATE TABLE set_catalog(id INTEGER)",
            "CREATE TABLE minifig_catalog(id INTEGER,is_active INTEGER)",
            "CREATE TABLE inventory_record(id INTEGER,quantity INTEGER)",
            "CREATE TABLE build(id INTEGER,is_active INTEGER)",
            "CREATE TABLE manufacturer(id INTEGER,is_active INTEGER)",
            "CREATE TABLE storage_location(id INTEGER,is_active INTEGER)"
        };
        for (const QString& statement : statements)
            if (!query.exec(statement)) return false;
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
    return true;
}

void sendF1(QWidget* focusWidget)
{
    focusWidget->setFocus(Qt::OtherFocusReason);
    QCoreApplication::processEvents();
    QKeyEvent press(QEvent::KeyPress, Qt::Key_F1, Qt::NoModifier);
    QApplication::sendEvent(focusWidget, &press);
    QKeyEvent release(QEvent::KeyRelease, Qt::Key_F1, Qt::NoModifier);
    QApplication::sendEvent(focusWidget, &release);
    QCoreApplication::processEvents();
}
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QTemporaryDir directory;
    const QString path = directory.filePath("recovery.db");
    if (!require(directory.isValid() && createFixture(path), "isolated fixture")) return 1;

    QMainWindow host;
    host.show();
    QAction globalHelp(&host);
    globalHelp.setShortcut(QKeySequence::HelpContents);
    globalHelp.setShortcutContext(Qt::ApplicationShortcut);
    host.addAction(&globalHelp);
    int helpInvocations = 0;
    QList<HelpTopic> invokedTopics;
    QObject::connect(&globalHelp, &QAction::triggered, [&]() {
        ++helpInvocations;
        invokedTopics.append(HelpManager::contextTopic(QApplication::activeWindow())
                                 .value_or(HelpTopic::PartsCatalog));
    });

    DatabaseStatusDialog dialog(path, &host);
    dialog.show();
    dialog.activateWindow();
    auto* progress = dialog.findChild<QProgressBar*>();
    QElapsedTimer waitTimer;
    waitTimer.start();
    while (progress && !progress->isHidden() && waitTimer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }
    QCoreApplication::processEvents();
    auto* panel = dialog.findChild<QWidget*>("recoveryPanel");
    if (!require(panel && panel->isHidden(), "healthy/default state hides recovery panel")) return 1;

    const QtMessageHandler previousHandler = qInstallMessageHandler(testMessageHandler);
    for (const char* objectName : {"statusRefreshButton", "integrityCheckButton", "foreignKeyCheckButton"}) {
        QWidget* child = dialog.findChild<QWidget*>(objectName);
        const int before = helpInvocations;
        sendF1(child);
        if (!require(helpInvocations == before + 1
                     && invokedTopics.last() == HelpTopic::DatabaseStatus,
                     QString("F1 dispatch from %1").arg(objectName))) return 1;
    }
    dialog.hide();
    host.activateWindow();
    sendF1(&host);
    qInstallMessageHandler(previousHandler);
    if (!require(helpInvocations == 4 && invokedTopics.last() == HelpTopic::PartsCatalog,
                 "F1 falls back after contextual dialog closes")
        || !require(!ambiguousShortcutWarning, "no ambiguous-shortcut warning")) return 1;

    dialog.show();
    dialog.activateWindow();
    QCoreApplication::processEvents();
    dialog.findChild<QPushButton*>("databaseStatusHelpButton")->click();
    QCoreApplication::processEvents();
    bool helpViewerOpened = false;
    for (QWidget* window : QApplication::topLevelWidgets()) {
        if (window->windowTitle().contains("BrickSuite Help")) {
            helpViewerOpened = true;
            break;
        }
    }
    if (!require(helpViewerOpened && helpInvocations == 4,
                 "Help button works independently of global F1 action")) return 1;

    DatabaseIntegrityCheckResult integrity;
    integrity.outcome = DatabaseDiagnosticOutcome::IntegrityProblems;
    integrity.issueCount = 30;
    integrity.elapsedMilliseconds = 42;
    for (int i = 0; i < 30; ++i) integrity.representativeIssues.append(QString("diagnostic-%1").arg(i));
    dialog.presentIntegrityResult(integrity);
    auto* recoveryHeading = dialog.findChild<QLabel*>("recoveryHeading");
    auto* recoveryText = dialog.findChild<QLabel*>("recoveryText");
    if (!require(!panel->isHidden(), "integrity problems show recovery panel")
        || !require(recoveryHeading && recoveryHeading->text() == "Integrity problems found"
                    && recoveryText->text().contains("damage to the SQLite database structure")
                    && recoveryText->text().contains("preserves its present state and may also preserve the detected problem"),
                    "integrity-specific guidance and backup preservation caveat")) return 1;
    QString summary = dialog.diagnosticSummary();
    if (!require(summary.contains("BrickSuite Version") && summary.contains("Schema version: 29")
                 && summary.contains("Integrity Check") && summary.contains("Issue count: 30")
                 && summary.contains("diagnostic-24") && !summary.contains("diagnostic-25")
                 && summary.contains("made no repairs or data changes"), "bounded integrity summary metadata")
        || !require(!summary.contains("API key", Qt::CaseInsensitive)
                    && !summary.contains("credential", Qt::CaseInsensitive), "summary excludes secrets")) return 1;

    DatabaseForeignKeyCheckResult foreignKeys;
    foreignKeys.outcome = DatabaseDiagnosticOutcome::ForeignKeyViolations;
    foreignKeys.violationCount = 1;
    foreignKeys.representativeViolations.append({"child", 7, "parent", 0});
    dialog.presentForeignKeyResult(foreignKeys);
    summary = dialog.diagnosticSummary();
    if (!require(!panel->isHidden() && summary.contains("Foreign Key Check")
                 && summary.contains("child row 7 -> parent")
                 && recoveryHeading->text() == "Database relationship violations found"
                 && recoveryText->text().contains("does not necessarily mean the SQLite file itself is physically damaged"),
                 "distinct FK guidance and summary")) return 1;

    int logRequests = 0;
    int backupRequests = 0;
    int restoreRequests = 0;
    QObject::connect(&dialog, &DatabaseStatusDialog::applicationLogRequested,
                     [&logRequests]() { ++logRequests; });
    QObject::connect(&dialog, &DatabaseStatusDialog::backupRequested,
                     [&backupRequests]() { ++backupRequests; });
    QObject::connect(&dialog, &DatabaseStatusDialog::restoreRequested,
                     [&restoreRequests]() { ++restoreRequests; });
    dialog.findChild<QPushButton*>("recoveryApplicationLogButton")->click();
    dialog.findChild<QPushButton*>("recoveryBackupButton")->click();
    dialog.findChild<QPushButton*>("recoveryRestoreButton")->click();
    if (!require(logRequests == 1 && backupRequests == 1 && restoreRequests == 1,
                 "recovery actions route through request signals")) return 1;

    DatabaseIntegrityCheckResult operational;
    for (const auto outcome : {DatabaseDiagnosticOutcome::Healthy,
                               DatabaseDiagnosticOutcome::BusyOrLocked,
                               DatabaseDiagnosticOutcome::DatabaseMissing,
                               DatabaseDiagnosticOutcome::OpenFailure,
                               DatabaseDiagnosticOutcome::QueryFailure}) {
        operational.outcome = outcome;
        operational.errorMessage = "synthetic failure";
        dialog.presentIntegrityResult(operational);
        if (!require(panel->isHidden(), QString("outcome %1 hides recovery panel").arg(static_cast<int>(outcome)))) return 1;
    }

    if (!require(DatabaseStatusService::statusSnapshot(path).schemaVersion == 29,
                 "schema remains 29 and presentation did not mutate database")) return 1;
    QTextStream(stdout) << "DatabaseRecoveryGuidanceTest passed\n";
    return 0;
}
