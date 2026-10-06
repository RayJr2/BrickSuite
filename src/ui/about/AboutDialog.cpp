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
#include "../common/ThemeRichTextLabel.h"
#include "../common/SupportLinks.h"

#include "../../core/AppConstants.h"
#include "../../core/AppVersion.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

namespace
{

QLabel* createWrappedLabel(const QString& text, QWidget* parent)
{
    auto* label = new ThemeRichTextLabel(text, parent);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    label->setOpenExternalLinks(true);

    return label;
}

} // namespace

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("About %1").arg(AppConstants::name()));

    setWindowIcon(QApplication::windowIcon());
    setModal(true);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 20);
    mainLayout->setSpacing(14);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(18);

    auto* iconLabel = new QLabel(this);
    iconLabel->setFixedSize(112, 112);
    iconLabel->setAlignment(Qt::AlignCenter);

    // Request the About artwork through QIcon at the size actually needed.
    // Loading an .ico directly into QPixmap can select a small embedded
    // representation and then scale it up, producing a pixelated image.
    // QIcon selects the best available resolution from the multi-size icon.
    const qreal devicePixelRatio = iconLabel->devicePixelRatioF();

    const QSize iconPixelSize(
        static_cast<int>(iconLabel->width() * devicePixelRatio),
        static_cast<int>(iconLabel->height() * devicePixelRatio));

    QPixmap iconPixmap =
        QApplication::windowIcon().pixmap(iconPixelSize);

    if (!iconPixmap.isNull()) {
        iconPixmap.setDevicePixelRatio(devicePixelRatio);
        iconLabel->setPixmap(iconPixmap);
    }

    headerLayout->addWidget(iconLabel, 0, Qt::AlignTop);

    auto* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);

    auto* titleLabel = new QLabel(AppConstants::name(), this);

    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 7);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto* taglineLabel = new QLabel(QStringLiteral(
                                        "The Digital Twin Platform for Your Brick Workshop."),
                                    this);

    QFont taglineFont = taglineLabel->font();
    taglineFont.setItalic(true);
    taglineLabel->setFont(taglineFont);
    taglineLabel->setWordWrap(true);

    auto* versionLabel = new QLabel(QStringLiteral("Version %1").arg(AppVersion::version()), this);

    QFont versionFont = versionLabel->font();
    versionFont.setBold(true);
    versionLabel->setFont(versionFont);

    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(taglineLabel);
    titleLayout->addSpacing(8);
    titleLayout->addWidget(versionLabel);

    titleLayout->addWidget(
        new QLabel(QStringLiteral("Copyright © %1 %2")
                       .arg(AppConstants::copyrightYear(), AppConstants::company()),
                   this));

    titleLayout->addStretch(1);

    for (int i = 0; i < titleLayout->count(); ++i) {
        if (auto* widget = titleLayout->itemAt(i)->widget())
            widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    }

    headerLayout->addLayout(titleLayout, 1);

    mainLayout->addLayout(headerLayout);

    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);

    mainLayout->addWidget(separator);

    // Keep the header and Close action visible when the wrapped description
    // exceeds the screen. QScrollArea uses the content's height-for-width
    // minimum, so paragraphs scroll instead of being compressed vertically.
    m_textArea = new QScrollArea(this);
    m_textArea->setFrameShape(QFrame::NoFrame);
    m_textArea->setWidgetResizable(true);
    m_textArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* textContent = new QWidget(m_textArea);
    auto* textLayout = new QVBoxLayout(textContent);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(mainLayout->spacing());
    textLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    textLayout->setAlignment(Qt::AlignTop);
    m_textArea->setWidget(textContent);
    mainLayout->addWidget(m_textArea, 1);

    textLayout->addWidget(
        createWrappedLabel(QStringLiteral(
                               "%1 is an open-source desktop application for managing "
                               "a LEGO workshop: inventory, storage, collections, catalogs, "
                               "and Builds. Paired Host / Remote access connects shared workflows; "
                               "LDraw viewing, supported print preparation, and LEGO Fit Calibration "
                               "extend the workshop into 3D printing.")
                               .arg(AppConstants::name()),
                           this));

    textLayout->addWidget(
        createWrappedLabel(QStringLiteral(
                               "%1 integrates with Rebrickable for supported catalog and inventory "
                               "data workflows, and with Brickset for supported Set information "
                               "and instructions.")
                               .arg(AppConstants::name()),
                           this));

    auto* supportLayout = new QVBoxLayout();
    supportLayout->setSpacing(4);
    supportLayout->addWidget(createWrappedLabel(tr(
        "<b>Support BrickSuite</b><br>"
        "BrickSuite is free and open-source software. Voluntary contributions help "
        "with development, testing, documentation, and maintenance."), this));
    auto* support = createWrappedLabel(tr(
        "<a href=\"%1\">Support BrickSuite with PayPal</a>")
        .arg(QString::fromLatin1(AppConstants::SupportUrl)), this);
    support->setObjectName(QStringLiteral("supportBrickSuiteLink"));
    support->setAccessibleName(tr("Support BrickSuite with PayPal"));
    support->setFocusPolicy(Qt::StrongFocus);
    support->setOpenExternalLinks(false);
    connect(support, &QLabel::linkActivated, this,
            [this](const QString&) { SupportLinks::openSupportPage(this); });
    supportLayout->addWidget(support);
    textLayout->addLayout(supportLayout);

    textLayout->addWidget(
        createWrappedLabel(QStringLiteral(
                               "<b>License:</b> GNU Lesser General Public License, version 3.0 "
                               "(LGPL-3.0-only)<br>"
                               "<a href=\"https://www.gnu.org/licenses/lgpl-3.0.html\">"
                               "View the GNU LGPL v3.0 license</a>"),
                           this));

    textLayout->addWidget(
        createWrappedLabel(QStringLiteral("<b>%1:</b> "
                                          "<a href=\"https://%2\">https://%2</a>")
                               .arg(AppConstants::company(), AppConstants::domain()),
                           this));

    auto* trademarkLabel = createWrappedLabel(
        QStringLiteral("<b>Trademarks:</b> LEGO® is a trademark of the LEGO Group "
                       "of companies, which does not sponsor, authorize, or endorse "
                       "%1. Rebrickable and Brickset are trademarks or brand names of their "
                       "respective owners. %1 is an independent application and is not "
                       "affiliated with or endorsed by the LEGO Group, Rebrickable, or Brickset.")
            .arg(AppConstants::name()),
        this);

    textLayout->addWidget(trademarkLabel);
    textLayout->addWidget(createWrappedLabel(tr(
        "Software dependency notices are provided in the installed licenses directory "
        "and THIRD_PARTY_NOTICES.md (Contents/Resources/Licenses on macOS)."), this));

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);

    // Resolve the content-derived size before the native window is created.
    // Relying on the implicit first-show adjustment can leave the parented
    // Linux dialog at the layout minimum after native configure events.
    adjustSize();
}

QSize AboutDialog::sizeHint() const
{
    const QSize base = QDialog::sizeHint();
    if (!m_textArea)
        return base;
    // Qt asks for this after style/font polishing, before showing the window.
    // Leave room for window decorations; do not impose a fixed platform height
    // or a maximum size that would prevent the user from resizing later.
    const QSize available = screen()->availableGeometry().size() * 0.9;
    const int preferredWidth = fontMetrics().averageCharWidth() * 100;
    const int initialWidth = qMin(available.width(),
                                 qMax(minimumSizeHint().width(), preferredWidth));
    const auto margins = layout()->contentsMargins();
    const int textWidth = initialWidth - margins.left() - margins.right()
        - 2 * m_textArea->frameWidth();
    const int textHeight = m_textArea->widget()->heightForWidth(textWidth);
    const int chromeHeight = base.height() - m_textArea->sizeHint().height();
    return QSize(initialWidth, qMin(available.height(), chromeHeight + textHeight));
}
