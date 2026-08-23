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
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/PartCategoryRepository.h"
#include "../../repositories/StorageLocationRepository.h"

#include "../../services/RebrickableApiClient.h"
#include "../../services/images/PartImageService.h"

#include "../helpers/ColorComboHelper.h"

#include <QColor>
#include <QComboBox>
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
    QWidget* parent)
    : QWidget(parent)
    , m_workspaceContext(workspaceContext)
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
        m_searchButton);

    m_resultLabel =
        new QLabel(this);

    m_resultsTable =
        new QTableWidget(this);

    m_resultsTable->setColumnCount(10);

    m_resultsTable->setHorizontalHeaderLabels(QStringList() << "Image"
                                                            << "Part #"
                                                            << "Name"
                                                            << "Category"
                                                            << "Color"
                                                            << "Qty"
                                                            << "Storage"
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

void MyInventoryWidget::searchInventory()
{
    m_resultsTable->setRowCount(0);

    m_rowsByPartNumber.clear();
    m_rowsByPartColor.clear();
    m_rowsWithColorImage.clear();
    m_partDetailsRequested.clear();

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

    const int resultsPerPage = UserSettings::instance().resultsPerPage();

    criteria.limit = resultsPerPage;

    criteria.offset = m_currentPage * resultsPerPage;

    InventoryRecordRepository repository;

    m_totalResultCount = repository.count(criteria);

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

    const QList<InventorySearchResult> results = repository.search(criteria);

    m_lastResultCount = results.size();

    const QString apiKey = UserSettings::instance().rebrickableApiKey();

    int row = 0;

    ColorRepository colorRepository;

    for (const InventorySearchResult& result : results) {
        m_resultsTable->insertRow(row);

        const QString partNumber = result.partNumber;

        int rebrickableColorId = -1;

        const std::optional<Color> color = colorRepository.getById(result.colorId);

        if (color) {
            rebrickableColorId = color->rebrickableId();
        }

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

        auto* imageItem = new QTableWidgetItem();

        imageItem->setTextAlignment(Qt::AlignCenter);

        auto* partNumberItem = new QTableWidgetItem(partNumber);

        partNumberItem->setData(Qt::UserRole, result.inventoryRecordId);

        auto* nameItem = new QTableWidgetItem(result.partName);

        auto* categoryItem = new QTableWidgetItem(result.categoryName);

        auto* colorItem = new QTableWidgetItem(result.colorName);

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

        auto* quantityItem = new QTableWidgetItem(QString::number(result.quantity));

        QString storagePath = storagePathForId(result.storageLocationId);

        if (storagePath.isEmpty()) {
            storagePath = result.storageLocationName;
        }

        auto* storageItem = new QTableWidgetItem(storagePath);

        auto* conditionItem = new QTableWidgetItem(result.condition);

        auto* ownershipItem = new QTableWidgetItem(result.ownershipType);

        auto* actionCombo = new QComboBox(m_resultsTable);

        actionCombo->addItem("Actions...");
        actionCombo->addItem("Details", "details");
        actionCombo->addItem("Edit", "edit");
        actionCombo->addItem("Move", "move");
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
                        MoveInventoryDialog dialog(inventoryRecordId, m_workspaceContext, this);

                        if (dialog.exec() == QDialog::Accepted) {
                            refresh();
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
                        InventoryHistoryDialog dialog(partId, colorId, m_workspaceContext, this);

                        dialog.exec();
                    }

                    // Return the action control
                    // to its neutral state.
                    actionCombo->setCurrentIndex(0);
                });

        m_resultsTable->setItem(row, 0, imageItem);

        m_resultsTable->setItem(row, 1, partNumberItem);

        m_resultsTable->setItem(row, 2, nameItem);

        m_resultsTable->setItem(row, 3, categoryItem);

        m_resultsTable->setItem(row, 4, colorItem);

        m_resultsTable->setItem(row, 5, quantityItem);

        m_resultsTable->setItem(row, 6, storageItem);

        m_resultsTable->setItem(row, 7, conditionItem);

        m_resultsTable->setItem(row, 8, ownershipItem);

        m_resultsTable->setCellWidget(row, 9, actionCombo);

        //
        // My Loose Inventory prefers an actual Part+Color
        // image when one is already cached.
        //
        // Until the background worker fills that cache,
        // fall back to the existing generic Part image.
        //
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

        ++row;
    }

    if (results.isEmpty()) {
        if (m_currentPage > 0) {
            m_resultLabel->setText("No more matching inventory records.");
        } else {
            const bool hasSearchText = !criteria.searchText.isEmpty();
            const bool hasCategoryFilter = criteria.categoryId > 0;
            const bool hasColorFilter = criteria.colorId > 0;
            const bool hasStorageFilter = criteria.storageLocationId > 0;

            if (hasSearchText || hasCategoryFilter || hasColorFilter || hasStorageFilter) {
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
}

void MyInventoryWidget::previousPage()
{
    if (m_currentPage <= 0)
        return;

    --m_currentPage;

    searchInventory();
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

    searchInventory();
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

    loadStorageLocations();

    searchInventory();
}

void MyInventoryWidget::settingsChanged()
{
    m_currentPage = 0;

    searchInventory();
}

void MyInventoryWidget::importCsv()
{
    if (!m_workspaceContext.hasCurrentWorkspace()) {
        return;
    }

    ImportInventoryDialog dialog(m_workspaceContext, this);

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
    if (!m_workspaceContext.hasCurrentWorkspace()) {
        return;
    }

    AddInventoryDialog dialog(m_workspaceContext, this);

    //
    // When My Inventory is scoped to a specific operational storage
    // location, use that location as the initial Add Part destination.
    // "All Locations" remains neutral and leaves the dialog's normal
    // default unchanged.
    //
    const int filteredStorageLocationId =
        m_storageCombo->currentData().toInt();

    if (filteredStorageLocationId > 0)
        dialog.setPreferredStorageLocationId(filteredStorageLocationId);

    dialog.exec();

    //
    // Multiple items may have been entered while
    // Keep Open was enabled. Refresh only once when
    // the rapid-entry session finishes.
    //
    if (dialog.inventoryWasAdded()) {
        searchInventory();
        emit inventoryChanged();
    }
}

void MyInventoryWidget::showLostInventory()
{
    if (!m_workspaceContext.hasCurrentWorkspace()) {
        return;
    }

    LostInventoryDialog dialog(m_workspaceContext, this);

    dialog.exec();

    //
    // Found operations may have returned pieces
    // to loose inventory.
    //
    searchInventory();
}