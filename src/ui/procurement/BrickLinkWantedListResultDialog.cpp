/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "BrickLinkWantedListResultDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

BrickLinkWantedListResultDialog::BrickLinkWantedListResultDialog(
    const QString& xml,
    int itemRows,
    int totalPieces,
    const QString& buildNumber,
    const QString& buildName,
    QWidget* parent)
    : QDialog(parent)
    , m_xml(xml)
    , m_buildNumber(buildNumber)
    , m_buildName(buildName)
{
    setWindowTitle(QStringLiteral("BrickLink Wanted List XML"));
    resize(900, 680);

    auto* mainLayout = new QVBoxLayout(this);

    auto* summaryLabel = new QLabel(
        QStringLiteral("Items: %1    Pieces: %2")
            .arg(itemRows)
            .arg(totalPieces),
        this);
    mainLayout->addWidget(summaryLabel);

    auto* helpLabel = new QLabel(
        QStringLiteral("The XML below is ready for BrickLink's "
                       "\"Upload BrickLink XML format\" workflow. "
                       "Copy it, open the BrickLink upload page, and paste it there."),
        this);
    helpLabel->setWordWrap(true);
    mainLayout->addWidget(helpLabel);

    m_xmlEdit = new QPlainTextEdit(this);
    m_xmlEdit->setReadOnly(true);
    m_xmlEdit->setPlainText(m_xml);
    m_xmlEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    mainLayout->addWidget(m_xmlEdit, 1);

    m_statusLabel = new QLabel(this);
    mainLayout->addWidget(m_statusLabel);

    auto* actionLayout = new QHBoxLayout();

    auto* copyButton = new QPushButton(QStringLiteral("Copy XML"), this);
    auto* saveButton = new QPushButton(QStringLiteral("Save XML..."), this);
    auto* openButton =
        new QPushButton(QStringLiteral("Open BrickLink Upload"), this);

    actionLayout->addWidget(copyButton);
    actionLayout->addWidget(saveButton);
    actionLayout->addWidget(openButton);
    actionLayout->addStretch(1);

    mainLayout->addLayout(actionLayout);

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons,
            &QDialogButtonBox::rejected,
            this,
            &QDialog::reject);
    mainLayout->addWidget(buttons);

    connect(copyButton,
            &QPushButton::clicked,
            this,
            &BrickLinkWantedListResultDialog::copyXml);

    connect(saveButton,
            &QPushButton::clicked,
            this,
            &BrickLinkWantedListResultDialog::saveXml);

    connect(openButton,
            &QPushButton::clicked,
            this,
            &BrickLinkWantedListResultDialog::openBrickLinkUpload);
}

QString BrickLinkWantedListResultDialog::defaultFileName() const
{
    QString identity = m_buildNumber.trimmed();

    if (identity.isEmpty())
        identity = m_buildName.trimmed();

    if (identity.isEmpty())
        identity = QStringLiteral("MissingParts");

    identity.replace(QRegularExpression(R"([^A-Za-z0-9_-]+)"),
                     QStringLiteral("_"));

    return QStringLiteral("BrickSuite_BrickLink_%1.xml").arg(identity);
}

void BrickLinkWantedListResultDialog::copyXml()
{
    QApplication::clipboard()->setText(m_xml);

    m_statusLabel->setText(
        QStringLiteral("BrickLink XML copied to clipboard."));
}

void BrickLinkWantedListResultDialog::saveXml()
{
    const QString fileName =
        QFileDialog::getSaveFileName(
            this,
            QStringLiteral("Save BrickLink Wanted List XML"),
            defaultFileName(),
            QStringLiteral("XML Files (*.xml)"));

    if (fileName.isEmpty())
        return;

    QString finalFileName = fileName;

    if (!finalFileName.endsWith(QStringLiteral(".xml"),
                                Qt::CaseInsensitive)) {
        finalFileName += QStringLiteral(".xml");
    }

    QFile file(finalFileName);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_statusLabel->setText(
            QStringLiteral("Unable to save BrickLink XML."));
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << m_xml;
    file.close();

    m_statusLabel->setText(
        QStringLiteral("BrickLink XML saved: %1").arg(finalFileName));
}

void BrickLinkWantedListResultDialog::openBrickLinkUpload()
{
    const QUrl uploadUrl(
        QStringLiteral("https://www.bricklink.com/v2/wanted/upload.page"));

    if (QDesktopServices::openUrl(uploadUrl)) {
        m_statusLabel->setText(
            QStringLiteral("BrickLink Wanted List upload page opened."));
    } else {
        m_statusLabel->setText(
            QStringLiteral("Unable to open the BrickLink upload page."));
    }
}
