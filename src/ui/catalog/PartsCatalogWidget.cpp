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
#include "../../repositories/ExternalPartIdentifierRepository.h"
#include "../../repositories/ExternalPartMappingRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../services/RebrickableApiClient.h"
#include "../../services/parts/RebrickablePartAliasLearner.h"
#include "../../services/mappings/BrickLinkMappingService.h"
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
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace
{
bool hasProviderId(const QHash<QString, QStringList>& externalIds,
                   const QString& provider)
{
    for (auto it = externalIds.constBegin(); it != externalIds.constEnd(); ++it) {
        if (it.key().compare(provider, Qt::CaseInsensitive) != 0)
            continue;

        for (const QString& id : it.value()) {
            if (!id.trimmed().isEmpty())
                return true;
        }
    }

    return false;
}

bool isLikelyPrintedPartNumber(const QString& partNumber)
{
    static const QRegularExpression printedPattern(
        QStringLiteral("p(?:r|b)\\d"),
        QRegularExpression::CaseInsensitiveOption);

    return printedPattern.match(partNumber).hasMatch();
}
}

PartsCatalogWidget::PartsCatalogWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* titleLayout = new QHBoxLayout();

    auto* titleLabel = new QLabel("Parts Catalog", this);

    m_importPartsButton = new QPushButton("Import Rebrickable parts.csv", this);

    m_importPartRelationshipsButton =
        new QPushButton("Import part_relationships.csv", this);

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

    connect(m_rebrickableApiClient,
            &RebrickableApiClient::partDetailsFinished,
            this,
            &PartsCatalogWidget::handlePartDetailsForAliasLearning);

    connect(m_rebrickableApiClient,
            &RebrickableApiClient::partDetailsFinished,
            this,
            &PartsCatalogWidget::handlePartDetailsForExternalIdEnrichment);

    connect(m_rebrickableApiClient,
            &RebrickableApiClient::partImageUrlsFinished,
            this,
            [this](const RebrickableApiClient::PartImageUrlsResult& result) {
                if (!result.success)
                    return;

                ExternalPartIdentifierRepository externalIdentifierRepository;
                BrickLinkMappingService mappingService;
                PartRepository partRepository;
                QSet<QString> returnedPartNumbers;

                for (const RebrickableApiClient::PartImageUrl& part : result.parts) {
                    returnedPartNumbers.insert(part.partNumber.toLower());

                    const std::optional<Part> localPart =
                        partRepository.getByPartNumber(part.partNumber);

                    if (localPart) {
                        externalIdentifierRepository.replaceProviderIds(
                            localPart->id(),
                            part.externalIds,
                            QStringLiteral("Rebrickable"));

                        mappingService.storePartExternalIds(
                            localPart->id(),
                            part.externalIds);

                        const bool hasBrickLink =
                            hasProviderId(part.externalIds, QStringLiteral("BrickLink"));

                        if (hasBrickLink) {
                            externalIdentifierRepository.setLookupStatus(
                                localPart->id(),
                                QStringLiteral("Rebrickable"),
                                QStringLiteral("Loaded"));
                        } else if (isLikelyPrintedPartNumber(part.partNumber)) {
                            // Rebrickable's batched parts-list response can omit
                            // BrickLink IDs that are present on the direct Part
                            // Details endpoint. Printed variants are the main
                            // BrickScan workflow, so fill that gap with a
                            // throttled background detail request instead of
                            // incorrectly marking the lookup complete.
                            requestPrintedPartExternalIdDetails(part.partNumber);
                        } else {
                            externalIdentifierRepository.setLookupStatus(
                                localPart->id(),
                                QStringLiteral("Rebrickable"),
                                QStringLiteral("Loaded"));
                        }
                    }

                    if (!part.partImageUrl.isEmpty()) {
                        // Cache the image even if the user has already moved
                        // to another catalog page. imageReady() updates a row
                        // only when that part is currently visible.
                        m_partImageService->requestPartImage(part.partNumber,
                                                             part.partImageUrl);
                    }
                }

                // A successful batch can legitimately omit a requested part.
                // Remember that terminal result so we do not continuously
                // retry it while browsing the same catalog page.
                for (const QString& requestedPartNumber : result.requestedPartNumbers) {
                    if (returnedPartNumbers.contains(requestedPartNumber.toLower()))
                        continue;

                    const std::optional<Part> localPart =
                        partRepository.getByPartNumber(requestedPartNumber);

                    if (localPart) {
                        if (isLikelyPrintedPartNumber(requestedPartNumber)) {
                            // Printed/decorated variants are sometimes omitted
                            // from Rebrickable's batched parts-list response
                            // even though the direct Part Details endpoint can
                            // resolve them. Do not mark these terminally
                            // unavailable until the direct lookup has run.
                            qInfo() << "BrickLink external ID enrichment: batch omitted printed part;"
                                    << "requesting direct details."
                                    << "RebrickablePart:" << requestedPartNumber;

                            requestPrintedPartExternalIdDetails(requestedPartNumber);
                        } else {
                            externalIdentifierRepository.setLookupStatus(
                                localPart->id(),
                                QStringLiteral("Rebrickable"),
                                QStringLiteral("Unavailable"));
                        }
                    }
                }
            });

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
    ExternalPartIdentifierRepository externalIdentifierRepository;
    ExternalPartMappingRepository externalPartMappingRepository;

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

        bool needsExternalIds =
            !externalIdentifierRepository.isLookupComplete(
                part.id(), QStringLiteral("Rebrickable"));

        // Schema v20 initially marked a successful parts-list lookup as
        // Loaded even when that response omitted BrickLink IDs. Printed
        // parts are the BrickScan use case, so treat a missing BrickLink
        // mapping as still needing enrichment. This also repairs databases
        // that were already marked Loaded by the first v20 implementation.
        if (isLikelyPrintedPartNumber(partNumber)) {
            const auto brickLinkMapping =
                externalPartMappingRepository.getByPartAndProvider(
                    part.id(), QStringLiteral("BrickLink"));

            if (!brickLinkMapping
                || brickLinkMapping->externalId.trimmed().isEmpty()) {
                needsExternalIds = true;
            }
        }

        if (!cachedPath.isEmpty()) {
            // requestPartImage() will immediately emit imageReady() when the
            // file is already cached. External-ID enrichment is independent
            // of image state and may still be needed below.
            m_partImageService->requestPartImage(partNumber, QString());
        }

        if (cachedPath.isEmpty() || needsExternalIds)
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

void PartsCatalogWidget::requestPrintedPartExternalIdDetails(
    const QString& partNumber)
{
    const QString requested = partNumber.trimmed();
    if (requested.isEmpty())
        return;

    const QString key = requested.toLower();
    if (m_pendingExternalIdDetailLookups.contains(key))
        return;

    const QString apiKey = UserSettings::instance().rebrickableApiKey().trimmed();
    if (apiKey.isEmpty() || RebrickableApiClient::isSessionBlocked())
        return;

    m_pendingExternalIdDetailLookups.insert(key);

    qInfo() << "BrickLink external ID enrichment pending."
            << "RebrickablePart:" << requested
            << "Source: direct Part Details";

    m_rebrickableApiClient->getPartDetails(
        requested,
        apiKey,
        RebrickableApiClient::RequestPriority::Background);
}

void PartsCatalogWidget::handlePartDetailsForExternalIdEnrichment(
    const RebrickableService::PartDetailsResult& providerResult)
{
    const QString requested = providerResult.requestedPartNumber.trimmed();
    const QString key = requested.toLower();

    if (key.isEmpty() || !m_pendingExternalIdDetailLookups.contains(key))
        return;

    m_pendingExternalIdDetailLookups.remove(key);

    PartRepository partRepository;
    const std::optional<Part> localPart =
        partRepository.getByPartNumber(requested);

    if (!localPart)
        return;

    ExternalPartIdentifierRepository externalIdentifierRepository;

    if (!providerResult.success) {
        qWarning() << "BrickLink external ID enrichment failed."
                   << "RebrickablePart:" << requested
                   << "Message:" << providerResult.message;

        externalIdentifierRepository.setLookupStatus(
            localPart->id(),
            QStringLiteral("Rebrickable"),
            QStringLiteral("Unavailable"));
        return;
    }

    externalIdentifierRepository.replaceProviderIds(
        localPart->id(),
        providerResult.part.externalIds,
        QStringLiteral("Rebrickable"));

    BrickLinkMappingService mappingService;
    mappingService.storePartExternalIds(
        localPart->id(),
        providerResult.part.externalIds);

    QStringList brickLinkIds;
    for (auto it = providerResult.part.externalIds.constBegin();
         it != providerResult.part.externalIds.constEnd();
         ++it) {
        if (it.key().compare(QStringLiteral("BrickLink"), Qt::CaseInsensitive) == 0) {
            for (QString id : it.value()) {
                id = id.trimmed();
                if (!id.isEmpty())
                    brickLinkIds.append(id);
            }
            break;
        }
    }
    brickLinkIds.removeDuplicates();

    const bool hasBrickLink = !brickLinkIds.isEmpty();

    if (hasBrickLink) {
        qInfo() << "BrickLink external ID enrichment loaded."
                << "RebrickablePart:" << requested
                << "BrickLink:" << brickLinkIds;
    } else {
        qInfo() << "BrickLink external ID enrichment complete; no BrickLink ID returned."
                << "RebrickablePart:" << requested;
    }

    externalIdentifierRepository.setLookupStatus(
        localPart->id(),
        QStringLiteral("Rebrickable"),
        hasBrickLink
            ? QStringLiteral("Loaded")
            : QStringLiteral("Unavailable"));
}

void PartsCatalogWidget::requestMissingPartEnrichment(const QStringList& partNumbers)
{
    if (partNumbers.isEmpty())
        return;

    const QString apiKey = UserSettings::instance().rebrickableApiKey().trimmed();

    if (apiKey.isEmpty() || RebrickableApiClient::isSessionBlocked())
        return;

    //
    // Rebrickable's parts-list response supplies both the image URL and
    // external provider IDs when inc_part_details=1. One conservative
    // background request therefore fills both caches without a second API
    // call per part. Keep each batch deliberately small at 20 parts.
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

void PartsCatalogWidget::importPartRelationshipsCsv()
{
    const QString fileName =
        QFileDialog::getOpenFileName(
            this,
            "Import Rebrickable part_relationships.csv",
            QString(),
            "CSV Files (*.csv)");

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

