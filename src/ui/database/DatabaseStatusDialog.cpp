#include "DatabaseStatusDialog.h"

#include "../help/HelpManager.h"
#include "../help/HelpTopic.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>

#include <memory>

namespace
{
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
    m_foreignKeyButton = checkButtons->addButton("Run Foreign Key Check", QDialogButtonBox::ActionRole);
    integrityLayout->addWidget(checkButtons);
    layout->addWidget(integrity);

    auto* buttons = new QDialogButtonBox(this);
    m_refreshButton = buttons->addButton("Refresh", QDialogButtonBox::ActionRole);
    auto* folderButton = buttons->addButton("Open Database Folder", QDialogButtonBox::ActionRole);
    auto* backupButton = buttons->addButton("Backup...", QDialogButtonBox::ActionRole);
    auto* restoreButton = buttons->addButton("Restore...", QDialogButtonBox::ActionRole);
    auto* helpButton = buttons->addButton(QDialogButtonBox::Help);
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    connect(m_refreshButton, &QPushButton::clicked, this, &DatabaseStatusDialog::refresh);
    connect(m_integrityButton, &QPushButton::clicked, this, &DatabaseStatusDialog::runIntegrityCheck);
    connect(m_foreignKeyButton, &QPushButton::clicked, this, &DatabaseStatusDialog::runForeignKeyCheck);
    connect(folderButton, &QPushButton::clicked, this, [this]() {
        const QFileInfo file(m_databasePath);
        if (!file.exists() || !file.dir().exists()
            || !QDesktopServices::openUrl(QUrl::fromLocalFile(file.absolutePath()))) {
            qWarning() << "Unable to open database folder:" << file.absolutePath();
            QMessageBox::warning(this, "Open Database Folder",
                                 "BrickSuite could not open the folder containing the database.");
        }
    });
    connect(backupButton, &QPushButton::clicked, this, &DatabaseStatusDialog::backupRequested);
    connect(restoreButton, &QPushButton::clicked, this, &DatabaseStatusDialog::restoreRequested);
    connect(helpButton, &QPushButton::clicked, this, [this]() {
        HelpManager::showTopic(HelpTopic::DatabaseStatus, this);
    });
    auto* helpShortcut = new QShortcut(QKeySequence::HelpContents, this);
    connect(helpShortcut, &QShortcut::activated, this, [this]() {
        HelpManager::showTopic(HelpTopic::DatabaseStatus, this);
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    refresh();
}

bool DatabaseStatusDialog::event(QEvent* event)
{
    if (event->type() == QEvent::HelpRequest) {
        HelpManager::showTopic(HelpTopic::DatabaseStatus, this);
        return true;
    }
    return QDialog::event(event);
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
        if (result->outcome == DatabaseDiagnosticOutcome::Healthy) {
            qInfo() << "Database integrity check completed healthy in" << result->elapsedMilliseconds << "ms.";
            m_resultLabel->setText("Integrity Check: Healthy. No integrity problems were found.");
        } else if (result->outcome == DatabaseDiagnosticOutcome::IntegrityProblems) {
            qCritical() << "Database integrity check found" << result->issueCount << "issues in"
                        << result->elapsedMilliseconds << "ms. Representative results:"
                        << result->representativeIssues;
            m_resultLabel->setText(QString("Integrity Check: BrickSuite found %1 database consistency problem(s). BrickSuite did not repair anything. Review the Application Log and consider restoring a previously verified known-good backup.").arg(result->issueCount));
        } else showFailure(result->outcome, result->errorMessage);
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
        if (result->outcome == DatabaseDiagnosticOutcome::Healthy) {
            qInfo() << "Database foreign-key check completed healthy in" << result->elapsedMilliseconds << "ms.";
            m_resultLabel->setText("Foreign Key Check: Healthy. No relationship violations were found.");
        } else if (result->outcome == DatabaseDiagnosticOutcome::ForeignKeyViolations) {
            QStringList examples;
            for (const auto& violation : result->representativeViolations)
                examples.append(QString("%1 row %2 -> %3 (FK %4)").arg(violation.table).arg(violation.rowId).arg(violation.parentTable).arg(violation.foreignKeyIndex));
            qWarning() << "Database foreign-key check found" << result->violationCount << "violations in"
                       << result->elapsedMilliseconds << "ms. Representative results:" << examples;
            m_resultLabel->setText(QString("Foreign Key Check: BrickSuite found %1 database relationship violation(s). BrickSuite did not repair anything. Review the Application Log and consider restoring a previously verified known-good backup.").arg(result->violationCount));
        } else showFailure(result->outcome, result->errorMessage);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater); worker->start();
}

void DatabaseStatusDialog::showFailure(DatabaseDiagnosticOutcome outcome, const QString& details)
{
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
