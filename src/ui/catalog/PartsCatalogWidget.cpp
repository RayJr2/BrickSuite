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
#include "../../import/RebrickablePartRelationshipImporter.h"
#include "../../models/Part.h"
#include "../../models/PartCategory.h"
#include "../../models/PartSearchCriteria.h"
#include "../../models/PartSearchResult.h"
#include "../../repositories/PartCategoryRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../services/RebrickableApiClient.h"
#include "../../services/parts/RebrickablePartAliasLearner.h"
#include "../../services/parts/PartExternalIdEnrichmentService.h"
#include "../../services/images/PartImageService.h"
#include "../../settings/UserSettings.h"
#include "../parts/PartDetailsDialog.h"

#include <QComboBox>
#include <QDebug>
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
#include <QRegularExpression>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

PartsCatalogWidget::PartsCatalogWidget(PartExternalIdEnrichmentService* enrichmentService,
                                       QWidget* parent)
    : QWidget(parent), m_enrichmentService(enrichmentService)
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* titleLayout = new QHBoxLayout();

    auto* titleLabel = new QLabel("Parts Catalog", this);

    m_importPartsButton = new QPushButton("Import Rebrickable Parts (CSV/ZIP)", this);

    m_importPartRelationshipsButton =
        new QPushButton("Import part_relationships (CSV/ZIP)", this);

    mainLayout->addLayout(titleLayout);

    titleLayout->addStretch();

    titleLayout->addWidget(m_importPartsButton);
    titleLayout->addWidget(m_importPartRelationshipsButton);

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

    m_resultsTable->setColumnCount(7);

    m_resultsTable->setHorizontalHeaderLabels(QStringList() << "Image"
                                                            << "Part #"
                                                            << "Name"
                                                            << "Category"
                                                            << "Material"
                                                            << "Match"
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

    m_resultsTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

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

    if (m_enrichmentService) {
        connect(m_enrichmentService,
                &PartExternalIdEnrichmentService::generalImageMetadataReady,
                m_partImageService,
                &PartImageService::requestPartImage);
    }

    connect(m_rebrickableApiClient,
            &RebrickableApiClient::partDetailsFinished,
            this,
            &PartsCatalogWidget::handlePartDetailsForAliasLearning);

    connect(m_importPartsButton,
            &QPushButton::clicked,
            this,
            &PartsCatalogWidget::importPartsCsv);

    connect(m_importPartRelationshipsButton,
            &QPushButton::clicked,
            this,
            &PartsCatalogWidget::importPartRelationshipsCsv);

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

    if (results.isEmpty()
        && !criteria.searchText.isEmpty()
        && criteria.categoryId <= 0
        && m_pendingAliasLookupPartNumber.isEmpty()) {
        RebrickablePartAliasLearner learner;
        const auto localLearned =
            learner.learnFromLocalExternalId(criteria.searchText);

        if (localLearned.learned) {
            m_currentPage = 0;
            searchParts();
            return;
        }

        static const QRegularExpression partNumberPattern(
            QStringLiteral("^[A-Za-z0-9._-]+$"));

        const QString apiKey =
            UserSettings::instance().rebrickableApiKey().trimmed();

        if (partNumberPattern.match(criteria.searchText).hasMatch()
            && !apiKey.isEmpty()
            && !RebrickableApiClient::isSessionBlocked()) {
            m_pendingAliasLookupPartNumber = criteria.searchText;

            m_resultLabel->setText(
                QStringLiteral("No local match. Checking Rebrickable for a Part Number Mapping..."));

            m_rebrickableApiClient->getPartDetails(
                criteria.searchText,
                apiKey);
        }
    }

    m_resultsTable->setRowCount(0);

    m_rowByPartNumber.clear();

    QStringList missingPartEnrichment;
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

        auto* matchItem = new QTableWidgetItem();

        if (result.matchedAlias()) {
            matchItem->setText(
                QString("Alias: %1").arg(result.matchedAliasPartNumber));

            QString tooltip =
                QString("Matched active alias %1")
                    .arg(result.matchedAliasPartNumber);

            if (!result.matchedAliasType.trimmed().isEmpty()) {
                tooltip +=
                    QString("\nType: %1").arg(result.matchedAliasType);
            }

            if (!result.matchedAliasSource.trimmed().isEmpty()) {
                tooltip +=
                    QString("\nSource: %1").arg(result.matchedAliasSource);
            }

            matchItem->setToolTip(tooltip);
        }

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

        m_resultsTable->setItem(row, 5, matchItem);

        m_resultsTable->setCellWidget(row, 6, actionCombo);

        //
        // Resolve thumbnail.
        //
        const QString cachedPath = m_partImageService->cachedImagePath(partNumber);

        if (!cachedPath.isEmpty()) {
            // requestPartImage() will immediately emit imageReady() when the
            // file is already cached. External-ID enrichment is independent
            // of image state and may still be needed below.
            m_partImageService->requestPartImage(partNumber, QString());
        }

        missingPartEnrichment.append(partNumber);

        ++row;
    }

    requestMissingPartEnrichment(missingPartEnrichment);

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

        if (results.size() == 1
            && results.first().matchedAlias()
            && results.first().matchedAliasPartNumber.compare(
                   criteria.searchText,
                   Qt::CaseInsensitive) == 0) {
            m_resultLabel->setText(
                QString("Matched alias %1 -> BrickSuite part %2.")
                    .arg(results.first().matchedAliasPartNumber,
                         results.first().part.partNumber()));
        } else {
            m_resultLabel->setText(
                QString("Showing results %1 - %2.")
                    .arg(firstResult)
                    .arg(lastResult));
        }
    }

    updatePagingControls();
}

void PartsCatalogWidget::requestMissingPartEnrichment(const QStringList& partNumbers)
{
    if (partNumbers.isEmpty() || !m_enrichmentService)
        return;
    PartRepository repository;
    QList<int> partIds;
    for (const QString& number : partNumbers) {
        const auto part = repository.getByPartNumber(number);
        if (part) partIds.append(part->id());
    }
    m_enrichmentService->ensureExternalIds(partIds);
}

void PartsCatalogWidget::handlePartDetailsForAliasLearning(
    const RebrickableService::PartDetailsResult& providerResult)
{
    if (m_pendingAliasLookupPartNumber.isEmpty())
        return;

    if (providerResult.requestedPartNumber.compare(
            m_pendingAliasLookupPartNumber,
            Qt::CaseInsensitive) != 0) {
        return;
    }

    const QString requestedPartNumber =
        m_pendingAliasLookupPartNumber;

    m_pendingAliasLookupPartNumber.clear();

    RebrickablePartAliasLearner learner;
    const auto learned = learner.learn(providerResult);

    if (!learned.learned) {
        const QString message =
            learned.message.trimmed().isEmpty()
                ? QStringLiteral("Rebrickable could not resolve this part number.")
                : learned.message;

        m_resultLabel->setText(
            QStringLiteral("No match. %1").arg(message));
        return;
    }

    // The learned alias is now local. Re-run the normal catalog search so
    // M17.2.6 displays the canonical part and alias provenance.
    m_currentPage = 0;
    searchParts();
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
                                                          "Import Rebrickable Parts Catalog",
                                                          QString(),
                                                          "Rebrickable Catalog Files (*.csv *.CSV *.zip *.ZIP);;"
                                                          "CSV Files (*.csv *.CSV);;ZIP Files (*.zip *.ZIP)");

    if (fileName.isEmpty())
        return;

    const QMessageBox::StandardButton response
        = QMessageBox::question(this,
                                "Import Parts Catalog",
                                "Import/update the BrickSuite Parts Catalog "
                                "from this Rebrickable CSV or ZIP file?\n\n"
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

void PartsCatalogWidget::importPartRelationshipsCsv()
{
    const QString fileName =
        QFileDialog::getOpenFileName(
            this,
            "Import Rebrickable part_relationships.csv",
            QString(),
            "Rebrickable CSV or ZIP (*.csv *.zip *.CSV *.ZIP)");

    if (fileName.isEmpty())
        return;

    const QMessageBox::StandardButton response =
        QMessageBox::question(
            this,
            "Import Part Relationships",
            "Import/update BrickSuite Part Relationships from this "
            "Rebrickable part_relationships.csv file?\n\n"
            "Existing Rebrickable relationships missing from the selected "
            "file will be deactivated, not deleted.\n\n"
            "This import does not create aliases or change Add Inventory "
            "behavior yet.",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

    if (response != QMessageBox::Yes)
        return;

    RebrickablePartRelationshipImporter importer;

    const RebrickablePartRelationshipImporter::Result result =
        importer.importFile(fileName);

    if (!result.success) {
        QMessageBox::critical(
            this,
            "Import Part Relationships",
            result.message);
        return;
    }

    QMessageBox::information(
        this,
        "Import Part Relationships",
        QString("Part Relationship import completed.\n\n"
                "Rows Read: %1\n"
                "New: %2\n"
                "Updated / Reactivated: %3\n"
                "Unchanged: %4\n"
                "Skipped - Invalid: %5\n"
                "Skipped - Missing Parent: %6\n"
                "Skipped - Missing Child: %7\n"
                "Deactivated: %8")
            .arg(result.rowsRead)
            .arg(result.inserted)
            .arg(result.updated)
            .arg(result.unchanged)
            .arg(result.skippedInvalid)
            .arg(result.skippedMissingParent)
            .arg(result.skippedMissingChild)
            .arg(result.deactivated));
}
