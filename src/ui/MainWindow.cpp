#include "MainWindow.h"

#include "../app/WorkspaceContext.h"
#include "../models/Workspace.h"
#include "../repositories/WorkspaceRepository.h"
#include "storage/StorageWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(WorkspaceContext& workspaceContext, QWidget* parent)
    : QMainWindow(parent)
    , m_workspaceContext(workspaceContext)
{
    setWindowTitle("BrickSuite");
    resize(1200, 800);

    auto* centralWidget = new QWidget(this);
    auto* mainLayout = new QHBoxLayout(centralWidget);

    // Left side: workspace list
    auto* listLayout = new QVBoxLayout();

    auto* workspaceLabel = new QLabel("Workspaces", centralWidget);

    m_workspaceList = new QListWidget(centralWidget);

    listLayout->addWidget(workspaceLabel);
    listLayout->addWidget(m_workspaceList);

    m_storageWidget = new StorageWidget(m_workspaceContext, centralWidget);

    // Right side: create workspace
    auto* formLayout = new QVBoxLayout();

    auto* nameLabel = new QLabel("Workspace Name", centralWidget);

    m_nameEdit = new QLineEdit(centralWidget);

    auto* descriptionLabel = new QLabel("Description", centralWidget);

    m_descriptionEdit = new QTextEdit(centralWidget);

    m_addButton = new QPushButton("Add Workspace", centralWidget);

    formLayout->addWidget(nameLabel);
    formLayout->addWidget(m_nameEdit);

    formLayout->addWidget(descriptionLabel);
    formLayout->addWidget(m_descriptionEdit);

    formLayout->addWidget(m_addButton);
    formLayout->addStretch();

    mainLayout->addLayout(listLayout, 1);
    mainLayout->addLayout(formLayout, 1);
    mainLayout->addWidget(m_storageWidget, 2);

    setCentralWidget(centralWidget);

    connect(m_addButton, &QPushButton::clicked, this, &MainWindow::addWorkspace);

    connect(m_workspaceList,
            &QListWidget::itemSelectionChanged,
            this,
            &MainWindow::workspaceSelected);

    loadWorkspaces();

    statusBar()->showMessage("BrickSuite Version 1.0");
}

void MainWindow::loadWorkspaces()
{
    m_workspaceList->clear();

    WorkspaceRepository repository;

    const QList<Workspace> workspaces = repository.getAll();

    for (const Workspace& workspace : workspaces) {
        auto* item = new QListWidgetItem(workspace.name());

        item->setData(Qt::UserRole, workspace.id());

        m_workspaceList->addItem(item);
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