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

#include "AboutDialog.h"

#include "../../core/AppVersion.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace
{

QLabel* createWrappedLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    label->setOpenExternalLinks(true);

    return label;
}

} // namespace

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("About BrickSuite");
    setWindowIcon(QApplication::windowIcon());
    setModal(true);
    setMinimumWidth(560);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 20);
    mainLayout->setSpacing(14);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(18);

    auto* iconLabel = new QLabel(this);
    iconLabel->setFixedSize(112, 112);
    iconLabel->setAlignment(Qt::AlignCenter);

    QPixmap iconPixmap(QStringLiteral(":/icons/bricksuite.ico"));

    if (!iconPixmap.isNull()) {
        iconLabel->setPixmap(
            iconPixmap.scaled(iconLabel->size(),
                              Qt::KeepAspectRatio,
                              Qt::SmoothTransformation));
    }

    headerLayout->addWidget(iconLabel, 0, Qt::AlignTop);

    auto* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);

    auto* titleLabel = new QLabel(QStringLiteral("BrickSuite"), this);

    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 7);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto* taglineLabel
        = new QLabel(QStringLiteral("The Digital Twin Platform for Your Brick Workshop."), this);

    QFont taglineFont = taglineLabel->font();
    taglineFont.setItalic(true);
    taglineLabel->setFont(taglineFont);
    taglineLabel->setWordWrap(true);

    auto* versionLabel
        = new QLabel(QStringLiteral("Version %1").arg(AppVersion::version()), this);

    QFont versionFont = versionLabel->font();
    versionFont.setBold(true);
    versionLabel->setFont(versionFont);

    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(taglineLabel);
    titleLayout->addSpacing(8);
    titleLayout->addWidget(versionLabel);
    titleLayout->addWidget(
        new QLabel(QStringLiteral("Copyright © 2026 RF StateSide, LLC"), this));
    titleLayout->addStretch(1);

    headerLayout->addLayout(titleLayout, 1);

    mainLayout->addLayout(headerLayout);

    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);

    mainLayout->addWidget(separator);

    mainLayout->addWidget(
        createWrappedLabel(
            QStringLiteral(
                "BrickSuite is an open-source desktop application for managing "
                "brick inventory, storage locations, Sets, MOCs, Builds, missing "
                "parts, Lost/Found inventory, and Rebrickable reference data."),
            this));

    mainLayout->addWidget(
        createWrappedLabel(
            QStringLiteral(
                "<b>License:</b> GNU Lesser General Public License, version 3.0 "
                "(LGPL-3.0-only)<br>"
                "<a href=\"https://www.gnu.org/licenses/lgpl-3.0.html\">"
                "View the GNU LGPL v3.0 license</a>"),
            this));

    mainLayout->addWidget(
        createWrappedLabel(
            QStringLiteral(
                "<b>RF StateSide, LLC:</b> "
                "<a href=\"https://rfstateside.com\">https://rfstateside.com</a>"),
            this));

    auto* trademarkLabel
        = createWrappedLabel(
            QStringLiteral(
                "<b>Third-Party Notice:</b> LEGO® is a trademark of the LEGO Group "
                "of companies, which does not sponsor, authorize, or endorse "
                "BrickSuite. Rebrickable is a trademark or brand name of its "
                "respective owner. BrickSuite is an independent application and "
                "is not affiliated with or endorsed by Rebrickable."),
            this);

    mainLayout->addWidget(trademarkLabel);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);
}
