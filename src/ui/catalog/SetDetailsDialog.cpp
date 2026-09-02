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

#include "SetDetailsDialog.h"

#include "../../models/SetCatalogItem.h"
#include "../../models/SetCatalogPart.h"
#include "../../import/RebrickableSetPartsImporter.h"
#include "../../repositories/SetCatalogPartRepository.h"
#include "../../repositories/SetCatalogRepository.h"
#include "../../services/images/PartImageService.h"
#include "../../services/images/SetImageService.h"
#include "../../services/sets/RebrickableSetPartsService.h"
#include "../../services/sets/SetDetailsProviderService.h"
#include "../../settings/UserSettings.h"
#include "BricksetInstructionsDialog.h"

#include <QDebug>
#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

SetDetailsDialog::SetDetailsDialog(int setCatalogId, QWidget* parent)
    : QDialog(parent)
    , m_setCatalogId(setCatalogId)
{
    setWindowTitle("Set Details");

    resize(960, 850);

    auto* mainLayout = new QVBoxLayout(this);

    auto* contentLayout = new QHBoxLayout();

    m_imageLabel = new QLabel(this);

    m_imageLabel->setFixedSize(300, 300);

    m_imageLabel->setAlignment(Qt::AlignCenter);

    m_imageLabel->setText("No Image");

    m_imageLabel->setStyleSheet("QLabel { border: 1px solid gray; }");

    contentLayout->addWidget(m_imageLabel);

    auto* formLayout = new QFormLayout();

    m_setNumberLabel = new QLabel(this);

    m_nameLabel = new QLabel(this);

    m_nameLabel->setWordWrap(true);

    m_yearLabel = new QLabel(this);

    m_themeLabel = new QLabel(this);

    m_partsLabel = new QLabel(this);

    m_imageStatusLabel = new QLabel(this);

    m_imageStatusLabel->setWordWrap(true);

    formLayout->addRow("Set Number:", m_setNumberLabel);

    formLayout->addRow("Name:", m_nameLabel);

    formLayout->addRow("Year:", m_yearLabel);

    formLayout->addRow("Theme ID:", m_themeLabel);

    formLayout->addRow("Parts:", m_partsLabel);

    formLayout->addRow("Image:", m_imageStatusLabel);

    contentLayout->addLayout(formLayout, 1);

    mainLayout->addLayout(contentLayout);

    auto* providerGroup = new QGroupBox("Provider Enrichment", this);
    m_providerLayout = new QFormLayout(providerGroup);

    m_providerSourceLabel = new QLabel("Checking...", providerGroup);
    m_providerStatusLabel = new QLabel(providerGroup);
    m_providerStatusLabel->setWordWrap(true);

    m_bricksetIdLabel = new QLabel("-", providerGroup);
    m_providerThemeLabel = new QLabel("-", providerGroup);
    m_providerSubthemeLabel = new QLabel("-", providerGroup);
    m_minifigsLabel = new QLabel("-", providerGroup);
    m_availabilityLabel = new QLabel("-", providerGroup);
    m_ratingLabel = new QLabel("-", providerGroup);

    m_instructionsRowWidget = new QWidget(providerGroup);
    auto* instructionsRowLayout = new QHBoxLayout(m_instructionsRowWidget);
    instructionsRowLayout->setContentsMargins(0, 0, 0, 0);

    m_instructionsLabel = new QLabel("-", m_instructionsRowWidget);
    m_viewInstructionsButton = new QPushButton("View Instructions", m_instructionsRowWidget);
    m_viewInstructionsButton->setVisible(false);

    instructionsRowLayout->addWidget(m_instructionsLabel);
    instructionsRowLayout->addStretch();
    instructionsRowLayout->addWidget(m_viewInstructionsButton);

    m_additionalImagesLabel = new QLabel("-", providerGroup);

    m_providerLinkLabel = new QLabel("-", providerGroup);
    m_providerLinkLabel->setOpenExternalLinks(true);
    m_providerLinkLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);

    m_providerLayout->addRow("Source:", m_providerSourceLabel);
    m_providerLayout->addRow("Status:", m_providerStatusLabel);
    m_providerLayout->addRow("Brickset Set ID:", m_bricksetIdLabel);
    m_providerLayout->addRow("Theme:", m_providerThemeLabel);
    m_providerLayout->addRow("Subtheme:", m_providerSubthemeLabel);
    m_providerLayout->addRow("Minifigs:", m_minifigsLabel);
    m_providerLayout->addRow("Availability:", m_availabilityLabel);
    m_providerLayout->addRow("Rating:", m_ratingLabel);
    m_providerLayout->addRow("Instructions:", m_instructionsRowWidget);
    m_providerLayout->addRow("Additional Images:", m_additionalImagesLabel);
    m_providerLayout->addRow("Provider Link:", m_providerLinkLabel);

    // Until provider selection completes, show only the generic Source and
    // Status rows. Brickset-specific enrichment fields become visible only
    // when Brickset actually supplies them.
    setBricksetRowsVisible(false);

    mainLayout->addWidget(providerGroup);

    auto* compositionGroup = new QGroupBox("Catalog Parts List", this);
    auto* compositionLayout = new QVBoxLayout(compositionGroup);
    auto* compositionHeader = new QHBoxLayout();
    auto* compositionLabels = new QVBoxLayout();
    m_compositionSummaryLabel = new QLabel(compositionGroup);
    m_compositionSummaryLabel->setWordWrap(true);
    m_compositionStatusLabel = new QLabel(compositionGroup);
    m_compositionStatusLabel->setWordWrap(true);
    compositionLabels->addWidget(m_compositionSummaryLabel);
    compositionLabels->addWidget(m_compositionStatusLabel);
    m_getPartsButton = new QPushButton("Get Parts from Rebrickable...", compositionGroup);
    m_importPartsButton = new QPushButton("Import Parts List...", compositionGroup);
    compositionHeader->addLayout(compositionLabels, 1);
    compositionHeader->addWidget(m_getPartsButton);
    compositionHeader->addWidget(m_importPartsButton);
    compositionLayout->addLayout(compositionHeader);

    m_compositionTable = new QTableWidget(compositionGroup);
    m_compositionTable->setColumnCount(6);
    m_compositionTable->setHorizontalHeaderLabels(
        {"Image", "Part #", "Part Name", "Color", "Qty", "Spare"});
    m_compositionTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_compositionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_compositionTable->setIconSize(QSize(48, 48));
    m_compositionTable->verticalHeader()->setVisible(false);
    m_compositionTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_compositionTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_compositionTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_compositionTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_compositionTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_compositionTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    compositionLayout->addWidget(m_compositionTable);
    mainLayout->addWidget(compositionGroup, 1);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);

    m_createBuildButton = buttonBox->addButton("Create Build From Stock...",
                                               QDialogButtonBox::ActionRole);
    m_createBuildButton->setEnabled(false);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);

    m_imageService = new SetImageService(this);
    m_providerService = new SetDetailsProviderService(this);
    m_bricksetService = new BricksetService(this);
    m_partImageService = new PartImageService(this);
    m_rebrickablePartsService = new RebrickableSetPartsService(this);

    connect(m_getPartsButton, &QPushButton::clicked,
            this, &SetDetailsDialog::getPartsFromRebrickable);
    connect(m_importPartsButton, &QPushButton::clicked,
            this, &SetDetailsDialog::importPartsList);
    connect(m_createBuildButton, &QPushButton::clicked,
            this, &SetDetailsDialog::createBuildFromStock);
    connect(m_rebrickablePartsService, &RebrickableSetPartsService::finished,
            this, [this](const RebrickableSetPartsService::Result& result) {
        setCompositionActionsEnabled(true);
        m_compositionStatusLabel->clear();
        if (!result.success) {
            QMessageBox::critical(this, "Get Set Parts from Rebrickable", result.message);
            return;
        }
        loadComposition();
        QMessageBox::information(
            this, "Get Set Parts from Rebrickable",
            QString("Parts retrieved for %1 (%2).\n\n"
                    "Distinct composition rows: %3\nRequired pieces: %4\nSpare pieces: %5")
                .arg(m_setName, m_setNumber).arg(result.compositionRows)
                .arg(result.requiredPieces).arg(result.sparePieces));
    });

    connect(m_viewInstructionsButton,
            &QPushButton::clicked,
            this,
            &SetDetailsDialog::requestBricksetInstructions);

    connect(m_bricksetService,
            &BricksetService::instructionsFinished,
            this,
            [this](const BricksetService::InstructionsResult& result) {
                m_viewInstructionsButton->setEnabled(true);
                m_viewInstructionsButton->setText("View Instructions");

                if (result.setNumber != m_setNumber)
                    return;

                if (!result.success) {
                    QMessageBox::warning(this,
                                         "Brickset Instructions",
                                         result.message);
                    return;
                }

                if (result.instructions.isEmpty()) {
                    QMessageBox::information(this,
                                             "Brickset Instructions",
                                             QString("No instruction files were returned for %1.")
                                                 .arg(m_setNumber));
                    return;
                }

                BricksetInstructionsDialog dialog(m_setNumber,
                                                  result.instructions,
                                                  this);
                dialog.exec();
            });

    connect(m_providerService,
            &SetDetailsProviderService::detailsReady,
            this,
            [this](const SetDetailsProviderService::Result& result) {
                if (result.setNumber != m_setNumber)
                    return;

                if (result.source == SetDetailsProviderService::Source::Brickset
                    && result.hasEnrichment) {
                    const BricksetService::SetDetails& set = result.brickset;

                    setBricksetRowsVisible(true);

                    m_providerSourceLabel->setText("Brickset");
                    m_providerStatusLabel->setText(
                        QString("Brickset enrichment loaded. getSets calls this session: %1")
                            .arg(BricksetService::sessionGetSetsCallCount()));

                    m_bricksetIdLabel->setText(QString::number(set.bricksetSetId));
                    m_providerThemeLabel->setText(
                        set.theme.isEmpty() ? "-" : set.theme);
                    m_providerSubthemeLabel->setText(
                        set.subtheme.isEmpty() ? "-" : set.subtheme);
                    m_minifigsLabel->setText(QString::number(set.minifigs));
                    m_availabilityLabel->setText(
                        set.availability.isEmpty() ? "-" : set.availability);

                    if (set.ratingCount > 0) {
                        m_ratingLabel->setText(
                            QString("%1 / 5 (%2 ratings)")
                                .arg(set.rating, 0, 'f', 1)
                                .arg(set.ratingCount));
                    } else if (set.rating > 0.0) {
                        m_ratingLabel->setText(
                            QString("%1 / 5").arg(set.rating, 0, 'f', 1));
                    } else {
                        m_ratingLabel->setText("-");
                    }

                    m_instructionsLabel->setText(QString::number(set.instructionsCount));
                    m_viewInstructionsButton->setVisible(set.instructionsCount > 0);
                    m_viewInstructionsButton->setEnabled(set.instructionsCount > 0);

                    m_additionalImagesLabel->setText(
                        QString::number(set.additionalImageCount));

                    if (!set.bricksetUrl.isEmpty()) {
                        m_providerLinkLabel->setText(
                            QString("<a href=\"%1\">Open on Brickset</a>")
                                .arg(set.bricksetUrl.toHtmlEscaped()));
                    } else {
                        m_providerLinkLabel->setText("-");
                    }

                    return;
                }

                if (result.source == SetDetailsProviderService::Source::Rebrickable
                    && result.hasEnrichment) {
                    setBricksetRowsVisible(false);

                    m_providerSourceLabel->setText(
                        result.usedFallback ? "Rebrickable (fallback)" : "Rebrickable");

                    if (result.usedFallback) {
                        m_providerStatusLabel->setText(
                            result.fallbackReason.isEmpty()
                                ? "Brickset enrichment was unavailable; Rebrickable details are being used."
                                : QString("%1 Rebrickable details are being used.")
                                      .arg(result.fallbackReason));
                    } else {
                        m_providerStatusLabel->setText("Rebrickable details loaded.");
                    }

                    return;
                }

                setBricksetRowsVisible(false);

                m_providerSourceLabel->setText("Local catalog");
                m_providerStatusLabel->setText(
                    result.message.isEmpty()
                        ? "No external provider enrichment is available."
                        : result.message);
            });

    connect(m_imageService,
            &SetImageService::imageReady,
            this,
            [this](const QString& setNumber, const QString& imagePath) {
                if (setNumber != m_setNumber) {
                    return;
                }

                QPixmap pixmap(imagePath);

                if (pixmap.isNull()) {
                    m_imageStatusLabel->setText("Unable to read cached image.");

                    return;
                }

                m_imageLabel->setPixmap(pixmap.scaled(m_imageLabel->size(),
                                                      Qt::KeepAspectRatio,
                                                      Qt::SmoothTransformation));

                m_imageLabel->setText(QString());

                m_imageStatusLabel->setText("Cached");
            });

    connect(m_imageService,
            &SetImageService::imageFailed,
            this,
            [this](const QString& setNumber, const QString& message) {
                if (setNumber != m_setNumber) {
                    return;
                }

                m_imageStatusLabel->setText(message);
            });

    if (!loadSet())
        return;

    loadComposition();

    loadCachedImage();

    requestImage();

    requestProviderEnrichment();
}

bool SetDetailsDialog::loadSet()
{
    SetCatalogRepository repository;

    const std::optional<SetCatalogItem> set = repository.getById(m_setCatalogId);

    if (!set) {
        qWarning() << "Set Details could not load Set."
                   << "SetCatalogId:" << m_setCatalogId;

        m_setNumberLabel->setText("Unable to load Set.");

        return false;
    }

    m_setNumber = set->setNumber();

    m_setName = set->name();

    m_imageUrl = set->imageUrl();

    m_setNumberLabel->setText(set->setNumber());

    m_nameLabel->setText(set->name());

    m_yearLabel->setText(QString::number(set->year()));

    m_themeLabel->setText(QString::number(set->themeId()));

    m_partsLabel->setText(QString::number(set->numberOfParts()));

    setWindowTitle(QString("Set Details — %1").arg(set->setNumber()));

    return true;
}

void SetDetailsDialog::loadComposition()
{
    const QList<SetCatalogPart> composition =
        SetCatalogPartRepository().listForSet(m_setCatalogId);
    m_compositionTable->setRowCount(0);
    m_requiredPieces = 0;
    m_sparePieces = 0;
    for (const SetCatalogPart& part : composition) {
        if (part.isSpare)
            m_sparePieces += part.quantityRequired;
        else
            m_requiredPieces += part.quantityRequired;
        const int row = m_compositionTable->rowCount();
        m_compositionTable->insertRow(row);
        m_compositionTable->setRowHeight(row, 54);
        auto* imageItem = new QTableWidgetItem("No Image");
        QString imagePath = m_partImageService->cachedPartColorImagePath(
            part.partNumber, part.rebrickableColorId);
        if (imagePath.isEmpty())
            imagePath = m_partImageService->cachedImagePath(part.partNumber);
        const QPixmap pixmap(imagePath);
        if (!pixmap.isNull()) {
            imageItem->setText(QString());
            imageItem->setIcon(QIcon(pixmap));
        }
        imageItem->setData(Qt::UserRole, part.id);
        m_compositionTable->setItem(row, 0, imageItem);
        m_compositionTable->setItem(row, 1, new QTableWidgetItem(part.partNumber));
        m_compositionTable->setItem(row, 2, new QTableWidgetItem(part.partName));
        m_compositionTable->setItem(row, 3, new QTableWidgetItem(part.colorName));
        auto* quantity = new QTableWidgetItem(QString::number(part.quantityRequired));
        quantity->setTextAlignment(Qt::AlignCenter);
        m_compositionTable->setItem(row, 4, quantity);
        auto* spare = new QTableWidgetItem(part.isSpare ? "Yes" : "No");
        spare->setTextAlignment(Qt::AlignCenter);
        m_compositionTable->setItem(row, 5, spare);
    }
    if (composition.isEmpty()) {
        m_compositionSummaryLabel->setText("No catalog parts list has been acquired for this Set.");
    } else {
        m_compositionSummaryLabel->setText(
            QString("%1 distinct rows; %2 required pieces; %3 spare pieces retained.")
                .arg(composition.size()).arg(m_requiredPieces).arg(m_sparePieces));
    }
    m_createBuildButton->setEnabled(m_requiredPieces > 0);
    m_createBuildButton->setToolTip(m_requiredPieces > 0
        ? QString() : QStringLiteral("Get or import a parts list containing required pieces first."));
}

void SetDetailsDialog::setCompositionActionsEnabled(bool enabled)
{
    m_getPartsButton->setEnabled(enabled && !m_setNumber.isEmpty());
    m_importPartsButton->setEnabled(enabled);
}

void SetDetailsDialog::getPartsFromRebrickable()
{
    if (m_setNumber.isEmpty() || m_rebrickablePartsService->isBusy())
        return;
    const QString apiKey = UserSettings::instance().rebrickableApiKey().trimmed();
    if (apiKey.isEmpty()) {
        QMessageBox::information(this, "Rebrickable API Key Required",
                                 "Configure and test your Rebrickable API key in Settings before getting Set parts.");
        return;
    }
    if (!SetCatalogPartRepository().listForSet(m_setCatalogId).isEmpty()) {
        const auto answer = QMessageBox::question(
            this, "Replace Set Parts List",
            "This Set already has a catalog parts list. Replace it with the complete parts list returned by Rebrickable?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    setCompositionActionsEnabled(false);
    m_compositionStatusLabel->setText("Getting the complete parts list from Rebrickable...");
    m_rebrickablePartsService->retrieveAndReplace(m_setCatalogId, m_setNumber, apiKey);
}

void SetDetailsDialog::importPartsList()
{
    if (!SetCatalogPartRepository().listForSet(m_setCatalogId).isEmpty()) {
        const auto answer = QMessageBox::question(
            this, "Replace Set Parts List",
            QString("Import a new parts list for %1 (%2)?\n\n"
                    "This replaces the existing catalog parts list; it does not merge with it.")
                .arg(m_setName, m_setNumber),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    const QString fileName = QFileDialog::getOpenFileName(
        this, QString("Import Parts List for %1 (%2)").arg(m_setName, m_setNumber),
        QString(), "Rebrickable Parts Lists (*.csv *.CSV *.zip *.ZIP);;CSV Files (*.csv *.CSV);;ZIP Files (*.zip *.ZIP)");
    if (fileName.isEmpty())
        return;
    const auto result = RebrickableSetPartsImporter().importFile(m_setCatalogId, fileName);
    if (!result.success) {
        QMessageBox::critical(this, "Import Set Parts List", result.message);
        return;
    }
    loadComposition();
    QMessageBox::information(this, "Import Set Parts List",
                             QString("Parts list imported for %1 (%2).\n\nCSV rows read: %3\nDistinct composition rows: %4")
                                 .arg(m_setName, m_setNumber).arg(result.rowsRead).arg(result.compositionRows));
}

void SetDetailsDialog::createBuildFromStock()
{
    if (m_requiredPieces <= 0)
        return;
    QDialog dialog(this);
    dialog.setWindowTitle("Create Build From Stock");
    auto* layout = new QFormLayout(&dialog);
    layout->addRow("Set #:", new QLabel(m_setNumber, &dialog));
    layout->addRow("Set:", new QLabel(m_setName, &dialog));
    layout->addRow("Required pieces:", new QLabel(QString::number(m_requiredPieces), &dialog));
    layout->addRow("Spare pieces excluded:", new QLabel(QString::number(m_sparePieces), &dialog));
    auto* nameEdit = new QLineEdit(m_setName, &dialog);
    layout->addRow("Build name:", nameEdit);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    QPushButton* create = buttons->addButton("Create", QDialogButtonBox::AcceptRole);
    connect(nameEdit, &QLineEdit::textChanged, create,
            [create](const QString& text) { create->setEnabled(!text.trimmed().isEmpty()); });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttons);
    nameEdit->selectAll();
    if (dialog.exec() == QDialog::Accepted)
        emit createBuildRequested(m_setCatalogId, nameEdit->text().trimmed());
}

void SetDetailsDialog::loadCachedImage()
{
    if (m_setNumber.isEmpty())
        return;

    const QString cachedPath = m_imageService->cachedImagePath(m_setNumber);

    if (cachedPath.isEmpty())
        return;

    QPixmap pixmap(cachedPath);

    if (pixmap.isNull())
        return;

    m_imageLabel->setPixmap(
        pixmap.scaled(m_imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    m_imageLabel->setText(QString());

    m_imageStatusLabel->setText("Cached");
}

void SetDetailsDialog::requestImage()
{
    if (m_setNumber.isEmpty())
        return;

    if (m_imageUrl.isEmpty()) {
        m_imageStatusLabel->setText("No image URL is available.");

        return;
    }

    const QString cachedPath = m_imageService->cachedImagePath(m_setNumber);

    if (!cachedPath.isEmpty())
        return;

    m_imageStatusLabel->setText("Loading...");

    m_imageService->requestSetImage(m_setNumber, m_imageUrl);
}

void SetDetailsDialog::requestProviderEnrichment()
{
    if (!m_providerService || m_setNumber.isEmpty())
        return;

    m_providerSourceLabel->setText("Checking...");
    m_providerStatusLabel->setText("Looking for provider enrichment.");

    m_providerService->requestDetails(m_setNumber);
}

void SetDetailsDialog::requestBricksetInstructions()
{
    if (!m_bricksetService || m_setNumber.isEmpty())
        return;

    const QString apiKey = UserSettings::instance().bricksetApiKey().trimmed();

    if (apiKey.isEmpty()) {
        m_viewInstructionsButton->setVisible(false);
        return;
    }

    m_viewInstructionsButton->setEnabled(false);
    m_viewInstructionsButton->setText("Loading...");

    m_bricksetService->getInstructions2(m_setNumber, apiKey);
}

void SetDetailsDialog::setBricksetRowsVisible(bool visible)
{
    if (!m_providerLayout)
        return;

    m_providerLayout->setRowVisible(m_bricksetIdLabel, visible);
    m_providerLayout->setRowVisible(m_providerThemeLabel, visible);
    m_providerLayout->setRowVisible(m_providerSubthemeLabel, visible);
    m_providerLayout->setRowVisible(m_minifigsLabel, visible);
    m_providerLayout->setRowVisible(m_availabilityLabel, visible);
    m_providerLayout->setRowVisible(m_ratingLabel, visible);
    m_providerLayout->setRowVisible(m_instructionsRowWidget, visible);
    m_providerLayout->setRowVisible(m_additionalImagesLabel, visible);

    if (!visible && m_viewInstructionsButton)
        m_viewInstructionsButton->setVisible(false);
    m_providerLayout->setRowVisible(m_providerLinkLabel, visible);
}
