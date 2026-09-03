#include "DatabaseStatusDialog.h"

#include "../help/HelpManager.h"
#include "../help/HelpTopic.h"
#include "../../core/AppVersion.h"
#include "../../services/Logger.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>

#include <memory>

namespace
{
constexpr int MaximumSummaryRows = 25;

QString sizeText(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes);
}

QGroupBox* group(const QString& title, QLabel*& label, QWidget* parent)
{
    auto* box = new QGroupBox(title, parent);
    auto* layout = new QVBoxLayout(box);
    label = new QLabel(box);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    layout->addWidget(label);
    return box;
}
}

DatabaseStatusDialog::DatabaseStatusDialog(const QString& databasePath, QWidget* parent)
    : QDialog(parent), m_databasePath(databasePath)
{
    setWindowTitle("Database Status & Integrity");
    HelpManager::setContextTopic(this, HelpTopic::DatabaseStatus);
    resize(650, 620);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(group("Database", m_databaseValues, this));
    layout->addWidget(group("BrickSuite Data", m_dataValues, this));
    auto* details = group("Database Details", m_detailsValues, this);
    details->setCheckable(true);
    details->setChecked(false);
    m_detailsValues->setVisible(false);
    connect(details, &QGroupBox::toggled, m_detailsValues, &QWidget::setVisible);
    layout->addWidget(details);

    auto* integrity = new QGroupBox("Integrity", this);
    auto* integrityLayout = new QVBoxLayout(integrity);
    m_resultLabel = new QLabel("No check has been run in this session.", integrity);
    m_resultLabel->setWordWrap(true);
    integrityLayout->addWidget(m_resultLabel);
    m_progress = new QProgressBar(integrity);
    m_progress->setRange(0, 0);
    m_progress->hide();
    integrityLayout->addWidget(m_progress);
    auto* checkButtons = new QDialogButtonBox(integrity);
    m_integrityButton = checkButtons->addButton("Run Integrity Check", QDialogButtonBox::ActionRole);
    m_integrityButton->setObjectName("integrityCheckButton");
    m_foreignKeyButton = checkButtons->addButton("Run Foreign Key Check", QDialogButtonBox::ActionRole);
    m_foreignKeyButton->setObjectName("foreignKeyCheckButton");
    integrityLayout->addWidget(checkButtons);
    layout->addWidget(integrity);

    m_recoveryPanel = new QGroupBox("Recovery Guidance", this);
    m_recoveryPanel->setObjectName("recoveryPanel");
    auto* recoveryLayout = new QVBoxLayout(m_recoveryPanel);
    m_recoveryHeading = new QLabel(m_recoveryPanel);
    m_recoveryHeading->setObjectName("recoveryHeading");
    QFont headingFont = m_recoveryHeading->font();
    headingFont.setBold(true);
    m_recoveryHeading->setFont(headingFont);
    recoveryLayout->addWidget(m_recoveryHeading);
    m_recoveryText = new QLabel(m_recoveryPanel);
    m_recoveryText->setObjectName("recoveryText");
    m_recoveryText->setWordWrap(true);
    m_recoveryText->setTextFormat(Qt::RichText);
    recoveryLayout->addWidget(m_recoveryText);
    auto* recoveryButtons = new QDialogButtonBox(m_recoveryPanel);
    auto* recoveryLogButton = recoveryButtons->addButton("Open Application Log", QDialogButtonBox::ActionRole);
    recoveryLogButton->setObjectName("recoveryApplicationLogButton");
    auto* recoveryFolderButton = recoveryButtons->addButton("Open Database Folder", QDialogButtonBox::ActionRole);
    auto* recoveryRestoreButton = recoveryButtons->addButton("Restore from Backup...", QDialogButtonBox::ActionRole);
    recoveryRestoreButton->setObjectName("recoveryRestoreButton");
    auto* recoveryBackupButton = recoveryButtons->addButton("Backup Current Database...", QDialogButtonBox::ActionRole);
    recoveryBackupButton->setObjectName("recoveryBackupButton");
    auto* copyButton = recoveryButtons->addButton("Copy Diagnostic Summary", QDialogButtonBox::ActionRole);
    copyButton->setObjectName("copyDiagnosticSummaryButton");
    auto* recoveryHelpButton = recoveryButtons->addButton(QDialogButtonBox::Help);
    recoveryLayout->addWidget(recoveryButtons);
    layout->addWidget(m_recoveryPanel);
    m_recoveryPanel->hide();

    auto* buttons = new QDialogButtonBox(this);
    m_refreshButton = buttons->addButton("Refresh", QDialogButtonBox::ActionRole);
    m_refreshButton->setObjectName("statusRefreshButton");
    auto* folderButton = buttons->addButton("Open Database Folder", QDialogButtonBox::ActionRole);
    auto* backupButton = buttons->addButton("Backup...", QDialogButtonBox::ActionRole);
    auto* restoreButton = buttons->addButton("Restore...", QDialogButtonBox::ActionRole);
    auto* helpButton = buttons->addButton(QDialogButtonBox::Help);
    helpButton->setObjectName("databaseStatusHelpButton");
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    connect(m_refreshButton, &QPushButton::clicked, this, &DatabaseStatusDialog::refresh);
    connect(m_integrityButton, &QPushButton::clicked, this, &DatabaseStatusDialog::runIntegrityCheck);
    connect(m_foreignKeyButton, &QPushButton::clicked, this, &DatabaseStatusDialog::runForeignKeyCheck);
    connect(folderButton, &QPushButton::clicked, this, &DatabaseStatusDialog::openDatabaseFolder);
    connect(backupButton, &QPushButton::clicked, this, &DatabaseStatusDialog::backupRequested);
    connect(restoreButton, &QPushButton::clicked, this, &DatabaseStatusDialog::restoreRequested);
    connect(recoveryLogButton, &QPushButton::clicked, this, &DatabaseStatusDialog::applicationLogRequested);
    connect(recoveryFolderButton, &QPushButton::clicked, this, &DatabaseStatusDialog::openDatabaseFolder);
    connect(recoveryRestoreButton, &QPushButton::clicked, this, &DatabaseStatusDialog::restoreRequested);
    connect(recoveryBackupButton, &QPushButton::clicked, this, &DatabaseStatusDialog::backupRequested);
    connect(copyButton, &QPushButton::clicked, this, &DatabaseStatusDialog::copyDiagnosticSummary);
    connect(recoveryHelpButton, &QPushButton::clicked, this, [this]() {
        HelpManager::showTopic(HelpTopic::DatabaseStatus, this);
    });
    connect(helpButton, &QPushButton::clicked, this, [this]() {
        HelpManager::showTopic(HelpTopic::DatabaseStatus, this);
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    refresh();
}

void DatabaseStatusDialog::setBusy(bool busy, const QString& text)
{
    m_refreshButton->setEnabled(!busy);
    m_integrityButton->setEnabled(!busy);
    m_foreignKeyButton->setEnabled(!busy);
    m_progress->setVisible(busy);
    if (!text.isEmpty()) m_resultLabel->setText(text);
}

void DatabaseStatusDialog::refresh()
{
    setBusy(true, "Refreshing database status...");
    auto result = std::make_shared<DatabaseStatusSnapshot>();
    QThread* worker = QThread::create([result, path = m_databasePath]() {
        *result = DatabaseStatusService::statusSnapshot(path);
    });
    connect(worker, &QThread::finished, this, [this, result]() {
        setBusy(false);
        applySnapshot(*result);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void DatabaseStatusDialog::applySnapshot(const DatabaseStatusSnapshot& s)
{
    if (s.outcome != DatabaseDiagnosticOutcome::Healthy) {
        showFailure(s.outcome, s.errorMessage); return;
    }
    m_snapshot = s;
    m_databaseValues->setText(QString("Path: %1\nSchema version: %2\nFile size: %3\nLast modified: %4")
        .arg(s.databasePath).arg(s.schemaVersion).arg(sizeText(s.fileSize))
        .arg(QLocale().toString(s.lastModified, QLocale::ShortFormat)));
    m_dataValues->setText(QString("Parts: %1    Sets: %2    Minifigs: %3\nInventory records: %4    Loose pieces: %5\nActive Builds: %6    Archived Builds: %7\nActive Manufacturers: %8    Active Storage Locations: %9")
        .arg(s.partCount).arg(s.setCount).arg(s.minifigCount).arg(s.inventoryRecordCount)
        .arg(s.inventoryPieceCount).arg(s.activeBuildCount).arg(s.archivedBuildCount)
        .arg(s.activeManufacturerCount).arg(s.activeStorageLocationCount));
    const qint64 allocated = s.pageSize * s.pageCount;
    const qint64 reclaimable = s.pageSize * s.freelistPageCount;
    m_detailsValues->setText(QString("SQLite: %1    Journal mode: %2    Foreign keys: %3\nPage size: %4    Page count: %5    Freelist pages: %6\nAllocated: %7    Reclaimable: %8\nWAL: %9    SHM: %10")
        .arg(s.sqliteVersion, s.journalMode, s.foreignKeysEnabled ? "Enabled" : "Disabled")
        .arg(s.pageSize).arg(s.pageCount).arg(s.freelistPageCount).arg(sizeText(allocated))
        .arg(sizeText(reclaimable)).arg(s.walExists ? sizeText(s.walSize) : "Not present")
        .arg(s.shmExists ? sizeText(s.shmSize) : "Not present"));
    m_resultLabel->setText("Status refreshed. No integrity check has been run in this session.");
}

void DatabaseStatusDialog::runIntegrityCheck()
{
    qInfo() << "Database integrity check started.";
    setBusy(true, "Running integrity check... This can take time on a large database.");
    auto result = std::make_shared<DatabaseIntegrityCheckResult>();
    QThread* worker = QThread::create([result, path = m_databasePath]() {
        *result = DatabaseStatusService::runIntegrityCheck(path);
    });
    connect(worker, &QThread::finished, this, [this, result]() {
        setBusy(false);
        presentIntegrityResult(*result);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater); worker->start();
}

void DatabaseStatusDialog::runForeignKeyCheck()
{
    qInfo() << "Database foreign-key check started.";
    setBusy(true, "Running foreign-key check... This can take time on a large database.");
    auto result = std::make_shared<DatabaseForeignKeyCheckResult>();
    QThread* worker = QThread::create([result, path = m_databasePath]() {
        *result = DatabaseStatusService::runForeignKeyCheck(path);
    });
    connect(worker, &QThread::finished, this, [this, result]() {
        setBusy(false);
        presentForeignKeyResult(*result);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater); worker->start();
}

void DatabaseStatusDialog::showFailure(DatabaseDiagnosticOutcome outcome, const QString& details)
{
    hideRecoveryPanel();
    QString message;
    switch (outcome) {
    case DatabaseDiagnosticOutcome::BusyOrLocked:
        message = "The check could not complete because the database was busy or locked. Close other database operations and try again.";
        break;
    case DatabaseDiagnosticOutcome::DatabaseMissing:
        message = "The BrickSuite database file is missing. No database was created by this diagnostic operation.";
        break;
    case DatabaseDiagnosticOutcome::OpenFailure:
        message = "BrickSuite could not open the database for a read-only diagnostic. Review the Application Log for details.";
        break;
    case DatabaseDiagnosticOutcome::QueryFailure:
        message = "A database diagnostic query failed. The result is not a healthy check. Review the Application Log for details.";
        break;
    default:
        message = "The database diagnostic could not complete. Review the Application Log for technical details.";
        break;
    }
    qWarning() << "Database diagnostic failed:" << details;
    m_resultLabel->setText(message);
}

void DatabaseStatusDialog::presentIntegrityResult(const DatabaseIntegrityCheckResult& result)
{
    m_lastCheckType = "Integrity Check";
    m_lastOutcome = result.outcome;
    m_lastIssueCount = result.issueCount;
    m_lastElapsedMilliseconds = result.elapsedMilliseconds;
    m_lastDiagnosticRows = result.representativeIssues.mid(0, MaximumSummaryRows);
    m_lastCheckTime = QDateTime::currentDateTime();
    if (result.outcome == DatabaseDiagnosticOutcome::Healthy) {
        qInfo() << "Database integrity check completed healthy in" << result.elapsedMilliseconds << "ms.";
        m_resultLabel->setText("Integrity Check: Healthy. No integrity problems were found.");
        hideRecoveryPanel();
    } else if (result.outcome == DatabaseDiagnosticOutcome::IntegrityProblems) {
        qCritical() << "Database integrity check found" << result.issueCount << "issues in"
                    << result.elapsedMilliseconds << "ms. Representative results:"
                    << result.representativeIssues;
        m_resultLabel->setText(QString("Integrity Check: BrickSuite found %1 database consistency problem(s). No changes were made.").arg(result.issueCount));
        showRecoveryPanel(result.outcome);
    } else {
        showFailure(result.outcome, result.errorMessage);
    }
}

void DatabaseStatusDialog::presentForeignKeyResult(const DatabaseForeignKeyCheckResult& result)
{
    m_lastCheckType = "Foreign Key Check";
    m_lastOutcome = result.outcome;
    m_lastIssueCount = result.violationCount;
    m_lastElapsedMilliseconds = result.elapsedMilliseconds;
    m_lastDiagnosticRows.clear();
    for (const auto& violation : result.representativeViolations.mid(0, MaximumSummaryRows))
        m_lastDiagnosticRows.append(QString("%1 row %2 -> %3 (FK %4)").arg(violation.table).arg(violation.rowId).arg(violation.parentTable).arg(violation.foreignKeyIndex));
    m_lastCheckTime = QDateTime::currentDateTime();
    if (result.outcome == DatabaseDiagnosticOutcome::Healthy) {
        qInfo() << "Database foreign-key check completed healthy in" << result.elapsedMilliseconds << "ms.";
        m_resultLabel->setText("Foreign Key Check: Healthy. No relationship violations were found.");
        hideRecoveryPanel();
    } else if (result.outcome == DatabaseDiagnosticOutcome::ForeignKeyViolations) {
        qWarning() << "Database foreign-key check found" << result.violationCount << "violations in"
                   << result.elapsedMilliseconds << "ms. Representative results:" << m_lastDiagnosticRows;
        m_resultLabel->setText(QString("Foreign Key Check: BrickSuite found %1 database relationship violation(s). No changes were made.").arg(result.violationCount));
        showRecoveryPanel(result.outcome);
    } else {
        showFailure(result.outcome, result.errorMessage);
    }
}

void DatabaseStatusDialog::showRecoveryPanel(DatabaseDiagnosticOutcome outcome)
{
    if (outcome == DatabaseDiagnosticOutcome::IntegrityProblems) {
        m_recoveryHeading->setText("Integrity problems found");
        m_recoveryText->setText("BrickSuite detected database consistency problems. <b>No changes were made.</b> This may indicate damage to the SQLite database structure. Avoid unnecessary database changes until the problem is understood.<br><br><b>Recommended next steps</b><br>1. Review the Application Log.<br>2. Preserve the current database file for troubleshooting.<br>3. Restore a previously verified known-good backup if available.<br>4. Do not treat a new backup as a repair; it may preserve the detected problem.<br><br>BrickSuite verifies a selected backup and creates a pre-restore safety backup before replacement. A backup of the current database preserves its present state and may also preserve the detected problem.");
    } else {
        m_recoveryHeading->setText("Database relationship violations found");
        m_recoveryText->setText("BrickSuite found records whose required related records are missing. <b>No changes were made.</b> This is a logical data-consistency problem and does not necessarily mean the SQLite file itself is physically damaged.<br><br><b>Recommended next steps</b><br>1. Review the Application Log.<br>2. Avoid unnecessary relevant data changes.<br>3. Preserve the current database file for troubleshooting.<br>4. Restore a previously verified known-good backup if appropriate.<br><br>BrickSuite verifies a selected backup and creates a pre-restore safety backup before replacement. A backup of the current database preserves its present state and may also preserve the detected problem.");
    }
    m_recoveryPanel->show();
}

void DatabaseStatusDialog::hideRecoveryPanel()
{
    m_recoveryPanel->hide();
}

void DatabaseStatusDialog::openDatabaseFolder()
{
    const QFileInfo file(m_databasePath);
    if (!file.exists() || !file.dir().exists()
        || !QDesktopServices::openUrl(QUrl::fromLocalFile(file.absolutePath()))) {
        qWarning() << "Unable to open database folder:" << file.absolutePath();
        QMessageBox::warning(this, "Open Database Folder",
                             "BrickSuite could not open the folder containing the database.");
    }
}

QString DatabaseStatusDialog::diagnosticSummary() const
{
    QStringList lines;
    lines << AppVersion::displayVersion()
          << QString("Schema version: %1").arg(m_snapshot.schemaVersion)
          << QString("Database path: %1").arg(m_databasePath)
          << QString("Database file size: %1").arg(sizeText(m_snapshot.fileSize))
          << QString("Check type: %1").arg(m_lastCheckType)
          << QString("Check time: %1").arg(QLocale().toString(m_lastCheckTime, QLocale::ShortFormat))
          << QString("Outcome: %1").arg(m_lastOutcome == DatabaseDiagnosticOutcome::IntegrityProblems
                                             ? "Integrity problems" : "Foreign-key violations")
          << QString("Issue count: %1").arg(m_lastIssueCount)
          << QString("Elapsed time: %1 ms").arg(m_lastElapsedMilliseconds)
          << QString("Application Log: %1").arg(Logger::logFilePath())
          << "Representative diagnostics:";
    lines.append(m_lastDiagnosticRows);
    lines << "BrickSuite made no repairs or data changes.";
    return lines.join('\n');
}

void DatabaseStatusDialog::copyDiagnosticSummary()
{
    QApplication::clipboard()->setText(diagnosticSummary());
    m_resultLabel->setText("Diagnostic summary copied to the clipboard.");
}
