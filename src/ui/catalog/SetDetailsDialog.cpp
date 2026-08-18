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
#include "../../repositories/SetCatalogRepository.h"
#include "../../services/images/SetImageService.h"
#include "../../services/sets/SetDetailsProviderService.h"

#include <QDebug>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>

SetDetailsDialog::SetDetailsDialog(int setCatalogId, QWidget* parent)
    : QDialog(parent)
    , m_setCatalogId(setCatalogId)
{
    setWindowTitle("Set Details");

    resize(760, 640);

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
    m_instructionsLabel = new QLabel("-", providerGroup);
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
    m_providerLayout->addRow("Instructions:", m_instructionsLabel);
    m_providerLayout->addRow("Additional Images:", m_additionalImagesLabel);
    m_providerLayout->addRow("Provider Link:", m_providerLinkLabel);

    // Until provider selection completes, show only the generic Source and
    // Status rows. Brickset-specific enrichment fields become visible only
    // when Brickset actually supplies them.
    setBricksetRowsVisible(false);

    mainLayout->addWidget(providerGroup);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);

    m_imageService = new SetImageService(this);
    m_providerService = new SetDetailsProviderService(this);

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

                    m_providerStatusLabel->setText(
                        result.usedFallback
                            ? "Brickset enrichment was unavailable; Rebrickable details are being used."
                            : "Rebrickable details loaded.");

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

    m_imageUrl = set->imageUrl();

    m_setNumberLabel->setText(set->setNumber());

    m_nameLabel->setText(set->name());

    m_yearLabel->setText(QString::number(set->year()));

    m_themeLabel->setText(QString::number(set->themeId()));

    m_partsLabel->setText(QString::number(set->numberOfParts()));

    setWindowTitle(QString("Set Details — %1").arg(set->setNumber()));

    return true;
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
    m_providerLayout->setRowVisible(m_instructionsLabel, visible);
    m_providerLayout->setRowVisible(m_additionalImagesLabel, visible);
    m_providerLayout->setRowVisible(m_providerLinkLabel, visible);
}

