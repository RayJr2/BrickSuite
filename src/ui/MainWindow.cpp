/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#include "MainWindow.h"

#include "about/AboutDialog.h"
#include "help/HelpManager.h"
#include "mappings/BrickLinkColorMappingStatusDialog.h"
#include "mappings/BrickLinkPartMappingTestDialog.h"
#include "logging/LogViewerDialog.h"

#include "../app/WorkspaceContext.h"
#include "../database/DatabaseManager.h"

#include "../models/Build.h"
#include "../models/Part.h"
#include "../models/SetCatalogItem.h"
#include "../models/Workspace.h"

#include "../repositories/BuildRepository.h"
#include "../repositories/PartRepository.h"
#include "../repositories/SetCatalogRepository.h"
#include "../repositories/WorkspaceRepository.h"

#include "../services/RebrickableApiClient.h"
#include "../api/brickset/BricksetService.h"
#include "../services/images/BackgroundPartColorImageCacheService.h"
#include "../services/images/PartImageService.h"
#include "../services/mappings/BrickLinkMappingService.h"

#include "../settings/UserSettings.h"

#include "../ui/parts/PartDetailsDialog.h"

#include "../core/AppVersion.h"

#include "builds/BuildsWidget.h"
#include "catalog/PartsCatalogWidget.h"
#include "catalog/SetsCatalogWidget.h"
#include "inventory/AddInventoryDialog.h"
#include "inventory/MyInventoryWidget.h"
#include "settings/SettingsDialog.h"
#include "storage/StorageWidget.h"

#include <QAction>
#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <optional>

namespace {

bool windowIsVisibleOnAnyScreen(const QRect& windowGeometry)
{
    const QList<QScreen*> screens = QGuiApplication::screens();

    for (QScreen* screen : screens) {
        if (!screen)
            continue;

        const QRect available = screen->availableGeometry();

        if (available.intersects(windowGeometry)) {
            return true;
        }
    }

    return false;
}

} // namespace

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

    // Sets Catalog tab
    m_setsCatalogWidget = new SetsCatalogWidget(m_tabWidget);

    connect(m_setsCatalogWidget,
            &SetsCatalogWidget::createBuildRequested,
            this,
            [this](int setCatalogId, const QString& inventoryMode) {
                if (!m_workspaceContext.hasCurrentWorkspace()) {
                    QMessageBox::warning(this,
                                         "Create Build",
                                         "Select a workspace before creating "
                                         "a Build from the Sets Catalog.");

                    return;
                }

                SetCatalogRepository setRepository;

                const std::optional<SetCatalogItem> set = setRepository.getById(setCatalogId);

                if (!set) {
                    QMessageBox::critical(this,
                                          "Create Build",
                                          "Unable to load the selected "
                                          "Sets Catalog record.");

                    return;
                }

                QString modeText;

                if (inventoryMode == "Stock") {
                    modeText = "Build from Stock";
                } else if (inventoryMode == "CompleteSet") {
                    modeText = "Complete Set";
                } else {
                    QMessageBox::critical(this,
                                          "Create Build",
                                          "The requested Inventory Mode "
                                          "is invalid.");

                    return;
                }

                const QMessageBox::StandardButton response
                    = QMessageBox::question(this,
                                            "Create Build",
                                            QString("Create this Build?\n\n"
                                                    "Set: %1\n"
                                                    "Name: %2\n"
                                                    "Inventory Mode: %3\n\n"
                                                    "The new Build will be created "
                                                    "with status Planned.")
                                                .arg(set->setNumber())
                                                .arg(set->name())
                                                .arg(modeText),
                                            QMessageBox::Yes | QMessageBox::No,
                                            QMessageBox::Yes);

                if (response != QMessageBox::Yes)
                    return;

                Build build;

                build.setWorkspaceId(m_workspaceContext.currentWorkspaceId());

                build.setBuildType("Set");

                build.setSetNumber(set->setNumber());

                build.setName(set->name());

                build.setInventoryMode(inventoryMode);

                build.setStatus("Planned");

                BuildRepository buildRepository;

                if (!buildRepository.create(build)) {
                    QMessageBox::critical(this, "Create Build", "Unable to create the Build.");

                    return;
                }

                //
                // Move directly into the existing Build
                // workflow and select the new record.
                //
                m_tabWidget->setCurrentWidget(m_buildsWidget);

                m_buildsWidget->selectBuild(build.id());

                statusBar()->showMessage(QString("Build created: %1").arg(build.name()), 5000);
            });

    // My Inventory tab
    m_myInventoryWidget = new MyInventoryWidget(m_workspaceContext, m_tabWidget);

    // Builds tab
    m_buildsWidget = new BuildsWidget(m_workspaceContext, m_tabWidget);

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

                    if (m_backgroundPartColorImageCacheService) {
                        m_backgroundPartColorImageCacheService->rebuildQueue();
                    }

                    statusBar()->showMessage("Inventory updated.", 5000);
                }
            });

    m_tabWidget->addTab(workspaceTab, "Workspace");

    m_tabWidget->addTab(m_storageWidget, "Storage");

    m_tabWidget->addTab(m_partsCatalogWidget, "Parts Catalog");

    m_tabWidget->addTab(m_setsCatalogWidget, "Sets Catalog");

    m_tabWidget->addTab(m_myInventoryWidget, "My Inventory");

    m_tabWidget->addTab(m_buildsWidget, "Builds");

    setCentralWidget(m_tabWidget);

    loadWorkspaces();

    //
    // Slowly populate actual-color images for
    // Part/Color combinations in My Loose Inventory.
    //
    m_backgroundPartColorImageCacheService
        = new BackgroundPartColorImageCacheService(m_workspaceContext, this);

    connect(m_backgroundPartColorImageCacheService,
            &BackgroundPartColorImageCacheService::partColorImageCached,
            m_myInventoryWidget,
            &MyInventoryWidget::updatePartColorImage);

    //
    // The background queue is a snapshot of inventory. Rebuild it whenever
    // inventory changes so newly added/imported Part+Color combinations are
    // picked up without requiring an application restart.
    //
    connect(m_myInventoryWidget,
            &MyInventoryWidget::inventoryChanged,
            m_backgroundPartColorImageCacheService,
            &BackgroundPartColorImageCacheService::rebuildQueue);

    m_backgroundPartColorImageCacheService->start();

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

    // Help menu
    auto* helpMenu = menuBar()->addMenu("Help");

    auto* helpContentsAction = helpMenu->addAction("BrickSuite Help");
    helpContentsAction->setShortcut(QKeySequence::HelpContents);
    helpContentsAction->setShortcutContext(Qt::ApplicationShortcut);

    connect(helpContentsAction, &QAction::triggered, this, [this]() {
        HelpTopic topic = HelpTopic::GettingStarted;

        QWidget* currentWidget = m_tabWidget ? m_tabWidget->currentWidget() : nullptr;

        if (currentWidget == m_storageWidget) {
            topic = HelpTopic::Storage;
        } else if (currentWidget == m_partsCatalogWidget) {
            topic = HelpTopic::PartsCatalog;
        } else if (currentWidget == m_setsCatalogWidget) {
            topic = HelpTopic::SetsCatalog;
        } else if (currentWidget == m_myInventoryWidget) {
            topic = HelpTopic::Inventory;
        } else if (currentWidget == m_buildsWidget) {
            topic = HelpTopic::Builds;
        }

        HelpManager::showTopic(topic, this);
    });

    helpMenu->addSeparator();

    auto* applicationLogAction = helpMenu->addAction("Application Log...");

    connect(applicationLogAction, &QAction::triggered, this, [this]() {
        //
        // Keep one non-modal viewer alive so it can sit on another monitor
        // while BrickSuite continues to be used and tested.
        //
        if (m_logViewerDialog) {
            m_logViewerDialog->show();
            m_logViewerDialog->raise();
            m_logViewerDialog->activateWindow();
            return;
        }

        m_logViewerDialog = new LogViewerDialog(this);
        m_logViewerDialog->setAttribute(Qt::WA_DeleteOnClose);
        m_logViewerDialog->setWindowModality(Qt::NonModal);
        m_logViewerDialog->setModal(false);

        connect(m_logViewerDialog, &QObject::destroyed, this, [this]() {
            m_logViewerDialog = nullptr;
        });

        m_logViewerDialog->show();
    });

    helpMenu->addSeparator();

    auto* aboutAction = helpMenu->addAction("About BrickSuite...");

    connect(aboutAction, &QAction::triggered, this, [this]() {
        AboutDialog dialog(this);
        dialog.exec();
    });

    // Test menu
    auto* testMenu = menuBar()->addMenu("Test");

    auto* rebrickableMenu = testMenu->addMenu("Rebrickable API");

    auto* partDetailsAction = rebrickableMenu->addAction("Part Details");

    auto* partDetailsDialogAction = rebrickableMenu->addAction("Part Details Dialog");

    auto* partImageAction = rebrickableMenu->addAction("Part Image");

    auto* setDetailsAction = rebrickableMenu->addAction("Set Details");

    auto* setPartsAction = rebrickableMenu->addAction("Set Parts");

    auto* partColorImageAction = rebrickableMenu->addAction("Part Color Image");

    rebrickableMenu->addSeparator();

    auto* throttleTestAction = rebrickableMenu->addAction("Throttle Test");

    auto* bricksetMenu = testMenu->addMenu("Brickset");

    auto* bricksetSetDetailsAction = bricksetMenu->addAction("Set Details");

    auto* bricksetKeyUsageAction = bricksetMenu->addAction("Key Usage Stats");

    auto* brickLinkMenu = testMenu->addMenu("BrickLink Export");

    auto* refreshBrickLinkColorMappingsAction =
        brickLinkMenu->addAction("Refresh Color Mappings from Rebrickable");

    auto* viewBrickLinkColorMappingStatusAction =
        brickLinkMenu->addAction("View Color Mapping Status");

    auto* testBrickLinkPartResolverAction =
        brickLinkMenu->addAction("Test Part Number Resolver");

    connect(testBrickLinkPartResolverAction,
            &QAction::triggered,
            this,
            [this]() {
                BrickLinkPartMappingTestDialog dialog(this);
                dialog.exec();
            });

    connect(viewBrickLinkColorMappingStatusAction,
            &QAction::triggered,
            this,
            [this]() {
                BrickLinkColorMappingStatusDialog dialog(this);
                dialog.exec();
            });

    connect(refreshBrickLinkColorMappingsAction,
            &QAction::triggered,
            this,
            [this]() {
                const QString apiKey =
                    UserSettings::instance().rebrickableApiKey().trimmed();

                if (apiKey.isEmpty()) {
                    QMessageBox::warning(
                        this,
                        "BrickLink Color Mappings",
                        "A Rebrickable API key is required to refresh "
                        "BrickLink color mappings.");
                    return;
                }

                auto* mappingService = new BrickLinkMappingService(this);

                connect(mappingService,
                        &BrickLinkMappingService::colorMappingsRefreshed,
                        this,
                        [this, mappingService](
                            const BrickLinkMappingService::ColorRefreshResult& result) {
                            if (!result.success) {
                                QMessageBox::warning(
                                    this,
                                    "BrickLink Color Mappings",
                                    result.message);
                                mappingService->deleteLater();
                                return;
                            }

                            QMessageBox::information(
                                this,
                                "BrickLink Color Mappings",
                                QString("BrickLink color mapping refresh completed.\n\n"
                                        "Mapping source: Rebrickable reference data\n"
                                        "BrickLink API access used: No\n\n"
                                        "BrickSuite colors: %1\n"
                                        "Rebrickable colors returned: %2\n"
                                        "Matched BrickSuite colors: %3\n\n"
                                        "Mapped: %4\n"
                                        "Unsupported: %5\n"
                                        "Unknown: %6")
                                    .arg(result.brickSuiteColors)
                                    .arg(result.rebrickableColors)
                                    .arg(result.matchedBrickSuiteColors)
                                    .arg(result.mapped)
                                    .arg(result.unsupported)
                                    .arg(result.unknown));

                            mappingService->deleteLater();
                        });

                mappingService->refreshColorMappings(apiKey);
            });

    connect(bricksetKeyUsageAction, &QAction::triggered, this, [this]() {
        const QString apiKey = UserSettings::instance().bricksetApiKey();

        if (apiKey.isEmpty()) {
            QMessageBox::warning(this,
                                 "Brickset Key Usage Stats",
                                 "No Brickset API key is configured.");
            return;
        }

        auto* bricksetService = new BricksetService(this);

        connect(bricksetService,
                &BricksetService::keyUsageStatsFinished,
                this,
                [this, bricksetService](const BricksetService::KeyUsageResult& result) {
                    if (!result.success) {
                        QMessageBox::warning(this,
                                             "Brickset Key Usage Stats",
                                             result.message);
                        bricksetService->deleteLater();
                        return;
                    }

                    qInfo() << "Brickset key usage stats retrieved."
                            << "Today:" << result.todayCount
                            << "Entries:" << result.entries.size()
                            << "Session getSets calls:"
                            << BricksetService::sessionGetSetsCallCount();

                    QMessageBox::information(
                        this,
                        "Brickset Key Usage Stats",
                        QString("Today's getSets calls: %1\n"
                                "Usage entries returned: %2\n"
                                "BrickSuite getSets calls this session: %3")
                            .arg(result.todayCount)
                            .arg(result.entries.size())
                            .arg(BricksetService::sessionGetSetsCallCount()));

                    bricksetService->deleteLater();
                });

        bricksetService->getKeyUsageStats(apiKey);
    });

    connect(bricksetSetDetailsAction, &QAction::triggered, this, [this]() {
        const QString apiKey = UserSettings::instance().bricksetApiKey();

        if (apiKey.isEmpty()) {
            QMessageBox::warning(this,
                                 "Brickset Set Details",
                                 "No Brickset API key is configured.");
            return;
        }

        bool ok = false;

        const QString setNumber =
            QInputDialog::getText(this,
                                  "Brickset Set Details",
                                  "Set number:",
                                  QLineEdit::Normal,
                                  QStringLiteral("10300-1"),
                                  &ok)
                .trimmed();

        if (!ok || setNumber.isEmpty())
            return;

        auto* bricksetService = new BricksetService(this);

        connect(bricksetService,
                &BricksetService::setDetailsFinished,
                this,
                [this, bricksetService](const BricksetService::SetDetailsResult& result) {
                    if (!result.success) {
                        qWarning() << "Brickset set lookup failed."
                                   << "Set:" << result.requestedSetNumber
                                   << "HTTP:" << result.httpStatusCode
                                   << "Message:" << result.message;

                        QMessageBox::warning(this, "Brickset Set Details", result.message);
                        bricksetService->deleteLater();
                        return;
                    }

                    const BricksetService::SetDetails& set = result.set;

                    qInfo() << "Brickset set lookup succeeded."
                            << "Set:" << set.fullSetNumber
                            << "BricksetSetId:" << set.bricksetSetId
                            << "Name:" << set.name
                            << "Year:" << set.year
                            << "Theme:" << set.theme
                            << "Subtheme:" << set.subtheme
                            << "Pieces:" << set.pieces
                            << "Minifigs:" << set.minifigs
                            << "AdditionalImages:" << set.additionalImageCount
                            << "Instructions:" << set.instructionsCount;

                    QMessageBox::information(
                        this,
                        "Brickset Set Details",
                        QString("Set: %1\n"
                                "Brickset Set ID: %2\n"
                                "Name: %3\n"
                                "Year: %4\n"
                                "Theme: %5\n"
                                "Subtheme: %6\n"
                                "Pieces: %7\n"
                                "Minifigs: %8\n"
                                "Additional Images: %9\n"
                                "Instructions: %10")
                            .arg(set.fullSetNumber)
                            .arg(set.bricksetSetId)
                            .arg(set.name)
                            .arg(set.year)
                            .arg(set.theme)
                            .arg(set.subtheme)
                            .arg(set.pieces)
                            .arg(set.minifigs)
                            .arg(set.additionalImageCount)
                            .arg(set.instructionsCount));

                    bricksetService->deleteLater();
                });

        bricksetService->getSetDetails(setNumber, apiKey);
    });

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

    connect(throttleTestAction, &QAction::triggered, this, [this]() {
        const QString apiKey = UserSettings::instance().rebrickableApiKey();

        if (apiKey.isEmpty()) {
            QMessageBox::warning(this,
                                 "Rebrickable Throttle Test",
                                 "No Rebrickable API key is configured.");

            return;
        }

        if (RebrickableApiClient::isSessionBlocked()) {
            QMessageBox::warning(this,
                                 "Rebrickable Throttle Test",
                                 RebrickableApiClient::sessionBlockReason());

            return;
        }

        const int intervalMs = UserSettings::instance().rebrickableMinimumRequestIntervalMs();

        const QMessageBox::StandardButton response
            = QMessageBox::question(this,
                                    "Rebrickable Throttle Test",
                                    QString("This test will queue 3 Rebrickable API "
                                            "requests immediately.\n\n"
                                            "BrickSuite should dispatch them no faster "
                                            "than the configured minimum interval of "
                                            "%1 ms.\n\n"
                                            "Continue?")
                                        .arg(intervalMs),
                                    QMessageBox::Yes | QMessageBox::No,
                                    QMessageBox::No);

        if (response != QMessageBox::Yes)
            return;

        auto* apiClient = new RebrickableApiClient(this);

        auto* completedCount = new int(0);

        connect(apiClient,
                &RebrickableApiClient::partDetailsFinished,
                this,
                [this, apiClient, completedCount, intervalMs](
                    const RebrickableApiClient::PartDetailsResult& result) {
                    ++(*completedCount);

                    qDebug() << "Throttle test result" << *completedCount << ":"
                             << result.part.partNumber << "HTTP" << result.httpStatusCode
                             << "Success" << result.success;

                    if (!result.success) {
                        qWarning() << "Throttle test request failed:" << result.message;
                    }

                    if (*completedCount < 3)
                        return;

                    const bool sessionBlocked = RebrickableApiClient::isSessionBlocked();

                    QString message;

                    if (sessionBlocked) {
                        message = QString("Throttle test completed, but the "
                                          "Rebrickable circuit breaker was "
                                          "tripped.\n\n%1")
                                      .arg(RebrickableApiClient::sessionBlockReason());
                    } else {
                        message = QString("Throttle test completed.\n\n"
                                          "3 API requests were queued.\n"
                                          "Configured minimum interval: %1 ms\n\n"
                                          "Check the Application Output for the "
                                          "'Rebrickable API GET dispatched' "
                                          "timestamps.")
                                      .arg(intervalMs);
                    }

                    QMessageBox::information(this, "Rebrickable Throttle Test", message);

                    delete completedCount;

                    apiClient->deleteLater();
                });

        //
        // These calls are intentionally made back-to-back.
        // RebrickableApiClient must serialize their actual
        // network dispatch through its shared request queue.
        //
        apiClient->getPartDetails("3001", apiKey);

        apiClient->getPartDetails("3002", apiKey);

        apiClient->getPartDetails("3003", apiKey);
    });

    connect(setDetailsAction, &QAction::triggered, this, [this]() {
        const QString apiKey = UserSettings::instance().rebrickableApiKey();

        if (apiKey.isEmpty()) {
            QMessageBox::warning(this,
                                 "Rebrickable Set Details",
                                 "No Rebrickable API key is configured.");

            return;
        }

        auto* apiClient = new RebrickableApiClient(this);

        connect(apiClient,
                &RebrickableApiClient::setDetailsFinished,
                this,
                [this, apiClient](const RebrickableApiClient::SetDetailsResult& result) {
                    if (!result.success) {
                        QMessageBox::warning(this, "Rebrickable Set Details", result.message);

                        apiClient->deleteLater();

                        return;
                    }

                    qDebug() << "Set:" << result.set.setNumber;

                    qDebug() << "Name:" << result.set.name;

                    qDebug() << "Year:" << result.set.year;

                    qDebug() << "Theme:" << result.set.themeId;

                    qDebug() << "Parts:" << result.set.numberOfParts;

                    qDebug() << "Image:" << result.set.setImageUrl;

                    QMessageBox::information(this,
                                             "Rebrickable Set Details",
                                             QString("Set: %1\n"
                                                     "Name: %2\n"
                                                     "Year: %3\n"
                                                     "Theme ID: %4\n"
                                                     "Parts: %5")
                                                 .arg(result.set.setNumber)
                                                 .arg(result.set.name)
                                                 .arg(result.set.year)
                                                 .arg(result.set.themeId)
                                                 .arg(result.set.numberOfParts));

                    apiClient->deleteLater();
                });

        apiClient->getSetDetails("77244-1", apiKey);
    });

    connect(setPartsAction, &QAction::triggered, this, [this]() {
        const QString apiKey = UserSettings::instance().rebrickableApiKey();

        if (apiKey.isEmpty()) {
            QMessageBox::warning(this,
                                 "Rebrickable Set Parts",
                                 "No Rebrickable API key is configured.");

            return;
        }

        auto* apiClient = new RebrickableApiClient(this);

        connect(apiClient,
                &RebrickableApiClient::setPartsFinished,
                this,
                [this, apiClient](const RebrickableApiClient::SetPartsResult& result) {
                    if (!result.success) {
                        QMessageBox::warning(this, "Rebrickable Set Parts", result.message);

                        apiClient->deleteLater();

                        return;
                    }

                    int regularRows = 0;
                    int spareRows = 0;

                    int regularQuantity = 0;
                    int spareQuantity = 0;

                    for (const auto& part : result.parts) {
                        if (part.isSpare) {
                            ++spareRows;
                            spareQuantity += part.quantity;
                        } else {
                            ++regularRows;
                            regularQuantity += part.quantity;
                        }

                        qDebug() << part.partNumber << part.colorName << "Qty" << part.quantity
                                 << "Spare" << part.isSpare;
                    }

                    qDebug() << "Set Parts count:" << result.totalCount;

                    qDebug() << "Rows returned:" << result.parts.size();

                    qDebug() << "Regular quantity:" << regularQuantity;

                    qDebug() << "Spare quantity:" << spareQuantity;

                    qDebug() << "Next URL:" << result.nextUrl;

                    QMessageBox::information(this,
                                             "Rebrickable Set Parts",
                                             QString("Set: %1\n\n"
                                                     "API row count: %2\n"
                                                     "Rows returned: %3\n\n"
                                                     "Regular rows: %4\n"
                                                     "Regular pieces: %5\n\n"
                                                     "Spare rows: %6\n"
                                                     "Spare pieces: %7\n\n"
                                                     "More pages: %8")
                                                 .arg(result.setNumber)
                                                 .arg(result.totalCount)
                                                 .arg(result.parts.size())
                                                 .arg(regularRows)
                                                 .arg(regularQuantity)
                                                 .arg(spareRows)
                                                 .arg(spareQuantity)
                                                 .arg(result.nextUrl.isEmpty() ? "No" : "Yes"));

                    apiClient->deleteLater();
                });

        apiClient->getSetParts("77244-1", apiKey);
    });

    connect(partColorImageAction, &QAction::triggered, this, [this]() {
        const QString apiKey = UserSettings::instance().rebrickableApiKey();

        if (apiKey.isEmpty()) {
            QMessageBox::warning(this,
                                 "Part Color Image Test",
                                 "No Rebrickable API key is configured.");

            return;
        }

        //
        // Change these two values if you want to
        // test a specific Part/Color combination
        // that exists in My Loose Inventory.
        //
        const QString partNumber = "3001";

        const int rebrickableColorId = 14; //Yellow

        auto* apiClient = new RebrickableApiClient(this);

        connect(apiClient,
                &RebrickableApiClient::partColorDetailsFinished,
                this,
                [this, apiClient](const RebrickableApiClient::PartColorDetailsResult& result) {
                    if (!result.success) {
                        QMessageBox::warning(this, "Part Color Image Test", result.message);

                        apiClient->deleteLater();

                        return;
                    }

                    if (result.partColor.partImageUrl.isEmpty()) {
                        QMessageBox::warning(this,
                                             "Part Color Image Test",
                                             "Rebrickable returned no "
                                             "Part Color image URL.");

                        apiClient->deleteLater();

                        return;
                    }

                    auto* imageService = new PartImageService(this);

                    connect(imageService,
                            &PartImageService::partColorImageReady,
                            this,
                            [this, imageService, apiClient](const QString& partNumber,
                                                            int rebrickableColorId,
                                                            const QString& imagePath) {
                                QMessageBox::information(this,
                                                         "Part Color Image Test",
                                                         QString("Colored Part image "
                                                                 "cached successfully.\n\n"
                                                                 "Part: %1\n"
                                                                 "Rebrickable Color ID: %2\n"
                                                                 "Path:\n%3")
                                                             .arg(partNumber)
                                                             .arg(rebrickableColorId)
                                                             .arg(imagePath));

                                if (m_myInventoryWidget) {
                                    m_myInventoryWidget->refresh();
                                }

                                imageService->deleteLater();

                                apiClient->deleteLater();
                            });

                    connect(imageService,
                            &PartImageService::partColorImageFailed,
                            this,
                            [this, imageService, apiClient](const QString& partNumber,
                                                            int rebrickableColorId,
                                                            const QString& message) {
                                QMessageBox::warning(this,
                                                     "Part Color Image Test",
                                                     QString("Part: %1\n"
                                                             "Color ID: %2\n\n"
                                                             "%3")
                                                         .arg(partNumber)
                                                         .arg(rebrickableColorId)
                                                         .arg(message));

                                imageService->deleteLater();

                                apiClient->deleteLater();
                            });

                    imageService->requestPartColorImage(result.partColor.partNumber,
                                                        result.partColor.rebrickableColorId,
                                                        result.partColor.partImageUrl);
                });

        apiClient->getPartColorDetails(partNumber, rebrickableColorId, apiKey);
    });

    //
    // Restore the Main Window geometry and state from
    // the previous BrickSuite session.
    //
    // The existing 1200 x 800 size remains the
    // first-run fallback.
    //
    const QByteArray savedGeometry = UserSettings::instance().mainWindowGeometry();

    if (!savedGeometry.isEmpty()) {
        restoreGeometry(savedGeometry);
    }

    const QByteArray savedState = UserSettings::instance().mainWindowState();

    if (!savedState.isEmpty()) {
        restoreState(savedState);
    }

    //
    // A previously used monitor may no longer exist.
    //
    // Example:
    //   BrickSuite was closed on an external monitor,
    //   then reopened with only the laptop display.
    //
    // If the restored window does not intersect any
    // currently available screen, move it to the
    // primary screen.
    //
    if (!windowIsVisibleOnAnyScreen(frameGeometry())) {
        QScreen* primaryScreen = QGuiApplication::primaryScreen();

        if (primaryScreen) {
            const QRect available = primaryScreen->availableGeometry();

            resize(qMin(width(), available.width()), qMin(height(), available.height()));

            move(available.x() + (available.width() - width()) / 2,
                 available.y() + (available.height() - height()) / 2);
        }
    }

    //statusBar()->showMessage("BrickSuite Version 1.0");
    statusBar()->showMessage(AppVersion::displayVersion());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    UserSettings::instance().setMainWindowGeometry(saveGeometry());

    UserSettings::instance().setMainWindowState(saveState());

    QMainWindow::closeEvent(event);
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

        m_updateButton->setEnabled(false);

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

    m_updateButton->setEnabled(true);

    statusBar()->showMessage(QString("Current Workspace: %1").arg(workspace->name()));
}

void MainWindow::updateWorkspace()
{
    QListWidgetItem* item = m_workspaceList->currentItem();

    if (!item) {
        QMessageBox::information(this,
                                 "Edit Workspace",
                                 "Select a workspace before saving changes.");
        return;
    }

    const QString name = m_nameEdit->text().trimmed();
    const QString description = m_descriptionEdit->toPlainText().trimmed();

    if (name.isEmpty()) {
        QMessageBox::warning(this,
                             "Edit Workspace",
                             "Please enter a workspace name.");
        return;
    }

    const int workspaceId = item->data(Qt::UserRole).toInt();

    WorkspaceRepository repository;

    std::optional<Workspace> workspace = repository.getById(workspaceId);

    if (!workspace) {
        QMessageBox::critical(this,
                              "Edit Workspace",
                              "Unable to load the selected workspace.");
        return;
    }

    workspace->setName(name);
    workspace->setDescription(description);

    //
    // Preserve the existing active state and database identity. Only the
    // editable Workspace metadata is changed here.
    //
    if (!repository.update(*workspace)) {
        QMessageBox::critical(this,
                              "Edit Workspace",
                              "Unable to save the workspace changes.");
        return;
    }

    //
    // Update the visible list immediately. The Workspace ID and current
    // Workspace selection do not change.
    //
    item->setText(workspace->name());

    statusBar()->showMessage(
        QString("Workspace updated: %1").arg(workspace->name()),
        5000);
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

    m_updateButton = new QPushButton("Save Workspace Changes", tab);
    m_updateButton->setEnabled(false);

    formLayout->addWidget(nameLabel);

    formLayout->addWidget(m_nameEdit);

    formLayout->addWidget(descriptionLabel);

    formLayout->addWidget(m_descriptionEdit);

    auto* buttonLayout = new QHBoxLayout();

    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_updateButton);
    buttonLayout->addStretch(1);

    formLayout->addLayout(buttonLayout);

    formLayout->addStretch();

    mainLayout->addLayout(listLayout, 1);

    mainLayout->addLayout(formLayout, 2);

    connect(m_addButton, &QPushButton::clicked, this, &MainWindow::addWorkspace);

    connect(m_updateButton,
            &QPushButton::clicked,
            this,
            &MainWindow::updateWorkspace);

    connect(m_workspaceList,
            &QListWidget::itemSelectionChanged,
            this,
            &MainWindow::workspaceSelected);

    return tab;
}