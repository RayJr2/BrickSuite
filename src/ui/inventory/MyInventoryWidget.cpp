#include "MyInventoryWidget.h"
#include "EditInventoryDialog.h"
#include "ImportInventoryDialog.h"
#include "InventoryHistoryDialog.h"
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
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
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

    m_importButton = new QPushButton("Import CSV", this);

    auto* titleLayout = new QHBoxLayout();

    titleLayout->addWidget(titleLabel);

    titleLayout->addStretch();

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

    auto* pagingLayout =
        new QHBoxLayout();

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
                if (!m_rowsByPartNumber.contains(partNumber)) {
                    return;
                }

                QPixmap pixmap(imagePath);

                if (pixmap.isNull())
                    return;

                const QPixmap thumbnail = pixmap.scaled(44,
                                                        44,
                                                        Qt::KeepAspectRatio,
                                                        Qt::SmoothTransformation);

                const QList<int> rows = m_rowsByPartNumber.value(partNumber);

                for (const int row : rows) {
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

void MyInventoryWidget::workspaceChanged(int workspaceId)
{
    Q_UNUSED(workspaceId);

    m_currentPage = 0;

    loadStorageLocations();

    searchInventory();

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
    m_storageCombo->clear();

    m_storageCombo->addItem("All Locations", 0);

    m_storagePathById.clear();

    if (!m_workspaceContext.hasCurrentWorkspace())
        return;

    StorageLocationRepository repository;

    const QList<StorageLocation> locations = repository.getByWorkspace(
        m_workspaceContext.currentWorkspaceId());

    QHash<int, StorageLocation> locationById;

    for (const StorageLocation& location : locations) {
        locationById.insert(location.id(), location);
    }

    for (const StorageLocation& location : locations) {
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
    }
}

void MyInventoryWidget::searchInventory()
{
    m_resultsTable->setRowCount(0);

    m_rowsByPartNumber.clear();
    m_rowsByPartColor.clear();
    m_partDetailsRequested.clear();

    if (!m_workspaceContext.hasCurrentWorkspace()) {
        m_lastResultCount = 0;

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
            m_resultLabel->setText("No inventory records found.");
        }
    } else {
        const int firstResult = criteria.offset + 1;

        const int lastResult = criteria.offset + results.size();

        m_resultLabel->setText(QString("Showing results %1 - %2.").arg(firstResult).arg(lastResult));
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
    if (m_lastResultCount < ResultsPerPage) {
        return;
    }

    ++m_currentPage;

    searchInventory();

    if (m_lastResultCount == 0) {
        --m_currentPage;

        searchInventory();
    }
}

void MyInventoryWidget::updatePagingControls()
{
    m_previousButton->setEnabled(m_currentPage > 0);

    const int resultsPerPage = UserSettings::instance().resultsPerPage();

    m_nextButton->setEnabled(m_lastResultCount >= resultsPerPage);

    m_pageLabel->setText(QString("Page %1").arg(m_currentPage + 1));
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
        //
        // Protect against the table changing between
        // the background download and this update.
        //
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
}