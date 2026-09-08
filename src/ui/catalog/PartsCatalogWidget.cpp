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
#include "../helpers/LargeViewLoadingGuard.h"

#include <QComboBox>
#include <QDebug>
#include <QElapsedTimer>
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

                QElapsedTimer imagePhaseTimer;
                imagePhaseTimer.start();
                QPixmap pixmap(imagePath);
                const qint64 decodeMs = imagePhaseTimer.elapsed();

                const auto recordCachedTiming = [this, &partNumber](qint64 decode,
                                                                    qint64 scale,
                                                                    qint64 apply) {
                    if (!m_timedCachedImages.remove(partNumber))
                        return;
                    m_cachedImageDecodeMs += decode;
                    m_cachedImageScaleMs += scale;
                    m_cachedImageApplyMs += apply;
                    if (m_timedCachedImages.isEmpty()) {
                        qInfo().noquote()
                            << QStringLiteral("Performance PartsCatalog cached-images "
                                              "decode=%1ms scale=%2ms apply=%3ms")
                                   .arg(m_cachedImageDecodeMs)
                                   .arg(m_cachedImageScaleMs)
                                   .arg(m_cachedImageApplyMs);
                    }
                };

                if (pixmap.isNull()) {
                    recordCachedTiming(decodeMs, 0, 0);
                    return;
                }

                imagePhaseTimer.restart();
                const QPixmap thumbnail = pixmap.scaled(44,
                                                        44,
                                                        Qt::KeepAspectRatio,
                                                        Qt::SmoothTransformation);
                const qint64 scaleMs = imagePhaseTimer.elapsed();

                imagePhaseTimer.restart();
                QTableWidgetItem* item = m_resultsTable->item(row, 0);

                if (!item) {
                    item = new QTableWidgetItem();

                    m_resultsTable->setItem(row, 0, item);
                }

                item->setIcon(QIcon(thumbnail));
                const qint64 applyMs = imagePhaseTimer.elapsed();
                recordCachedTiming(decodeMs, scaleMs, applyMs);
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

void PartsCatalogWidget::searchParts(const QString& loadingMessage)
{
    LargeViewLoadingGuard loading(
        this, m_refreshInProgress,
        loadingMessage.isEmpty() ? QStringLiteral("Loading Parts Catalog...") : loadingMessage,
        {m_searchButton, m_searchEdit, m_categoryCombo},
        {m_previousButton, m_nextButton});
    if (!loading.active())
        return;

    QElapsedTimer totalTimer;
    totalTimer.start();
    PartSearchCriteria criteria;

    criteria.searchText = m_searchEdit->text().trimmed();

    criteria.categoryId = m_categoryCombo->currentData().toInt();

    const int resultsPerPage = UserSettings::instance().resultsPerPage();

    criteria.limit = resultsPerPage;

    criteria.offset = m_currentPage * resultsPerPage;

    PartRepository repository;

    QElapsedTimer phaseTimer;
    phaseTimer.start();
    m_totalResultCount = repository.count(criteria);
    const qint64 countMs = phaseTimer.elapsed();

    phaseTimer.restart();
    const QList<PartSearchResult> results = repository.search(criteria);
    const qint64 searchMs = phaseTimer.elapsed();

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

    phaseTimer.restart();
    m_resultsTable->setRowCount(0);

    m_rowByPartNumber.clear();
    m_timedCachedImages.clear();
    m_cachedImageDecodeMs = 0;
    m_cachedImageScaleMs = 0;
    m_cachedImageApplyMs = 0;
    const qint64 clearMs = phaseTimer.elapsed();

    QList<int> missingPartEnrichment;
    qint64 itemCreationMs = 0;
    qint64 actionCreationMs = 0;
    qint64 tableAttachMs = 0;
    qint64 imagePathLookupMs = 0;
    qint64 imageDispatchMs = 0;
    const bool tableUpdatesEnabled = m_resultsTable->updatesEnabled();
    m_resultsTable->setUpdatesEnabled(false);
    m_resultsTable->setRowCount(results.size());
    phaseTimer.restart();
    int row = 0;

    for (const PartSearchResult& result : results) {
        const Part& part = result.part;

        const QString partNumber = part.partNumber();

        //
        // Remember which table row belongs
        // to this part number so the imageReady()
        // signal can update the correct row later.
        //
        m_rowByPartNumber.insert(partNumber, row);

        QElapsedTimer rowPhaseTimer;
        rowPhaseTimer.start();
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
        itemCreationMs += rowPhaseTimer.elapsed();

        rowPhaseTimer.restart();
        auto* actionCombo = new QComboBox(m_resultsTable);

        actionCombo->addItem("Actions...");

        actionCombo->addItem("Details", "details");

        actionCombo->addItem("Add to Inventory", "add");
        actionCombo->addItem("Add to Part Reference...", "reference");

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
                    } else if (action == "reference") {
                        emit addPartToReferenceRequested(partId);
                    }

                    // Return to neutral state.
                    actionCombo->setCurrentIndex(0);
                });
        actionCreationMs += rowPhaseTimer.elapsed();

        rowPhaseTimer.restart();
        m_resultsTable->setItem(row, 0, imageItem);

        m_resultsTable->setItem(row, 1, partNumberItem);

        m_resultsTable->setItem(row, 2, nameItem);

        m_resultsTable->setItem(row, 3, categoryItem);

        m_resultsTable->setItem(row, 4, materialItem);

        m_resultsTable->setItem(row, 5, matchItem);

        m_resultsTable->setCellWidget(row, 6, actionCombo);
        tableAttachMs += rowPhaseTimer.elapsed();

        //
        // Resolve thumbnail.
        //
        rowPhaseTimer.restart();
        const QString cachedPath = m_partImageService->cachedImagePath(partNumber);
        imagePathLookupMs += rowPhaseTimer.elapsed();

        if (!cachedPath.isEmpty()) {
            m_timedCachedImages.insert(partNumber);
            rowPhaseTimer.restart();
            // Cache hits are emitted one per event-loop turn so decode, scale,
            // and cell updates do not block the initial table construction.
            m_partImageService->requestPartImage(partNumber, QString());
            imageDispatchMs += rowPhaseTimer.elapsed();
        }

        missingPartEnrichment.append(part.id());

        ++row;
    }

    const qint64 rowAndImageMs = phaseTimer.elapsed();
    QElapsedTimer finalizeTimer;
    finalizeTimer.start();
    m_resultsTable->setUpdatesEnabled(tableUpdatesEnabled);
    const qint64 tableFinalizeMs = finalizeTimer.elapsed();
    phaseTimer.restart();
    requestMissingPartEnrichment(missingPartEnrichment);
    const qint64 enrichmentMs = phaseTimer.elapsed();
    phaseTimer.restart();

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
    const qint64 statusMs = phaseTimer.elapsed();
    qInfo().noquote()
        << QStringLiteral("Performance PartsCatalog page=%1 rows=%2 total=%3ms count=%4ms "
                          "search=%5ms clear=%6ms item-create=%7ms actions=%8ms "
                          "table-attach=%9ms row-other=%10ms image-path=%11ms "
                          "image-dispatch=%12ms table-finalize=%13ms "
                          "enrichment-schedule=%14ms status=%15ms "
                          "repository-lookups=0")
               .arg(m_currentPage + 1).arg(results.size()).arg(totalTimer.elapsed())
               .arg(countMs).arg(searchMs).arg(clearMs)
               .arg(itemCreationMs).arg(actionCreationMs).arg(tableAttachMs)
               .arg(qMax<qint64>(0, rowAndImageMs - itemCreationMs - actionCreationMs
                                    - tableAttachMs - imagePathLookupMs - imageDispatchMs))
               .arg(imagePathLookupMs).arg(imageDispatchMs).arg(tableFinalizeMs)
               .arg(enrichmentMs).arg(statusMs);
}

void PartsCatalogWidget::requestMissingPartEnrichment(const QList<int>& partIds)
{
    if (partIds.isEmpty() || !m_enrichmentService)
        return;
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

    searchParts(QStringLiteral("Loading page %1...").arg(m_currentPage + 1));
}

void PartsCatalogWidget::nextPage()
{
    const int resultsPerPage = UserSettings::instance().resultsPerPage();
    const int totalPages = qMax(1, (m_totalResultCount + resultsPerPage - 1) / resultsPerPage);

    if (m_currentPage + 1 >= totalPages)
        return;

    ++m_currentPage;

    searchParts(QStringLiteral("Loading page %1...").arg(m_currentPage + 1));
}

void PartsCatalogWidget::settingsChanged()
{
    m_currentPage = 0;

    searchParts();
}

void PartsCatalogWidget::refreshCatalog()
{
    loadCategories();
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
