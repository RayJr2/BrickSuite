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

#pragma once

#include <QMainWindow>
class QCloseEvent;

class WorkspaceContext;
class QListWidget;
class QLineEdit;
class QTextEdit;
class QPushButton;
class QTabWidget;
class QWidget;
class QAction;

class StorageWidget;
class PartsCatalogWidget;
class MyInventoryWidget;
class BuildsWidget;
class SetsCatalogWidget;
class MinifigsCatalogWidget;
class BackgroundPartColorImageCacheService;
class LogViewerDialog;
class PartReferenceDialog;
class DatabaseStatusDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(WorkspaceContext& workspaceContext, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void addWorkspace();
    void updateWorkspace();
    void workspaceSelected();

private:
    void loadWorkspaces();
    QWidget* createWorkspaceTab();
    void initializeProviderStatuses();
    void ensureBrickLinkColorMappings();

    WorkspaceContext& m_workspaceContext;

    QTabWidget* m_tabWidget = nullptr;

    QListWidget* m_workspaceList = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QTextEdit* m_descriptionEdit = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_updateButton = nullptr;

    StorageWidget* m_storageWidget = nullptr;
    PartsCatalogWidget* m_partsCatalogWidget = nullptr;
    MyInventoryWidget* m_myInventoryWidget = nullptr;
    BuildsWidget* m_buildsWidget = nullptr;
    SetsCatalogWidget* m_setsCatalogWidget = nullptr;
    MinifigsCatalogWidget* m_minifigsCatalogWidget = nullptr;

    BackgroundPartColorImageCacheService* m_backgroundPartColorImageCacheService = nullptr;
    LogViewerDialog* m_logViewerDialog = nullptr;
    PartReferenceDialog* m_partReferenceDialog = nullptr;
    DatabaseStatusDialog* m_databaseStatusDialog = nullptr;
    QAction* m_backupDatabaseAction = nullptr;
    QAction* m_restoreDatabaseAction = nullptr;
    QAction* m_applicationLogAction = nullptr;
};
