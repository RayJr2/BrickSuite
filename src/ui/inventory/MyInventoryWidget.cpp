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

#include "MyInventoryWidget.h"
#include "AddInventoryDialog.h"
#include "EditInventoryDialog.h"
#include "CorrectInventoryDialog.h"
#include "RemoveInventoryDialog.h"
#include "ImportInventoryDialog.h"
#include "InventoryHistoryDialog.h"
#include "LostInventoryDialog.h"
#include "MarkLostInventoryDialog.h"
#include "MoveInventoryDialog.h"

#include "../parts/PartDetailsDialog.h"

#include "../../app/WorkspaceContext.h"
#include "../../settings/UserSettings.h"

#include "../../models/Color.h"
#include "../../models/InventorySearchCriteria.h"
#include "../../models/InventorySearchResult.h"
#include "../../models/PartCategory.h"
#include "../../models/StorageLocation.h"

#include "../../repositories/ColorRepository.h"
#include "../../repositories/ManufacturerRepository.h"
#include "../../repositories/PartCategoryRepository.h"
#include "../../repositories/StorageLocationRepository.h"

#include "../../services/RebrickableApiClient.h"
#include "../../services/images/PartImageService.h"
#include "../../services/storage/SessionStorageSelectionService.h"
#include "../../services/application/ApplicationServices.h"

#include "../helpers/ColorComboHelper.h"
#include "../helpers/LargeViewLoadingGuard.h"

#include <QColor>
#include <QComboBox>
#include <QDebug>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QHash>
#include <QSet>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QLineEdit>
#include <QListView>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

MyInventoryWidget::MyInventoryWidget(
    WorkspaceContext& workspaceContext,
    SessionStorageSelectionService& sessionStorageSelectionService,
    InventoryApplicationService& inventoryService,
    QWidget* parent)
    : QWidget(parent)
    , m_workspaceContext(workspaceContext)
    , m_sessionStorageSelectionService(sessionStorageSelectionService)
    , m_inventoryService(inventoryService)
{
    auto* mainLayout =
        new QVBoxLayout(this);

    auto* titleLabel = new QLabel("My Loose Inventory", this);

    m_addPartButton = new QPushButton("Add Part...", this);
    m_lostInventoryButton = new QPushButton("Lost Inventory...", this);

    m_importButton = new QPushButton("Import CSV", this);

    auto* titleLayout = new QHBoxLayout();

    titleLayout->addWidget(titleLabel);

    titleLayout->addStretch();

    titleLayout->addWidget(m_addPartButton);

    titleLayout->addWidget(m_lostInventoryButton);

    titleLayout->addWidget(m_importButton);

    auto* filterLayout =
        new QHBoxLayout();

    m_searchEdit =
        new QLineEdit(this);

    m_searchEdit->setPlaceholderText(
        "Search by part number or name");

    m_categoryCombo =
        new QComboBox(this);

    m_colorCombo =
        new QComboBox(this);

    // TODO: Fix spacing of items in comboBox
    auto* colorView = new QListView(m_colorCombo);

    colorView->setSpacing(0);
    colorView->setUniformItemSizes(true);

    m_colorCombo->setView(colorView);

    m_storageCombo =
        new QComboBox(this);

    m_manufacturerCombo = new QComboBox(this);

    m_searchButton =
        new QPushButton(
            "Search",
            this);

    filterLayout->addWidget(
        new QLabel("Search:", this));

    filterLayout->addWidget(
        m_searchEdit,
        2);

    filterLayout->addWidget(
        new QLabel("Category:", this));

    filterLayout->addWidget(
        m_categoryCombo,
        1);

    filterLayout->addWidget(
        new QLabel("Color:", this));

    filterLayout->addWidget(
        m_colorCombo,
        1);

    filterLayout->addWidget(
        new QLabel("Storage:", this));

    filterLayout->addWidget(
        m_storageCombo,
        1);

    filterLayout->addWidget(
        new QLabel("Manufacturer:", this));

    filterLayout->addWidget(
        m_manufacturerCombo,
        1);

    filterLayout->addWidget(
        m_searchButton);

    m_resultLabel =
        new QLabel(this);

    m_resultsTable =
        new QTableWidget(this);

    m_resultsTable->setColumnCount(11);

    m_resultsTable->setHorizontalHeaderLabels(QStringList() << "Image"
                                                            << "Part #"
                                                            << "Name"
                                                            << "Category"
                                                            << "Color"
                                                            << "Qty"
                                                            << "Storage"
                                                            << "Manufacturer"
                                                            << "Condition"
                                                            << "Ownership"
                                                            << "Action");

    m_resultsTable->setSelectionBehavior(
        QAbstractItemView::SelectRows);

    m_resultsTable->setSelectionMode(
        QAbstractItemView::SingleSelection);

    m_resultsTable->setEditTriggers(
        QAbstractItemView::NoEditTriggers);

    m_resultsTable->verticalHeader()
        ->setVisible(false);

    m_resultsTable->horizontalHeader()
        ->setStretchLastSection(false);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);

    m_resultsTable->setIconSize(QSize(44, 44));

    m_resultsTable->verticalHeader()->setDefaultSectionSize(52);
    m_resultsTable->setColumnWidth(0, 56);

    m_resultsTable->verticalHeader()->setDefaultSectionSize(52);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(9, QHeaderView::ResizeToContents);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(10, QHeaderView::ResizeToContents);

    m_previousButton = new QPushButton("Previous", this);

    m_nextButton =
        new QPushButton(
            "Next",
            this);

    m_pageLabel =
        new QLabel(this);

    m_summaryLabel =
        new QLabel(this);

    auto* pagingLayout =
        new QHBoxLayout();

    pagingLayout->addWidget(
        m_summaryLabel);

    pagingLayout->addStretch();

    pagingLayout->addWidget(
        m_previousButton);

    pagingLayout->addWidget(
        m_pageLabel);

    pagingLayout->addWidget(
        m_nextButton);

    mainLayout->addLayout(titleLayout);

    mainLayout->addLayout(
        filterLayout);

    mainLayout->addWidget(
        m_resultLabel);

    mainLayout->addWidget(
        m_resultsTable);

    mainLayout->addLayout(
        pagingLayout);

    // Initialize the PartImageService & RebrickableApiClient
    m_partImageService = new PartImageService(this);
    m_rebrickableApiClient = new RebrickableApiClient(this);

    connect(m_addPartButton, &QPushButton::clicked, this, &MyInventoryWidget::addPart);

    connect(m_lostInventoryButton,
            &QPushButton::clicked,
            this,
            &MyInventoryWidget::showLostInventory);

    connect(m_importButton, &QPushButton::clicked, this, &MyInventoryWidget::importCsv);

    connect(m_searchButton, &QPushButton::clicked, this, [this]() {
        m_currentPage = 0;
        searchInventory();
    });

    connect(
        m_searchEdit,
        &QLineEdit::returnPressed,
        this,
        [this]()
        {
            m_currentPage = 0;
            searchInventory();
        });

    connect(
        m_categoryCombo,
        &QComboBox::currentIndexChanged,
        this,
        [this]()
        {
            m_currentPage = 0;
            searchInventory();
        });

    connect(
        m_colorCombo,
        &QComboBox::currentIndexChanged,
        this,
        [this]()
        {
            m_currentPage = 0;
            searchInventory();
        });

    connect(
        m_storageCombo,
        &QComboBox::currentIndexChanged,
        this,
        [this]()
        {
            m_currentPage = 0;
            searchInventory();
        });

    connect(
        m_manufacturerCombo,
        &QComboBox::currentIndexChanged,
        this,
        [this]()
        {
            m_currentPage = 0;
            searchInventory();
        });

    connect(
        m_previousButton,
        &QPushButton::clicked,
        this,
        &MyInventoryWidget::previousPage);

    connect(
        m_nextButton,
        &QPushButton::clicked,
        this,
        &MyInventoryWidget::nextPage);

    connect(
        &m_workspaceContext,
        &WorkspaceContext::currentWorkspaceChanged,
        this,
        &MyInventoryWidget::workspaceChanged);

    connect(m_partImageService,
            &PartImageService::imageReady,
            this,
            [this](const QString& partNumber, const QString& imagePath) {
                if (!m_rowsByPartNumber.contains(partNumber))
                    return;

                QPixmap pixmap(imagePath);

                if (pixmap.isNull())
                    return;

                const QPixmap thumbnail = pixmap.scaled(44,
                                                        44,
                                                        Qt::KeepAspectRatio,
                                                        Qt::SmoothTransformation);

                const QList<int> rows = m_rowsByPartNumber.value(partNumber);

                for (const int row : rows) {
                    //
                    // A real Part+Color image always wins.
                    // Never overwrite it with the generic
                    // Parts Catalog image.
                    //
                    if (m_rowsWithColorImage.contains(row))
                        continue;

                    if (row < 0 || row >= m_resultsTable->rowCount()) {
                        continue;
                    }

                    QTableWidgetItem* item = m_resultsTable->item(row, 0);

                    if (!item) {
                        item = new QTableWidgetItem();

                        m_resultsTable->setItem(row, 0, item);
                    }

                    item->setIcon(QIcon(thumbnail));
                }
            });

    connect(m_partImageService,
            &PartImageService::partColorImageReady,
            this,
            &MyInventoryWidget::updatePartColorImage);

    connect(m_rebrickableApiClient,
            &RebrickableApiClient::partDetailsFinished,
            this,
            [this](const RebrickableApiClient::PartDetailsResult& result) {
                if (!result.success)
                    return;

                if (result.part.partNumber.isEmpty())
                    return;

                if (result.part.partImageUrl.isEmpty())
                    return;

                m_partImageService->requestPartImage(result.part.partNumber,
                                                     result.part.partImageUrl);
            });

    loadCategories();
    loadColors();
    if (m_inventoryService.status().isAvailable())
        loadManufacturers();

    workspaceChanged(
        m_workspaceContext.currentWorkspaceId());

    updatePagingControls();
}

void MyInventoryWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    //
    // Storage locations may have changed while
    // My Inventory was not visible. Refresh only
    // the Storage filter here. The inventory table
    // itself is refreshed by the operations that
    // actually modify inventory.
    //
    if (m_workspaceContext.hasCurrentWorkspace()) {
        loadStorageLocations();
    }
}

void MyInventoryWidget::workspaceChanged(int workspaceId)
{
    Q_UNUSED(workspaceId);

    m_currentPage = 0;

    loadStorageLocations();

    searchInventory();

    m_addPartButton->setEnabled(m_workspaceContext.hasCurrentWorkspace());
    m_lostInventoryButton->setEnabled(m_workspaceContext.hasCurrentWorkspace());
    m_importButton->setEnabled(m_workspaceContext.hasCurrentWorkspace());
}

void MyInventoryWidget::loadCategories()
{
    m_categoryCombo->clear();

    m_categoryCombo->addItem("All Categories", 0);

    PartCategoryRepository repository;

    const QList<PartCategory> categories = repository.getAll();

    for (const PartCategory& category : categories) {
        m_categoryCombo->addItem(category.name(), category.id());
    }
}

void MyInventoryWidget::loadColors()
{
    m_colorCombo->clear();

    m_colorCombo->addItem("All Colors", 0);

    ColorRepository repository;

    const QList<Color> colors = repository.getAll();

    for (const Color& color : colors) {
        m_colorCombo->addItem(color.name(), color.id());
    }
}

void MyInventoryWidget::loadManufacturers()
{
    const QSignalBlocker blocker(m_manufacturerCombo);
    const int selectedManufacturerId = m_manufacturerCombo->currentData().toInt();

    m_manufacturerCombo->clear();
    m_manufacturerCombo->addItem("All Manufacturers", 0);

    ManufacturerRepository repository;
    const QList<Manufacturer> manufacturers = repository.getAll(true);

    for (const Manufacturer& manufacturer : manufacturers)
        m_manufacturerCombo->addItem(manufacturer.name(), manufacturer.id());

    const int restoredIndex = m_manufacturerCombo->findData(selectedManufacturerId);
    if (restoredIndex >= 0)
        m_manufacturerCombo->setCurrentIndex(restoredIndex);
}

void MyInventoryWidget::loadStorageLocations()
{
    const QSignalBlocker blocker(m_storageCombo);

    const int selectedLocationId = m_storageCombo->currentData().toInt();

    m_storageCombo->clear();

    m_storageCombo->addItem("All Locations", 0);

    m_storagePathById.clear();

    if (!m_workspaceContext.hasCurrentWorkspace())
        return;

    StorageLocationRepository repository;

    const QList<StorageLocation> locations = repository.getByWorkspace(
        m_workspaceContext.currentWorkspaceId());

    QHash<int, StorageLocation> locationById;
    QSet<int> activeParentIds;

    for (const StorageLocation& location : locations) {
        locationById.insert(location.id(), location);

        if (location.parentLocationId() > 0) {
            activeParentIds.insert(location.parentLocationId());
        }
    }

    for (const StorageLocation& location : locations) {
        // Parent/container locations remain part of the hierarchy so
        // full paths can be built, but only active leaf locations
        // are offered for operational inventory selection.
        if (activeParentIds.contains(location.id())) {
            continue;
        }

        QStringList pathParts;

        pathParts.prepend(location.name());

        int parentId = location.parentLocationId();

        while (parentId > 0) {
            if (!locationById.contains(parentId))
                break;

            const StorageLocation parent = locationById.value(parentId);

            pathParts.prepend(parent.name());

            parentId = parent.parentLocationId();
        }

        const QString path = pathParts.join(" / ");

        m_storagePathById.insert(location.id(), path);

        m_storageCombo->addItem(path, location.id());

        const int restoredIndex = m_storageCombo->findData(selectedLocationId);

        if (restoredIndex >= 0) {
            m_storageCombo->setCurrentIndex(restoredIndex);
        }
    }
}

void MyInventoryWidget::searchInventory(const QString& loadingMessage)
{
    LargeViewLoadingGuard loading(
        this, m_refreshInProgress,
        loadingMessage.isEmpty() ? QStringLiteral("Loading My Inventory...") : loadingMessage,
        {m_searchButton, m_searchEdit, m_categoryCombo, m_colorCombo, m_storageCombo,
         m_manufacturerCombo},
        {m_previousButton, m_nextButton});
    if (!loading.active())
        return;

    QElapsedTimer totalTimer;
    totalTimer.start();
    QElapsedTimer phaseTimer;
    phaseTimer.start();
    m_resultsTable->setRowCount(0);

    m_rowsByPartNumber.clear();
    m_rowsByPartColor.clear();
    m_rowsWithColorImage.clear();
    m_partDetailsRequested.clear();
    const qint64 clearMs = phaseTimer.elapsed();

    const ApplicationServiceStatus serviceStatus = m_inventoryService.status();
    if (!serviceStatus.isAvailable()) {
        m_lastResultCount = 0;
        m_totalResultCount = 0;
        m_resultLabel->setText(serviceStatus.message);
        m_summaryLabel->setText(serviceStatus.message);
        m_addPartButton->setEnabled(false);
        m_lostInventoryButton->setEnabled(false);
        m_importButton->setEnabled(false);
        updatePagingControls();
        return;
    }

    if (!m_workspaceContext.hasCurrentWorkspace()) {
        m_lastResultCount = 0;
        m_totalResultCount = 0;

        m_resultLabel->setText("Select a workspace to view inventory.");

        updatePagingControls();

        return;
    }

    InventorySearchCriteria criteria;

    criteria.workspaceId = m_workspaceContext.currentWorkspaceId();

    criteria.searchText = m_searchEdit->text().trimmed();

    criteria.categoryId = m_categoryCombo->currentData().toInt();

    criteria.colorId = m_colorCombo->currentData().toInt();

    criteria.storageLocationId = m_storageCombo->currentData().toInt();

    criteria.manufacturerId = m_manufacturerCombo->currentData().toInt();

    const int resultsPerPage = UserSettings::instance().resultsPerPage();

    criteria.limit = resultsPerPage;

    criteria.offset = m_currentPage * resultsPerPage;

    phaseTimer.restart();
    m_totalResultCount = m_inventoryService.count(criteria);
    const qint64 countMs = phaseTimer.elapsed();

    //
    // If a data change leaves the current page beyond the new last page,
    // clamp once before loading the results. This can happen after moving,
    // losing, or editing the last record on a page.
    //
    const int totalPages = qMax(1,
                                (m_totalResultCount + resultsPerPage - 1)
                                    / resultsPerPage);

    if (m_currentPage >= totalPages) {
        m_currentPage = totalPages - 1;
        criteria.offset = m_currentPage * resultsPerPage;
    }

    phaseTimer.restart();
    const QList<InventorySearchResult> results = m_inventoryService.searchRows(criteria);
    const qint64 searchMs = phaseTimer.elapsed();

    m_lastResultCount = results.size();

    const QString apiKey = UserSettings::instance().rebrickableApiKey();

    int row = 0;

    qint64 mapSetupNs = 0;
    qint64 itemCreationNs = 0;
    qint64 colorSetupNs = 0;
    qint64 storageSetupNs = 0;
    qint64 metadataSetupNs = 0;
    qint64 actionCreationNs = 0;
    qint64 tableAttachNs = 0;
    qint64 imageSetupNs = 0;
    const bool tableUpdatesEnabled = m_resultsTable->updatesEnabled();
    m_resultsTable->setUpdatesEnabled(false);
    m_resultsTable->setRowCount(results.size());
    phaseTimer.restart();

    for (const InventorySearchResult& result : results) {
        QElapsedTimer rowPhaseTimer;
        rowPhaseTimer.start();
        const QString partNumber = result.partNumber;

        const int rebrickableColorId = result.rebrickableColorId;

        if (rebrickableColorId >= 0) {
            const QString colorKey = partColorKey(partNumber, rebrickableColorId);

            m_rowsByPartColor[colorKey].append(row);
        }

        //
        // The same part number may appear on several
        // inventory rows because of different colors,
        // locations, conditions, etc.
        //
        m_rowsByPartNumber[partNumber].append(row);
        mapSetupNs += rowPhaseTimer.nsecsElapsed();

        rowPhaseTimer.restart();
        auto* imageItem = new QTableWidgetItem();

        imageItem->setTextAlignment(Qt::AlignCenter);

        auto* partNumberItem = new QTableWidgetItem(partNumber);

        partNumberItem->setData(Qt::UserRole, result.inventoryRecordId);

        auto* nameItem = new QTableWidgetItem(result.partName);

        auto* categoryItem = new QTableWidgetItem(result.categoryName);

        auto* colorItem = new QTableWidgetItem(result.colorName);
        auto* quantityItem = new QTableWidgetItem(QString::number(result.quantity));
        itemCreationNs += rowPhaseTimer.nsecsElapsed();

        rowPhaseTimer.restart();
        QString normalizedRgb = result.colorRgb.trimmed();

        if (!normalizedRgb.isEmpty() && !normalizedRgb.startsWith('#')) {
            normalizedRgb.prepend('#');
        }

        const QColor sourceColor(normalizedRgb);

        if (sourceColor.isValid()) {
            const QColor backgroundColor = m_resultsTable->palette().color(QPalette::Base);

            const QColor displayColor = ColorComboHelper::readableColor(sourceColor,
                                                                        backgroundColor);

            colorItem->setForeground(displayColor);
        }
        colorSetupNs += rowPhaseTimer.nsecsElapsed();

        rowPhaseTimer.restart();
        QString storagePath = storagePathForId(result.storageLocationId);

        if (storagePath.isEmpty()) {
            storagePath = result.storageLocationName;
        }
        auto* storageItem = new QTableWidgetItem(storagePath);
        storageSetupNs += rowPhaseTimer.nsecsElapsed();

        rowPhaseTimer.restart();
        auto* manufacturerItem = new QTableWidgetItem(result.manufacturerName);

        auto* conditionItem = new QTableWidgetItem(result.condition);

        auto* ownershipItem = new QTableWidgetItem(result.ownershipType);
        metadataSetupNs += rowPhaseTimer.nsecsElapsed();

        rowPhaseTimer.restart();
        auto* actionCombo = new QComboBox(m_resultsTable);

        actionCombo->addItem("Actions...");
        actionCombo->addItem("Details", "details");
        actionCombo->addItem("Edit", "edit");
        actionCombo->addItem("Move", "move");
        actionCombo->addItem("Correct Entry...", "correct");
        actionCombo->addItem("Remove Entry...", "remove");
        actionCombo->addItem("Mark Lost...", "lost");
        actionCombo->addItem("View History", "history");

        // Future actions can be added here:
        //
        // actionCombo->addItem(
        //     "Allocate to Build",
        //     "allocate");

        const int inventoryRecordId = result.inventoryRecordId;

        const int partId = result.partId;

        const int colorId = result.colorId;

        connect(actionCombo,
                &QComboBox::currentIndexChanged,
                this,
                [this, actionCombo, inventoryRecordId, partId, colorId](int index) {
                    if (index <= 0)
                        return;

                    const QString action = actionCombo->itemData(index).toString();

                    if (action == "details") {
                        PartDetailsDialog dialog(partId, this);

                        dialog.exec();
                    } else if (action == "edit") {
                        EditInventoryDialog dialog(inventoryRecordId, m_workspaceContext, this);

                        if (dialog.exec() == QDialog::Accepted) {
                            refresh();
                        }
                    } else if (action == "move") {
                        MoveInventoryDialog dialog(inventoryRecordId, m_workspaceContext,
                                                   m_sessionStorageSelectionService, this);

                        if (dialog.exec() == QDialog::Accepted) {
                            refresh();
                        }
                    } else if (action == "correct") {
                        CorrectInventoryDialog dialog(inventoryRecordId, m_workspaceContext, this);

                        if (dialog.exec() == QDialog::Accepted) {
                            searchInventory();
                            emit inventoryChanged();
                            return;
                        }
                    } else if (action == "remove") {
                        RemoveInventoryDialog dialog(inventoryRecordId, this);

                        if (dialog.exec() == QDialog::Accepted) {
                            searchInventory();
                            emit inventoryChanged();
                            return;
                        }
                    } else if (action == "lost") {
                        MarkLostInventoryDialog dialog(inventoryRecordId, this);

                        if (dialog.exec() == QDialog::Accepted) {
                            //
                            // Keep the current filters/page and simply
                            // reload the current inventory view.
                            //
                            searchInventory();

                            return;
                        }
                    } else if (action == "history") {
                        InventoryHistoryDialog dialog(partId, colorId, m_workspaceContext,
                                                      m_inventoryService, this);

                        dialog.exec();
                    }

                    // Return the action control
                    // to its neutral state.
                    actionCombo->setCurrentIndex(0);
                });
        actionCreationNs += rowPhaseTimer.nsecsElapsed();

        rowPhaseTimer.restart();
        m_resultsTable->setItem(row, 0, imageItem);

        m_resultsTable->setItem(row, 1, partNumberItem);

        m_resultsTable->setItem(row, 2, nameItem);

        m_resultsTable->setItem(row, 3, categoryItem);

        m_resultsTable->setItem(row, 4, colorItem);

        m_resultsTable->setItem(row, 5, quantityItem);

        m_resultsTable->setItem(row, 6, storageItem);

        m_resultsTable->setItem(row, 7, manufacturerItem);

        m_resultsTable->setItem(row, 8, conditionItem);

        m_resultsTable->setItem(row, 9, ownershipItem);

        m_resultsTable->setCellWidget(row, 10, actionCombo);
        tableAttachNs += rowPhaseTimer.nsecsElapsed();

        //
        // My Loose Inventory prefers an actual Part+Color
        // image when one is already cached.
        //
        // Until the background worker fills that cache,
        // fall back to the existing generic Part image.
        //
        rowPhaseTimer.restart();
        bool colorImageLoaded = false;

        if (rebrickableColorId >= 0) {
            const QString colorCachedPath
                = m_partImageService->cachedPartColorImagePath(partNumber, rebrickableColorId);

            if (!colorCachedPath.isEmpty()) {
                //
                // Protect this row from a later generic-image
                // callback for the same Part Number.
                //
                m_rowsWithColorImage.insert(row);

                m_partImageService->requestPartColorImage(partNumber, rebrickableColorId, QString());

                colorImageLoaded = true;
            }
        }

        if (!colorImageLoaded) {
            const QString genericCachedPath = m_partImageService->cachedImagePath(partNumber);

            if (!genericCachedPath.isEmpty()) {
                m_partImageService->requestPartImage(partNumber, QString());
            }
        }
        imageSetupNs += rowPhaseTimer.nsecsElapsed();

        ++row;
    }

    const qint64 rowAggregateMs = phaseTimer.elapsed();
    QElapsedTimer finalizeTimer;
    finalizeTimer.start();
    m_resultsTable->setUpdatesEnabled(tableUpdatesEnabled);
    const qint64 tableFinalizeMs = finalizeTimer.elapsed();
    phaseTimer.restart();
    if (results.isEmpty()) {
        if (m_currentPage > 0) {
            m_resultLabel->setText("No more matching inventory records.");
        } else {
            const bool hasSearchText = !criteria.searchText.isEmpty();
            const bool hasCategoryFilter = criteria.categoryId > 0;
            const bool hasColorFilter = criteria.colorId > 0;
            const bool hasStorageFilter = criteria.storageLocationId > 0;
            const bool hasManufacturerFilter = criteria.manufacturerId > 0;

            if (hasSearchText || hasCategoryFilter || hasColorFilter || hasStorageFilter || hasManufacturerFilter) {
                m_resultLabel->setText(
                    "No inventory records match the current search or filters.");
            } else {
                m_resultLabel->setText(
                    "No loose inventory has been added to this workspace yet.");
            }
        }
    } else {
        const int firstResult = criteria.offset + 1;

        const int lastResult = criteria.offset + results.size();

        m_resultLabel->setText(
            QString("Showing results %1 - %2 of %3.")
                .arg(firstResult)
                .arg(lastResult)
                .arg(m_totalResultCount));
    }

    updatePagingControls();
    const qint64 statusMs = phaseTimer.elapsed();
    const auto milliseconds = [](qint64 nanoseconds) {
        return QString::number(static_cast<double>(nanoseconds) / 1000000.0, 'f', 1);
    };
    const qint64 measuredRowNs = mapSetupNs + itemCreationNs + colorSetupNs
        + storageSetupNs + metadataSetupNs + actionCreationNs + tableAttachNs + imageSetupNs;
    qInfo().noquote()
        << QStringLiteral("Performance MyInventory page=%1 rows=%2 total=%3ms count=%4ms "
                          "search=%5ms clear=%6ms item-create=%7ms actions=%8ms "
                          "table-attach=%9ms metadata=%10ms storage=%11ms color=%12ms "
                          "row-maps=%13ms image-setup=%14ms row-other=%15ms "
                          "table-finalize=%16ms status=%17ms color-lookups=0")
               .arg(m_currentPage + 1).arg(results.size()).arg(totalTimer.elapsed())
               .arg(countMs).arg(searchMs).arg(clearMs)
               .arg(milliseconds(itemCreationNs)).arg(milliseconds(actionCreationNs))
               .arg(milliseconds(tableAttachNs)).arg(milliseconds(metadataSetupNs))
               .arg(milliseconds(storageSetupNs)).arg(milliseconds(colorSetupNs))
               .arg(milliseconds(mapSetupNs)).arg(milliseconds(imageSetupNs))
               .arg(QString::number(qMax(0.0, static_cast<double>(rowAggregateMs)
                   - static_cast<double>(measuredRowNs) / 1000000.0), 'f', 1))
               .arg(tableFinalizeMs).arg(statusMs);
}

void MyInventoryWidget::previousPage()
{
    if (m_currentPage <= 0)
        return;

    --m_currentPage;

    searchInventory(QStringLiteral("Loading page %1...").arg(m_currentPage + 1));
}

void MyInventoryWidget::nextPage()
{
    const int resultsPerPage = UserSettings::instance().resultsPerPage();
    const int totalPages = qMax(1,
                                (m_totalResultCount + resultsPerPage - 1)
                                    / resultsPerPage);

    if (m_currentPage + 1 >= totalPages)
        return;

    ++m_currentPage;

    searchInventory(QStringLiteral("Loading page %1...").arg(m_currentPage + 1));
}

void MyInventoryWidget::updatePagingControls()
{
    const int resultsPerPage = UserSettings::instance().resultsPerPage();
    const int totalPages = qMax(1,
                                (m_totalResultCount + resultsPerPage - 1)
                                    / resultsPerPage);
    const int displayPage = qMin(m_currentPage + 1, totalPages);

    m_previousButton->setEnabled(m_currentPage > 0);
    m_nextButton->setEnabled(m_currentPage + 1 < totalPages);

    m_summaryLabel->setText(
        QString("%1 %2")
            .arg(QLocale().toString(m_totalResultCount))
            .arg(m_totalResultCount == 1
                     ? "Inventory Record"
                     : "Inventory Records"));

    m_pageLabel->setText(
        QString("Page %1 of %2")
            .arg(displayPage)
            .arg(totalPages));
}

void MyInventoryWidget::refresh()
{
    m_currentPage = 0;

    {
        LargeViewLoadingGuard loading(
            this, m_refreshInProgress, QStringLiteral("Loading My Inventory..."),
            {m_searchButton, m_searchEdit, m_categoryCombo, m_colorCombo, m_storageCombo,
             m_manufacturerCombo},
            {m_previousButton, m_nextButton});
        if (!loading.active())
            return;
        loadStorageLocations();
    }

    searchInventory();
}

void MyInventoryWidget::settingsChanged()
{
    m_currentPage = 0;

    searchInventory();
}

void MyInventoryWidget::reloadManufacturers()
{
    loadManufacturers();
    m_currentPage = 0;
    searchInventory();
}

void MyInventoryWidget::importCsv()
{
    if (!m_workspaceContext.hasCurrentWorkspace()) {
        return;
    }

    ImportInventoryDialog dialog(m_workspaceContext, m_sessionStorageSelectionService, this);

    if (dialog.exec() == QDialog::Accepted) {
        refresh();
        emit inventoryChanged();
    }
}

QString MyInventoryWidget::storagePathForId(int storageLocationId) const
{
    return m_storagePathById.value(storageLocationId);
}

QString MyInventoryWidget::partColorKey(const QString& partNumber, int rebrickableColorId) const
{
    return QString("%1|%2").arg(partNumber.trimmed()).arg(rebrickableColorId);
}

void MyInventoryWidget::updatePartColorImage(const QString& partNumber,
                                             int rebrickableColorId,
                                             const QString& imagePath)
{
    const QString key = partColorKey(partNumber, rebrickableColorId);

    //
    // The newly cached Part/Color is not on the
    // currently displayed inventory page.
    //
    if (!m_rowsByPartColor.contains(key))
        return;

    QPixmap pixmap(imagePath);

    if (pixmap.isNull())
        return;

    const QPixmap thumbnail = pixmap.scaled(44, 44, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    const QList<int> rows = m_rowsByPartColor.value(key);

    for (const int row : rows) {
        if (row < 0 || row >= m_resultsTable->rowCount()) {
            continue;
        }

        //
        // From this point forward this row owns an
        // actual Part+Color image. A generic image must
        // never replace it.
        //
        m_rowsWithColorImage.insert(row);

        QTableWidgetItem* item = m_resultsTable->item(row, 0);

        if (!item) {
            item = new QTableWidgetItem();

            m_resultsTable->setItem(row, 0, item);
        }

        item->setIcon(QIcon(thumbnail));
    }
}

void MyInventoryWidget::addPart()
{
    if (!m_workspaceContext.hasCurrentWorkspace())
        return;

    // M23.3: keep one rapid-entry dialog alive non-modally so Part Reference
    // can remain interactive on another monitor and send selected parts
    // directly into it.
    if (m_activeAddInventoryDialog) {
        m_activeAddInventoryDialog->show();
        m_activeAddInventoryDialog->raise();
        m_activeAddInventoryDialog->activateWindow();
        return;
    }

    auto* dialog = new AddInventoryDialog(m_workspaceContext,
                                          m_sessionStorageSelectionService, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::NonModal);
    dialog->setModal(false);

    const int filteredStorageLocationId = m_storageCombo->currentData().toInt();
    if (filteredStorageLocationId > 0)
        dialog->setPreferredStorageLocationId(filteredStorageLocationId);

    // Rapid-entry mode may add many parts while this non-modal dialog stays
    // open. Rebuilding My Inventory (and the background part-color image
    // cache queue via inventoryChanged) after every single Add is expensive
    // and blocks the GUI thread. Mark the view dirty here and perform one
    // refresh when the dialog closes.
    dialog->setProperty("brickSuiteInventoryAddedWhileOpen", false);

    connect(
        dialog,
        &AddInventoryDialog::inventoryAdded,
        this,
        [dialog]()
        {
            dialog->setProperty("brickSuiteInventoryAddedWhileOpen", true);
        });

    m_activeAddInventoryDialog = dialog;
    emit addInventoryDialogAvailabilityChanged(true);

    connect(dialog, &QDialog::finished, this, [this, dialog](int) {
        const bool inventoryAddedWhileOpen =
            dialog->property("brickSuiteInventoryAddedWhileOpen").toBool();

        if (m_activeAddInventoryDialog == dialog) {
            m_activeAddInventoryDialog = nullptr;
            emit addInventoryDialogAvailabilityChanged(false);
        }

        // Refresh the inventory table and rebuild dependent background queues
        // once per rapid-entry session instead of once per added part.
        if (inventoryAddedWhileOpen) {
            searchInventory();
            emit inventoryChanged();
        }

        dialog->deleteLater();
    });

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

bool MyInventoryWidget::hasActiveAddInventoryDialog() const
{
    return m_activeAddInventoryDialog != nullptr;
}

void MyInventoryWidget::sendPartToActiveAddInventoryDialog(const QString& partNumber)
{
    if (!m_activeAddInventoryDialog)
        return;

    m_activeAddInventoryDialog->setPartFromReference(partNumber);
}

void MyInventoryWidget::showLostInventory()
{
    if (!m_workspaceContext.hasCurrentWorkspace()) {
        return;
    }

    LostInventoryDialog dialog(m_workspaceContext, m_sessionStorageSelectionService, this);

    dialog.exec();

    //
    // Found operations may have returned pieces
    // to loose inventory.
    //
    searchInventory();
}
