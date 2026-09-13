#pragma once

#include "../../models/PartUsageDiscovery.h"
#include "../../models/InventoryBuildability.h"
#include "../../models/Part.h"
#include "../../services/application/dto/RemoteBuildabilityDtos.h"
#include <QWidget>
#include <optional>

class WorkspaceContext;
class RemoteReadApplicationServices;
class RemoteCollectionMutationApplicationService;
class PartUsageDiscoveryService;
class InventoryBuildabilityService;
class SetImageService;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QCheckBox;
class QStackedWidget;

class WhatCanIBuildWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WhatCanIBuildWidget(WorkspaceContext& workspaceContext,
        QWidget* parent = nullptr, RemoteReadApplicationServices* remoteReads = nullptr,
        RemoteCollectionMutationApplicationService* remoteCollection = nullptr);
    void invalidateCatalog();
    void invalidateOperationalData();
    void setRemoteSessionConnected(bool connected);

signals:
    void createBuildRequested(int setCatalogId, const QString& inventoryMode);
    void statusMessageRequested(const QString& message, int timeoutMs);

private:
    void addCriterion();
    void search();
    void searchInventory();
    void markStale();
    void showPage();
    void showDetails(int setCatalogId);
    void showMissingParts(const InventoryBuildabilitySetResult& result);
    void showSources(const InventoryBuildabilitySetResult& result);
    void requestRemoteDetails(const InventoryBuildabilitySetResult& result, bool sourcesOnly);
    void updateRemoteCapability();
    void modeChanged();
    void updateControls();
    void loadColors();
    void loadThemes();
    void renderCriteria();
    void updateResolvedPartSelection();

    WorkspaceContext& m_workspaceContext;
    RemoteReadApplicationServices* m_remoteReads = nullptr;
    RemoteCollectionMutationApplicationService* m_remoteCollection = nullptr;
    PartUsageDiscoveryService* m_service = nullptr;
    InventoryBuildabilityService* m_inventoryService = nullptr;
    SetImageService* m_images = nullptr;
    QList<PartUsageCriterion> m_criteria;
    QList<PartUsageSetResult> m_results;
    QList<InventoryBuildabilitySetResult> m_inventoryResults;
    bool m_capReached = false;
    bool m_stale = false;
    bool m_hasSearched = false;
    bool m_searchOutstanding = false;
    int m_page = 0;
    quint64 m_generation = 0;
    quint64 m_remoteSearchToken = 0;
    quint64 m_remoteDetailsGeneration = 0;
    bool m_remoteSessionConnected = false;
    std::optional<Part> m_resolvedPart;
    bool m_inventoryMode = true;

    QComboBox* m_modeCombo = nullptr;
    QStackedWidget* m_modeStack = nullptr;
    QLineEdit* m_partEdit = nullptr;
    QComboBox* m_colorCombo = nullptr;
    QSpinBox* m_quantitySpin = nullptr;
    QPushButton* m_addButton = nullptr;
    QTableWidget* m_criteriaTable = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_matchCombo = nullptr;
    QComboBox* m_pageSizeCombo = nullptr;
    QPushButton* m_searchButton = nullptr;
    QSpinBox* m_minimumSpin = nullptr;
    QSpinBox* m_minimumSetPartsSpin = nullptr;
    QSpinBox* m_yearFromSpin = nullptr;
    QSpinBox* m_yearToSpin = nullptr;
    QComboBox* m_themeCombo = nullptr;
    QCheckBox* m_fullyOnly = nullptr;
    QCheckBox* m_includeCollection = nullptr;
    QCheckBox* m_fullyFirst = nullptr;
    QLabel* m_collectionStatus = nullptr;
    QLabel* m_remoteInventoryNotice = nullptr;
    QLineEdit* m_inventorySearchEdit = nullptr;
    QPushButton* m_inventorySearchButton = nullptr;
    QLabel* m_status = nullptr;
    QTableWidget* m_resultsTable = nullptr;
    QPushButton* m_previous = nullptr;
    QPushButton* m_next = nullptr;
    QLabel* m_pageLabel = nullptr;
};
