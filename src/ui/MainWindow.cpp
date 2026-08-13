#include "MainWindow.h"

#include "../app/WorkspaceContext.h"
#include "../database/DatabaseManager.h"
#include "../models/Part.h"
#include "../models/Workspace.h"
#include "../repositories/PartRepository.h"
#include "../repositories/WorkspaceRepository.h"
#include "../services/RebrickableApiClient.h"
#include "../services/images/PartImageService.h"
#include "../settings/UserSettings.h"
#include "../ui/parts/PartDetailsDialog.h"
#include "catalog/PartsCatalogWidget.h"
#include "inventory/AddInventoryDialog.h"
#include "inventory/MyInventoryWidget.h"
#include "settings/SettingsDialog.h"
#include "storage/StorageWidget.h"

#include <QAction>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <optional>

MainWindow::MainWindow(WorkspaceContext& workspaceContext, QWidget* parent)
    : QMainWindow(parent)
    , m_workspaceContext(workspaceContext)
{
    setWindowTitle("BrickSuite");
    resize(1200, 800);

    m_tabWidget = new QTabWidget(this);

    // Workspace tab
    QWidget* workspaceTab = createWorkspaceTab();

    // Storage tab
    m_storageWidget = new StorageWidget(m_workspaceContext, m_tabWidget);

    // Parts Catalog tab
    m_partsCatalogWidget = new PartsCatalogWidget(m_tabWidget);

    // My Inventory tab
    m_myInventoryWidget = new MyInventoryWidget(m_workspaceContext, m_tabWidget);

    connect(m_partsCatalogWidget,
            &PartsCatalogWidget::addPartToInventoryRequested,
            this,
            [this](int partId) {
                if (!m_workspaceContext.hasCurrentWorkspace()) {
                    QMessageBox::warning(this,
                                         "BrickSuite",
                                         "Select a workspace before adding inventory.");

                    return;
                }

                AddInventoryDialog dialog(partId, m_workspaceContext, this);

                if (dialog.exec() == QDialog::Accepted) {
                    m_myInventoryWidget->refresh();

                    statusBar()->showMessage("Inventory updated.", 5000);
                }
            });

    m_tabWidget->addTab(workspaceTab, "Workspace");

    m_tabWidget->addTab(m_storageWidget, "Storage");

    m_tabWidget->addTab(m_partsCatalogWidget, "Parts Catalog");

    m_tabWidget->addTab(m_myInventoryWidget, "My Inventory");

    setCentralWidget(m_tabWidget);

    loadWorkspaces();

    /***** Menu bar *****/

    // File menu
    auto* fileMenu = menuBar()->addMenu("File");

    auto* backupDatabaseAction = fileMenu->addAction("Backup Database...");

    auto* restoreDatabaseAction = fileMenu->addAction("Restore Database...");

    connect(backupDatabaseAction, &QAction::triggered, this, [this]() {
        const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss");

        const QString defaultFileName = QString("BrickSuite_Backup_%1.db").arg(timestamp);

        QString initialDirectory = QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation);

        if (initialDirectory.isEmpty()) {
            initialDirectory = QDir::homePath();
        }

        const QString defaultPath = QDir(initialDirectory).filePath(defaultFileName);

        const QString backupPath = QFileDialog::getSaveFileName(this,
                                                                "Backup BrickSuite Database",
                                                                defaultPath,
                                                                "SQLite Database (*.db)");

        if (backupPath.isEmpty())
            return;

        QString finalBackupPath = backupPath;

        if (!finalBackupPath.endsWith(".db", Qt::CaseInsensitive)) {
            finalBackupPath += ".db";
        }

        QString errorMessage;

        if (!DatabaseManager::instance().backupDatabase(finalBackupPath, &errorMessage)) {
            QMessageBox::critical(this,
                                  "BrickSuite Database Backup",
                                  QString("The database backup could not be created.\n\n%1")
                                      .arg(errorMessage));

            return;
        }

        // Verify the backup
        QString verificationError;

        if (!DatabaseManager::instance().verifyDatabaseBackup(finalBackupPath, &verificationError)) {
            QMessageBox::warning(this,
                                 "BrickSuite Database Backup",
                                 QString("The database backup was created, "
                                         "but verification failed.\n\n%1\n\n"
                                         "Backup file:\n%2")
                                     .arg(verificationError, finalBackupPath));

            return;
        }

        QMessageBox::information(this,
                                 "BrickSuite Database Backup",
                                 QString("Database backup created and verified successfully.\n\n%1")
                                     .arg(finalBackupPath));

        statusBar()->showMessage("Database backup created.", 5000);
    });

    connect(restoreDatabaseAction, &QAction::triggered, this, [this]() {
        const QString backupPath = QFileDialog::getOpenFileName(this,
                                                                "Restore BrickSuite Database",
                                                                QString(),
                                                                "SQLite Database (*.db)");

        if (backupPath.isEmpty())
            return;

        //
        // Verify the selected backup before asking
        // the user to confirm the restore.
        //
        QString verificationError;

        if (!DatabaseManager::instance().verifyDatabaseBackup(backupPath, &verificationError)) {
            QMessageBox::critical(this,
                                  "BrickSuite Database Restore",
                                  QString("The selected backup is not a valid "
                                          "BrickSuite database.\n\n%1")
                                      .arg(verificationError));

            return;
        }

        const QMessageBox::StandardButton response
            = QMessageBox::warning(this,
                                   "Restore BrickSuite Database",
                                   QString("The selected backup has been verified.\n\n"
                                           "Restoring it will replace the current "
                                           "BrickSuite database.\n\n"
                                           "BrickSuite will automatically create and "
                                           "verify a pre-restore safety backup first.\n\n"
                                           "Restore from:\n%1\n\n"
                                           "Do you want to continue?")
                                       .arg(backupPath),
                                   QMessageBox::Yes | QMessageBox::No,
                                   QMessageBox::No);

        if (response != QMessageBox::Yes)
            return;

        //
        // Clear the current workspace before replacing
        // the underlying database.
        //
        m_workspaceContext.clearCurrentWorkspace();

        QString restoreError;

        if (!DatabaseManager::instance().restoreDatabase(backupPath, &restoreError)) {
            //
            // Reload the workspace list from whichever
            // database DatabaseManager recovered to.
            //
            loadWorkspaces();

            QMessageBox::critical(this,
                                  "BrickSuite Database Restore",
                                  QString("The database could not be restored.\n\n%1")
                                      .arg(restoreError));

            return;
        }

        //
        // The restored database is now open.
        //
        // Clear any details left from the old workspace
        // before reloading the workspace list.
        //
        m_nameEdit->clear();
        m_descriptionEdit->clear();

        loadWorkspaces();

        //
        // loadWorkspaces() may select the user's saved
        // default workspace or the only workspace.
        //
        // That selection will cause workspaceSelected()
        // to update WorkspaceContext, which in turn
        // refreshes widgets listening to
        // currentWorkspaceChanged().
        //
        if (!m_workspaceList->currentItem()) {
            m_workspaceContext.clearCurrentWorkspace();

            //
            // Explicit refresh for widgets that do not
            // depend solely on workspace selection.
            //
            if (m_partsCatalogWidget) {
                m_partsCatalogWidget->settingsChanged();
            }

            if (m_myInventoryWidget) {
                m_myInventoryWidget->refresh();
            }
        }

        QMessageBox::information(this,
                                 "BrickSuite Database Restore",
                                 QString("Database restored successfully.\n\n"
                                         "Source backup:\n%1")
                                     .arg(backupPath));

        statusBar()->showMessage("Database restored successfully.", 5000);
    });

    // Edit menu
    auto* editMenu = menuBar()->addMenu("Edit");

    auto* settingsAction = editMenu->addAction("Settings...");

    connect(settingsAction, &QAction::triggered, this, [this]() {
        SettingsDialog dialog(m_workspaceContext, this);

        connect(&dialog, &SettingsDialog::settingsChanged, this, [this]() {
            if (m_partsCatalogWidget) {
                m_partsCatalogWidget->settingsChanged();
            }

            if (m_myInventoryWidget) {
                m_myInventoryWidget->settingsChanged();
            }
        });

        dialog.exec();
    });

    // Test menu
    auto* testMenu = menuBar()->addMenu("Test");

    auto* rebrickableMenu = testMenu->addMenu("Rebrickable API");

    auto* partDetailsAction = rebrickableMenu->addAction("Part Details");

    auto* partDetailsDialogAction = rebrickableMenu->addAction("Part Details Dialog");

    connect(partDetailsAction, &QAction::triggered, this, [this]() {
        const QString apiKey = UserSettings::instance().rebrickableApiKey();

        if (apiKey.isEmpty()) {
            QMessageBox::warning(this,
                                 "Rebrickable API Test",
                                 "No Rebrickable API key is configured.");

            return;
        }

        auto* apiClient = new RebrickableApiClient(this);

        connect(apiClient,
                &RebrickableApiClient::partDetailsFinished,
                this,
                [this, apiClient](const RebrickableApiClient::PartDetailsResult& result) {
                    if (!result.success) {
                        QMessageBox::warning(this, "Rebrickable API Test", result.message);

                        apiClient->deleteLater();

                        return;
                    }

                    qDebug() << "Part:" << result.part.partNumber << result.part.name;

                    qDebug() << "Years:" << result.part.yearFrom << "-" << result.part.yearTo;

                    qDebug() << "Image:" << result.part.partImageUrl;

                    qDebug() << "Molds:" << result.part.molds;

                    qDebug() << "Alternates:" << result.part.alternates;

                    qDebug() << "Print count:" << result.part.prints.size();

                    qDebug() << "External IDs:" << result.part.externalIds;

                    QMessageBox::information(this,
                                             "Rebrickable API Test",
                                             QString("Part details retrieved successfully.\n\n"
                                                     "Part: %1\n"
                                                     "Name: %2\n"
                                                     "Years: %3 - %4\n"
                                                     "Molds: %5\n"
                                                     "Alternates: %6\n"
                                                     "Prints: %7")
                                                 .arg(result.part.partNumber)
                                                 .arg(result.part.name)
                                                 .arg(result.part.yearFrom)
                                                 .arg(result.part.yearTo)
                                                 .arg(result.part.molds.size())
                                                 .arg(result.part.alternates.size())
                                                 .arg(result.part.prints.size()));

                    apiClient->deleteLater();
                });

        apiClient->getPartDetails("3001", apiKey);
    });

    auto* partImageAction = rebrickableMenu->addAction("Part Image");

    connect(partImageAction, &QAction::triggered, this, [this]() {
        const QString apiKey = UserSettings::instance().rebrickableApiKey();

        if (apiKey.isEmpty()) {
            QMessageBox::warning(this,
                                 "Rebrickable API Test",
                                 "No Rebrickable API key is configured.");

            return;
        }

        auto* apiClient = new RebrickableApiClient(this);

        connect(apiClient,
                &RebrickableApiClient::partDetailsFinished,
                this,
                [this, apiClient](const RebrickableApiClient::PartDetailsResult& result) {
                    if (!result.success) {
                        QMessageBox::warning(this, "Part Image Test", result.message);

                        apiClient->deleteLater();

                        return;
                    }

                    auto* imageService = new PartImageService(this);

                    connect(imageService,
                            &PartImageService::imageReady,
                            this,
                            [this, imageService](const QString& partNumber,
                                                 const QString& imagePath) {
                                QMessageBox::information(this,
                                                         "Part Image Test",
                                                         QString(
                                                             "Part image cached successfully.\n\n"
                                                             "Part: %1\n"
                                                             "Path: %2")
                                                             .arg(partNumber)
                                                             .arg(imagePath));

                                imageService->deleteLater();
                            });

                    connect(imageService,
                            &PartImageService::imageFailed,
                            this,
                            [this, imageService](const QString& partNumber, const QString& message) {
                                QMessageBox::warning(this,
                                                     "Part Image Test",
                                                     QString("Part: %1\n\n%2")
                                                         .arg(partNumber)
                                                         .arg(message));

                                imageService->deleteLater();
                            });

                    imageService->requestPartImage(result.part.partNumber, result.part.partImageUrl);

                    apiClient->deleteLater();
                });

        apiClient->getPartDetails("3001", apiKey);
    });

    connect(partDetailsDialogAction, &QAction::triggered, this, [this]() {
        PartRepository repository;

        const std::optional<Part> part = repository.getByPartNumber("3001");

        if (!part) {
            QMessageBox::warning(this,
                                 "Part Details Test",
                                 "Unable to locate part 3001 in the BrickSuite catalog.");

            return;
        }

        PartDetailsDialog dialog(part->id(), this);

        dialog.exec();
    });

    statusBar()->showMessage("BrickSuite Version 1.0");
}

void MainWindow::loadWorkspaces()
{
    m_workspaceList->clear();

    WorkspaceRepository repository;

    const QList<Workspace> workspaces = repository.getAll();

    const int defaultWorkspaceId = UserSettings::instance().defaultWorkspaceId();

    QListWidgetItem* defaultItem = nullptr;

    for (const Workspace& workspace : workspaces) {
        auto* item = new QListWidgetItem(workspace.name());

        item->setData(Qt::UserRole, workspace.id());

        m_workspaceList->addItem(item);

        if (workspace.id() == defaultWorkspaceId) {
            defaultItem = item;
        }
    }

    // Selection priority:
    //
    // 1. User's saved default workspace.
    // 2. If there is only one workspace, select it automatically.
    // 3. Otherwise leave the selection empty.
    if (defaultItem) {
        m_workspaceList->setCurrentItem(defaultItem);
    } else if (m_workspaceList->count() == 1) {
        m_workspaceList->setCurrentRow(0);
    }
}

void MainWindow::workspaceSelected()
{
    QListWidgetItem* item = m_workspaceList->currentItem();

    if (!item) {
        m_workspaceContext.clearCurrentWorkspace();
        return;
    }

    const int workspaceId = item->data(Qt::UserRole).toInt();

    WorkspaceRepository repository;

    const std::optional<Workspace> workspace = repository.getById(workspaceId);

    if (!workspace) {
        m_workspaceContext.clearCurrentWorkspace();

        QMessageBox::warning(this, "BrickSuite", "Unable to load the selected workspace.");

        return;
    }

    m_workspaceContext.setCurrentWorkspaceId(workspace->id());

    m_nameEdit->setText(workspace->name());

    m_descriptionEdit->setPlainText(workspace->description());

    statusBar()->showMessage(QString("Current Workspace: %1").arg(workspace->name()));
}
void MainWindow::addWorkspace()
{
    const QString name = m_nameEdit->text().trimmed();

    const QString description = m_descriptionEdit->toPlainText().trimmed();

    if (name.isEmpty()) {
        QMessageBox::warning(this, "BrickSuite", "Please enter a workspace name.");

        return;
    }

    Workspace workspace;

    workspace.setName(name);
    workspace.setDescription(description);
    workspace.setIsActive(true);

    WorkspaceRepository repository;

    if (!repository.create(workspace)) {
        QMessageBox::critical(this, "BrickSuite", "Unable to create the workspace.");

        return;
    }

    m_nameEdit->clear();
    m_descriptionEdit->clear();

    loadWorkspaces();

    statusBar()->showMessage(QString("Workspace created: %1").arg(workspace.name()), 5000);
}

QWidget* MainWindow::createWorkspaceTab()
{
    auto* tab = new QWidget(m_tabWidget);

    auto* mainLayout = new QHBoxLayout(tab);

    // Left side: workspace list
    auto* listLayout = new QVBoxLayout();

    auto* workspaceLabel = new QLabel("Workspaces", tab);

    m_workspaceList = new QListWidget(tab);

    listLayout->addWidget(workspaceLabel);

    listLayout->addWidget(m_workspaceList);

    // Right side: workspace details
    auto* formLayout = new QVBoxLayout();

    auto* nameLabel = new QLabel("Workspace Name", tab);

    m_nameEdit = new QLineEdit(tab);

    auto* descriptionLabel = new QLabel("Description", tab);

    m_descriptionEdit = new QTextEdit(tab);

    m_addButton = new QPushButton("Add Workspace", tab);

    formLayout->addWidget(nameLabel);

    formLayout->addWidget(m_nameEdit);

    formLayout->addWidget(descriptionLabel);

    formLayout->addWidget(m_descriptionEdit);

    formLayout->addWidget(m_addButton);

    formLayout->addStretch();

    mainLayout->addLayout(listLayout, 1);

    mainLayout->addLayout(formLayout, 2);

    connect(m_addButton, &QPushButton::clicked, this, &MainWindow::addWorkspace);

    connect(m_workspaceList,
            &QListWidget::itemSelectionChanged,
            this,
            &MainWindow::workspaceSelected);

    return tab;
}