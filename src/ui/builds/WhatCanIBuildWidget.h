#pragma once

#include "../../models/PartUsageDiscovery.h"
#include "../../models/Part.h"
#include <QWidget>
#include <optional>

class WorkspaceContext;
class RemoteReadApplicationServices;
class RemoteCollectionMutationApplicationService;
class PartUsageDiscoveryService;
class SetImageService;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

class WhatCanIBuildWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WhatCanIBuildWidget(WorkspaceContext& workspaceContext,
        QWidget* parent = nullptr, RemoteReadApplicationServices* remoteReads = nullptr,
        RemoteCollectionMutationApplicationService* remoteCollection = nullptr);
    void invalidateCatalog();

signals:
    void createBuildRequested(int setCatalogId, const QString& inventoryMode);
    void statusMessageRequested(const QString& message, int timeoutMs);

private:
    void addCriterion();
    void search();
    void markStale();
    void showPage();
    void showDetails(int setCatalogId);
    void updateControls();
    void loadColors();
    void renderCriteria();
    void updateResolvedPartSelection();

    WorkspaceContext& m_workspaceContext;
    RemoteReadApplicationServices* m_remoteReads = nullptr;
    RemoteCollectionMutationApplicationService* m_remoteCollection = nullptr;
    PartUsageDiscoveryService* m_service = nullptr;
    SetImageService* m_images = nullptr;
    QList<PartUsageCriterion> m_criteria;
    QList<PartUsageSetResult> m_results;
    bool m_capReached = false;
    bool m_stale = false;
    bool m_hasSearched = false;
    bool m_searchOutstanding = false;
    int m_page = 0;
    quint64 m_generation = 0;
    std::optional<Part> m_resolvedPart;

    QLineEdit* m_partEdit = nullptr;
    QComboBox* m_colorCombo = nullptr;
    QSpinBox* m_quantitySpin = nullptr;
    QPushButton* m_addButton = nullptr;
    QTableWidget* m_criteriaTable = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_matchCombo = nullptr;
    QComboBox* m_pageSizeCombo = nullptr;
    QPushButton* m_searchButton = nullptr;
    QLabel* m_status = nullptr;
    QTableWidget* m_resultsTable = nullptr;
    QPushButton* m_previous = nullptr;
    QPushButton* m_next = nullptr;
    QLabel* m_pageLabel = nullptr;
};
