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

#include "../../services/application/dto/RemoteReadDtos.h"
#include "../common/SingleInstanceWindowRegistry.h"
#include <QWidget>

class WorkspaceContext;
class SessionStorageSelectionService;
class BuildApplicationService;
class RemoteReadApplicationServices;
class PartExternalIdEnrichmentService;

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTextEdit;
class QCheckBox;
class QSpinBox;
class QGroupBox;
class QWidget;
class QSplitter;
class QShowEvent;
class QDialog;

class BuildsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BuildsWidget(
        WorkspaceContext& workspaceContext,
        SessionStorageSelectionService& sessionStorageSelectionService,
        BuildApplicationService& buildService,
        QWidget* parent = nullptr,
        RemoteReadApplicationServices* remoteReads = nullptr,
        PartExternalIdEnrichmentService* enrichmentService = nullptr);

    void refresh();
    void selectBuild(int buildId);
    void reloadManufacturers();
    void setRemoteSessionConnected(bool connected);

signals:
    void collectionItemRequested(int collectionItemId);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void workspaceChanged(
        int workspaceId);

    void addBuild();
    void buildSelectionChanged();
    void addRequirement();
    void allocateAvailable();

private:
    void loadBuilds();
    void loadRemoteBuilds();
    void renderRemoteBuilds(const QList<RemoteReadDto::BuildSummary>& builds);
    void loadRemoteRequirements(int page = 1);
    void renderRemoteRequirements();
    void showRemoteDetails(int buildId);
    void showRemotePulling(int buildId);
    void loadManufacturers();
    void updateUiState();

    void loadRequirements();
    void loadColors();
    void updateRequirementUiState();
    void exportPullList();
    void importPullList();
    void interactivePulling();
    void importMocPartsCsv();
    void exportMissingParts();
    void procureMissingParts();
    void storeSpare(int requirementId);

    WorkspaceContext& m_workspaceContext;
    SessionStorageSelectionService& m_sessionStorageSelectionService;
    BuildApplicationService& m_buildService;
    RemoteReadApplicationServices* m_remoteReads = nullptr;
    PartExternalIdEnrichmentService* m_enrichmentService = nullptr;
    QList<RemoteReadDto::BuildRequirement> m_remoteRequirements;
    quint64 m_buildListGeneration = 0;
    quint64 m_requirementGeneration = 0;
    SingleInstanceWindowRegistry<int, QDialog> m_remotePullingDialogs;
    bool m_remoteMode = false;

    QComboBox* m_typeCombo = nullptr;
    QLineEdit* m_setNumberEdit = nullptr;
    QComboBox* m_inventoryModeCombo = nullptr;
    QComboBox* m_manufacturerCombo = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_statusCombo = nullptr;
    QTextEdit* m_notesEdit = nullptr;

    QPushButton* m_addButton = nullptr;

    QTableWidget* m_buildsTable = nullptr;
    QCheckBox* m_showArchivedBuildsCheck = nullptr;
    QSplitter* m_buildsRequirementsSplitter = nullptr;
    QLabel* m_statusLabel = nullptr;

    int m_selectedBuildId = 0;

    QLabel* m_requirementsLabel = nullptr;

    QLineEdit* m_partNumberEdit = nullptr;
    QComboBox* m_colorCombo = nullptr;
    QSpinBox* m_quantitySpin = nullptr;
    QCheckBox* m_spareCheck = nullptr;

    QPushButton* m_addRequirementButton = nullptr;

    QPushButton* m_loadSetFromRebrickableButton = nullptr;
    QPushButton* m_importMocPartsButton = nullptr;
    QPushButton* m_allocateAvailableButton = nullptr;
    QPushButton* m_exportMissingPartsButton = nullptr;
    QPushButton* m_procureMissingPartsButton = nullptr;
    QPushButton* m_exportPullListButton = nullptr;
    QPushButton* m_importPullListButton = nullptr;
    QPushButton* m_interactivePullButton = nullptr;

    QTableWidget* m_requirementsTable = nullptr;

    QGroupBox* m_newBuildGroup = nullptr;
    QWidget* m_newBuildContent = nullptr;
};
