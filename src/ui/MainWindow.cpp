#include "MainWindow.h"

#include "../app/WorkspaceContext.h"
#include "../models/Workspace.h"
#include "../repositories/WorkspaceRepository.h"
#include "../settings/UserSettings.h"
#include "catalog/PartsCatalogWidget.h"
#include "inventory/AddInventoryDialog.h"
#include "inventory/MyInventoryWidget.h"
#include "settings/SettingsDialog.h"
#include "storage/StorageWidget.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

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

    // Menu bar
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