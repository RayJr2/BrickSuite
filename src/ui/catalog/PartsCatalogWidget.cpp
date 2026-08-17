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

#include "PartsCatalogWidget.h"

#include "../../import/RebrickablePartCatalogImporter.h"
#include "../../models/Part.h"
#include "../../models/PartCategory.h"
#include "../../models/PartSearchCriteria.h"
#include "../../models/PartSearchResult.h"
#include "../../repositories/PartCategoryRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../services/RebrickableApiClient.h"
#include "../../services/images/PartImageService.h"
#include "../../settings/UserSettings.h"
#include "../parts/PartDetailsDialog.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

PartsCatalogWidget::PartsCatalogWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* titleLayout = new QHBoxLayout();

    auto* titleLabel = new QLabel("Parts Catalog", this);

    m_importPartsButton = new QPushButton("Import Rebrickable parts.csv", this);

    mainLayout->addLayout(titleLayout);

    titleLayout->addStretch();

    titleLayout->addWidget(m_importPartsButton);

    auto* filterLayout = new QHBoxLayout();

    m_searchEdit = new QLineEdit(this);

    m_searchEdit->setPlaceholderText("Search by part number or name");

    m_categoryCombo = new QComboBox(this);

    m_searchButton = new QPushButton("Search", this);

    filterLayout->addWidget(new QLabel("Search:", this));

    filterLayout->addWidget(m_searchEdit, 2);

    filterLayout->addWidget(new QLabel("Category:", this));

    filterLayout->addWidget(m_categoryCombo, 1);

    filterLayout->addWidget(m_searchButton);

    m_resultLabel = new QLabel(this);

    m_resultsTable = new QTableWidget(this);

    m_resultsTable->setColumnCount(6);

    m_resultsTable->setHorizontalHeaderLabels(QStringList() << "Image"
                                                            << "Part #"
                                                            << "Name"
                                                            << "Category"
                                                            << "Material"
                                                            << "Action");

    m_resultsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_resultsTable->setSelectionMode(QAbstractItemView::SingleSelection);

    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_resultsTable->verticalHeader()->setVisible(false);

    m_resultsTable->horizontalHeader()->setStretchLastSection(false);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);

    m_resultsTable->setColumnWidth(0, 56);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    m_resultsTable->setIconSize(QSize(44, 44));
    m_resultsTable->verticalHeader()->setDefaultSectionSize(52);

    // Pagination controls
    m_previousButton = new QPushButton("Previous", this);

    m_nextButton = new QPushButton("Next", this);

    m_pageLabel = new QLabel(this);
    m_summaryLabel = new QLabel(this);

    auto* pagingLayout = new QHBoxLayout();

    pagingLayout->addWidget(m_summaryLabel);

    pagingLayout->addStretch();

    pagingLayout->addWidget(m_previousButton);

    pagingLayout->addWidget(m_pageLabel);

    pagingLayout->addWidget(m_nextButton);

    mainLayout->addWidget(titleLabel);

    mainLayout->addLayout(filterLayout);

    mainLayout->addWidget(m_resultLabel);

    mainLayout->addWidget(m_resultsTable);

    mainLayout->addLayout(pagingLayout);

    // Initialize services
    m_partImageService = new PartImageService(this);

    m_rebrickableApiClient = new RebrickableApiClient(this);

    connect(m_searchButton, &QPushButton::clicked, this, [this]() {
        m_currentPage = 0;
        searchParts();
    });

    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        m_currentPage = 0;
        searchParts();
    });

    connect(m_categoryCombo, &QComboBox::currentIndexChanged, this, [this]() {
        m_currentPage = 0;
        searchParts();
    });

    connect(m_previousButton, &QPushButton::clicked, this, &PartsCatalogWidget::previousPage);

    connect(m_nextButton, &QPushButton::clicked, this, &PartsCatalogWidget::nextPage);

    connect(m_partImageService,
            &PartImageService::imageReady,
            this,
            [this](const QString& partNumber, const QString& imagePath) {
                if (!m_rowByPartNumber.contains(partNumber))
                    return;

                const int row = m_rowByPartNumber.value(partNumber);

                QPixmap pixmap(imagePath);

                if (pixmap.isNull())
                    return;

                const QPixmap thumbnail = pixmap.scaled(44,
                                                        44,
                                                        Qt::KeepAspectRatio,
                                                        Qt::SmoothTransformation);

                QTableWidgetItem* item = m_resultsTable->item(row, 0);

                if (!item) {
                    item = new QTableWidgetItem();

                    m_resultsTable->setItem(row, 0, item);
                }

                item->setIcon(QIcon(thumbnail));
            });

    connect(m_rebrickableApiClient,
            &RebrickableApiClient::partImageUrlsFinished,
            this,
            [this](const RebrickableApiClient::PartImageUrlsResult& result) {
                if (!result.success)
                    return;

                for (const RebrickableApiClient::PartImageUrl& part : result.parts) {
                    if (part.partImageUrl.isEmpty())
                        continue;

                    //
                    // Cache the image even if the user has already moved
                    // to another catalog page. imageReady() updates a row
                    // only when that part is currently visible.
                    //
                    m_partImageService->requestPartImage(part.partNumber,
                                                         part.partImageUrl);
                }
            });

    connect(m_importPartsButton, &QPushButton::clicked, this, &PartsCatalogWidget::importPartsCsv);

    loadCategories();

    m_resultLabel->setText("Enter search criteria or select a category.");

    updatePagingControls();
}

void PartsCatalogWidget::loadCategories()
{
    m_categoryCombo->clear();

    m_categoryCombo->addItem("All Categories", 0);

    PartCategoryRepository repository;

    const QList<PartCategory> categories = repository.getAll();

    for (const PartCategory& category : categories) {
        m_categoryCombo->addItem(category.name(), category.id());
    }
}

void PartsCatalogWidget::searchParts()
{
    PartSearchCriteria criteria;

    criteria.searchText = m_searchEdit->text().trimmed();

    criteria.categoryId = m_categoryCombo->currentData().toInt();

    const int resultsPerPage = UserSettings::instance().resultsPerPage();

    criteria.limit = resultsPerPage;

    criteria.offset = m_currentPage * resultsPerPage;

    PartRepository repository;

    m_totalResultCount = repository.count(criteria);

    const QList<PartSearchResult> results = repository.search(criteria);

    m_lastResultCount = results.size();

    m_resultsTable->setRowCount(0);

    m_rowByPartNumber.clear();

    QStringList missingPartImages;

    int row = 0;

    for (const PartSearchResult& result : results) {
        const Part& part = result.part;

        const QString partNumber = part.partNumber();

        m_resultsTable->insertRow(row);

        //
        // Remember which table row belongs
        // to this part number so the imageReady()
        // signal can update the correct row later.
        //
        m_rowByPartNumber.insert(partNumber, row);

        auto* imageItem = new QTableWidgetItem();

        imageItem->setTextAlignment(Qt::AlignCenter);

        auto* partNumberItem = new QTableWidgetItem(partNumber);

        partNumberItem->setData(Qt::UserRole, part.id());

        auto* nameItem = new QTableWidgetItem(part.name());

        auto* categoryItem = new QTableWidgetItem(result.categoryName);

        auto* materialItem = new QTableWidgetItem(part.material());

        auto* actionCombo = new QComboBox(m_resultsTable);

        actionCombo->addItem("Actions...");

        actionCombo->addItem("Details", "details");

        actionCombo->addItem("Add to Inventory", "add");

        const int partId = part.id();

        connect(actionCombo,
                &QComboBox::currentIndexChanged,
                this,
                [this, actionCombo, partId](int index) {
                    if (index <= 0)
                        return;

                    const QString action = actionCombo->itemData(index).toString();

                    if (action == "details") {
                        PartDetailsDialog dialog(partId, this);

                        dialog.exec();
                    } else if (action == "add") {
                        emit addPartToInventoryRequested(partId);
                    }

                    // Return to neutral state.
                    actionCombo->setCurrentIndex(0);
                });

        m_resultsTable->setItem(row, 0, imageItem);

        m_resultsTable->setItem(row, 1, partNumberItem);

        m_resultsTable->setItem(row, 2, nameItem);

        m_resultsTable->setItem(row, 3, categoryItem);

        m_resultsTable->setItem(row, 4, materialItem);

        m_resultsTable->setCellWidget(row, 5, actionCombo);

        //
        // Resolve thumbnail.
        //
        const QString cachedPath = m_partImageService->cachedImagePath(partNumber);

        if (!cachedPath.isEmpty()) {
            //
            // requestPartImage() will immediately emit
            // imageReady() when the file is already cached.
            //
            m_partImageService->requestPartImage(partNumber, QString());
        } else {
            missingPartImages.append(partNumber);
        }

        ++row;
    }

    requestMissingPartImages(missingPartImages);

    if (results.isEmpty()) {
        if (m_currentPage > 0) {
            m_resultLabel->setText("No more matching parts.");
        } else {
            const bool hasSearchText = !criteria.searchText.isEmpty();
            const bool hasCategoryFilter = criteria.categoryId > 0;

            if (hasSearchText || hasCategoryFilter) {
                m_resultLabel->setText("No parts match the current search or filters.");
            } else {
                m_resultLabel->setText("No parts are available in the Parts Catalog.");
            }
        }
    } else {
        const int firstResult = criteria.offset + 1;

        const int lastResult = criteria.offset + results.size();

        m_resultLabel->setText(QString("Showing results %1 - %2.").arg(firstResult).arg(lastResult));
    }

    updatePagingControls();
}

void PartsCatalogWidget::requestMissingPartImages(const QStringList& partNumbers)
{
    if (partNumbers.isEmpty())
        return;

    const QString apiKey = UserSettings::instance().rebrickableApiKey().trimmed();

    if (apiKey.isEmpty() || RebrickableApiClient::isSessionBlocked())
        return;

    //
    // Rebrickable recommends batching part detail lookups. Keep each
    // BrickSuite background request deliberately conservative at 20 parts.
    // The central API request queue still applies its normal rate limit and
    // gives foreground/user requests priority.
    //
    for (int first = 0; first < partNumbers.size(); first += PartImageBatchSize) {
        const QStringList batch = partNumbers.mid(first, PartImageBatchSize);

        m_rebrickableApiClient->getPartImageUrls(batch,
                                                 apiKey,
                                                 RebrickableApiClient::RequestPriority::Background);
    }
}

void PartsCatalogWidget::previousPage()
{
    if (m_currentPage <= 0)
        return;

    --m_currentPage;

    searchParts();
}

void PartsCatalogWidget::nextPage()
{
    const int resultsPerPage = UserSettings::instance().resultsPerPage();
    const int totalPages = qMax(1, (m_totalResultCount + resultsPerPage - 1) / resultsPerPage);

    if (m_currentPage + 1 >= totalPages)
        return;

    ++m_currentPage;

    searchParts();
}

void PartsCatalogWidget::settingsChanged()
{
    m_currentPage = 0;

    searchParts();
}

void PartsCatalogWidget::updatePagingControls()
{
    const int resultsPerPage = UserSettings::instance().resultsPerPage();
    const int totalPages = qMax(1, (m_totalResultCount + resultsPerPage - 1) / resultsPerPage);
    const int displayPage = qMin(m_currentPage + 1, totalPages);

    m_previousButton->setEnabled(m_currentPage > 0);
    m_nextButton->setEnabled(m_currentPage + 1 < totalPages);

    m_summaryLabel->setText(
        QString("%1 %2")
            .arg(QLocale().toString(m_totalResultCount))
            .arg(m_totalResultCount == 1 ? "Part" : "Parts"));

    m_pageLabel->setText(QString("Page %1 of %2").arg(displayPage).arg(totalPages));
}

void PartsCatalogWidget::importPartsCsv()
{
    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          "Import Rebrickable parts.csv",
                                                          QString(),
                                                          "CSV Files (*.csv)");

    if (fileName.isEmpty())
        return;

    const QMessageBox::StandardButton response
        = QMessageBox::question(this,
                                "Import Parts Catalog",
                                "Import/update the BrickSuite Parts Catalog "
                                "from this Rebrickable parts.csv file?\n\n"
                                "New Parts will be added and changed provider "
                                "data will be updated.\n\n"
                                "Parts already in BrickSuite will not be "
                                "deleted if they are absent from the CSV.",
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::No);

    if (response != QMessageBox::Yes)
        return;

    RebrickablePartCatalogImporter importer;

    const RebrickablePartCatalogImporter::Result result = importer.importFile(fileName);

    if (!result.success) {
        QMessageBox::critical(this, "Import Parts Catalog", result.message);

        return;
    }

    QMessageBox::information(this,
                             "Import Parts Catalog",
                             QString("Parts Catalog import completed.\n\n"
                                     "Rows Read: %1\n"
                                     "New: %2\n"
                                     "Updated: %3\n"
                                     "Unchanged: %4\n"
                                     "Skipped: %5")
                                 .arg(result.rowsRead)
                                 .arg(result.inserted)
                                 .arg(result.updated)
                                 .arg(result.unchanged)
                                 .arg(result.skipped));

    //
    // Categories do not normally change during a
    // parts.csv import, but reloading the combo keeps
    // the page completely synchronized with local
    // reference data.
    //
    loadCategories();

    m_currentPage = 0;

    searchParts();
}
