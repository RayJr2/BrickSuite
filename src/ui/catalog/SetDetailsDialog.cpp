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

#include <QDebug>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>

SetDetailsDialog::SetDetailsDialog(int setCatalogId, QWidget* parent)
    : QDialog(parent)
    , m_setCatalogId(setCatalogId)
{
    setWindowTitle("Set Details");

    resize(700, 520);

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

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);

    m_imageService = new SetImageService(this);

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