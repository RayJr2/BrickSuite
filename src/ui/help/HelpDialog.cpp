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

#include "HelpDialog.h"

#include "HelpManager.h"

#include "../../settings/UserSettings.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

namespace
{

constexpr int TopicRole = Qt::UserRole;

QTreeWidgetItem* addTopicItem(QTreeWidgetItem* parent,
                              HelpTopic topic,
                              const QString& title)
{
    auto* item = new QTreeWidgetItem(parent);
    item->setText(0, title);
    item->setData(0, TopicRole, static_cast<int>(topic));

    return item;
}

QTreeWidgetItem* addTopicItem(QTreeWidget* tree,
                              HelpTopic topic,
                              const QString& title)
{
    auto* item = new QTreeWidgetItem(tree);
    item->setText(0, title);
    item->setData(0, TopicRole, static_cast<int>(topic));

    return item;
}

} // namespace

HelpDialog::HelpDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("BrickSuite Help");
    setWindowIcon(QApplication::windowIcon());
    setWindowModality(Qt::NonModal);
    setModal(false);
    resize(1050, 720);

    const QByteArray savedGeometry = UserSettings::instance().helpViewerGeometry();

    if (!savedGeometry.isEmpty())
        restoreGeometry(savedGeometry);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    auto* navigationLayout = new QHBoxLayout();

    m_backButton = new QPushButton("Back", this);
    m_forwardButton = new QPushButton("Forward", this);
    m_homeButton = new QPushButton("Home", this);

    navigationLayout->addWidget(m_backButton);
    navigationLayout->addWidget(m_forwardButton);
    navigationLayout->addWidget(m_homeButton);
    navigationLayout->addStretch(1);

    mainLayout->addLayout(navigationLayout);

    m_splitter = new QSplitter(Qt::Horizontal, this);

    auto* leftPane = new QWidget(m_splitter);
    auto* leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);

    auto* searchLabel = new QLabel("Search Help", leftPane);
    m_searchEdit = new QLineEdit(leftPane);
    m_searchEdit->setPlaceholderText("Search topics and help text...");

    m_contentsTree = new QTreeWidget(leftPane);
    m_contentsTree->setHeaderLabel("Contents");
    m_contentsTree->setRootIsDecorated(true);
    m_contentsTree->setUniformRowHeights(true);

    leftLayout->addWidget(searchLabel);
    leftLayout->addWidget(m_searchEdit);
    leftLayout->addWidget(m_contentsTree, 1);

    m_browser = new QTextBrowser(m_splitter);
    m_browser->setOpenExternalLinks(true);
    m_browser->setOpenLinks(true);

    m_splitter->addWidget(leftPane);
    m_splitter->addWidget(m_browser);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({270, 780});

    const QByteArray splitterState = UserSettings::instance().helpViewerSplitterState();

    if (!splitterState.isEmpty())
        m_splitter->restoreState(splitterState);

    mainLayout->addWidget(m_splitter, 1);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(this, &QDialog::finished, this, [this](int) {
        UserSettings::instance().setHelpViewerGeometry(saveGeometry());
        UserSettings::instance().setHelpViewerSplitterState(m_splitter->saveState());
    });

    connect(m_backButton, &QPushButton::clicked, m_browser, &QTextBrowser::backward);
    connect(m_forwardButton, &QPushButton::clicked, m_browser, &QTextBrowser::forward);

    connect(m_homeButton, &QPushButton::clicked, this, [this]() {
        showTopic(HelpTopic::Home);
    });

    connect(m_browser, &QTextBrowser::backwardAvailable, this, [this](bool) {
        updateNavigationButtons();
    });

    connect(m_browser, &QTextBrowser::forwardAvailable, this, [this](bool) {
        updateNavigationButtons();
    });

    connect(m_browser, &QTextBrowser::sourceChanged, this, [this](const QUrl& source) {
        syncContentsSelection(source);
        updateNavigationButtons();
    });

    connect(m_contentsTree,
            &QTreeWidget::itemActivated,
            this,
            [this](QTreeWidgetItem* item, int) {
                if (!item)
                    return;

                const QVariant data = item->data(0, TopicRole);

                if (!data.isValid())
                    return;

                loadTopic(static_cast<HelpTopic>(data.toInt()));
            });

    connect(m_contentsTree,
            &QTreeWidget::itemClicked,
            this,
            [this](QTreeWidgetItem* item, int) {
                if (!item)
                    return;

                const QVariant data = item->data(0, TopicRole);

                if (!data.isValid())
                    return;

                loadTopic(static_cast<HelpTopic>(data.toInt()));
            });

    connect(m_searchEdit, &QLineEdit::textChanged, this, &HelpDialog::applySearch);

    buildContents();
    showTopic(HelpTopic::Home);
    updateNavigationButtons();
}

void HelpDialog::showTopic(HelpTopic topic)
{
    if (!m_searchEdit->text().isEmpty())
        m_searchEdit->clear();

    loadTopic(topic);
}

void HelpDialog::closeEvent(QCloseEvent* event)
{
    UserSettings::instance().setHelpViewerGeometry(saveGeometry());
    UserSettings::instance().setHelpViewerSplitterState(m_splitter->saveState());

    QDialog::closeEvent(event);
}

void HelpDialog::buildContents()
{
    m_contentsTree->clear();
    m_contentsTree->setHeaderLabel("Contents");

    addTopicItem(m_contentsTree, HelpTopic::Home, "Help Home");

    auto* gettingStarted = new QTreeWidgetItem(m_contentsTree);
    gettingStarted->setText(0, "Getting Started");
    gettingStarted->setData(0, TopicRole, static_cast<int>(HelpTopic::GettingStarted));

    addTopicItem(m_contentsTree, HelpTopic::QuickStart, "Quick Start");

    auto* inventoryGroup = new QTreeWidgetItem(m_contentsTree);
    inventoryGroup->setText(0, "Inventory & Storage");

    addTopicItem(inventoryGroup, HelpTopic::Storage, "Storage");
    addTopicItem(inventoryGroup, HelpTopic::PartsCatalog, "Parts Catalog");
    addTopicItem(inventoryGroup, HelpTopic::SetsCatalog, "Sets Catalog");
    addTopicItem(inventoryGroup, HelpTopic::MinifigsCatalog, "Minifigs Catalog");
    addTopicItem(inventoryGroup, HelpTopic::Inventory, "My Inventory");
    addTopicItem(inventoryGroup, HelpTopic::RebrickableImport, "Rebrickable Import");

    auto* buildsGroup = new QTreeWidgetItem(m_contentsTree);
    buildsGroup->setText(0, "Builds & MOCs");

    addTopicItem(buildsGroup, HelpTopic::Builds, "Builds");
    addTopicItem(buildsGroup, HelpTopic::Mocs, "MOCs");
    addTopicItem(buildsGroup, HelpTopic::AllocateAvailable, "Allocate Available");
    addTopicItem(buildsGroup, HelpTopic::MissingParts, "Missing Parts");
    addTopicItem(buildsGroup, HelpTopic::LostFound, "Lost / Found");

    auto* maintenanceGroup = new QTreeWidgetItem(m_contentsTree);
    maintenanceGroup->setText(0, "Maintenance & Support");

    addTopicItem(maintenanceGroup, HelpTopic::BackupRestore, "Backup / Restore");
    addTopicItem(maintenanceGroup, HelpTopic::Settings, "Settings");
    addTopicItem(maintenanceGroup, HelpTopic::Logging, "Application Log");
    addTopicItem(maintenanceGroup, HelpTopic::Troubleshooting, "Troubleshooting");

    m_contentsTree->expandAll();
}

void HelpDialog::applySearch(const QString& searchText)
{
    const QString needle = searchText.trimmed();

    if (needle.isEmpty()) {
        buildContents();
        syncContentsSelection(m_browser->source());
        return;
    }

    m_contentsTree->clear();
    m_contentsTree->setHeaderLabel("Search Results");

    for (const HelpTopicInfo& info : HelpManager::topics()) {
        const QString haystack
            = info.title + "\n" + searchableText(info.topic);

        if (!haystack.contains(needle, Qt::CaseInsensitive))
            continue;

        addTopicItem(m_contentsTree, info.topic, info.title);
    }

    if (m_contentsTree->topLevelItemCount() == 0) {
        auto* item = new QTreeWidgetItem(m_contentsTree);
        item->setText(0, "No help topics found.");
        item->setDisabled(true);
    }
}

void HelpDialog::loadTopic(HelpTopic topic)
{
    const QString resourcePath = HelpManager::resourcePath(topic);

    if (!QFile::exists(resourcePath)) {
        m_browser->setHtml(
            QString("<h1>%1</h1><p>This Help topic is not available yet.</p>")
                .arg(HelpManager::title(topic)));
        return;
    }

    QString qrcPath = resourcePath;

    if (qrcPath.startsWith(":/"))
        qrcPath.replace(0, 2, "qrc:/");

    m_browser->setSource(QUrl(qrcPath));
}

void HelpDialog::updateNavigationButtons()
{
    m_backButton->setEnabled(m_browser->isBackwardAvailable());
    m_forwardButton->setEnabled(m_browser->isForwardAvailable());
}

void HelpDialog::syncContentsSelection(const QUrl& source)
{
    if (m_syncingSelection || !m_searchEdit->text().isEmpty())
        return;

    QString sourcePath = source.toString();

    if (sourcePath.startsWith("qrc:/"))
        sourcePath.replace(0, 5, ":/");

    for (const HelpTopicInfo& info : HelpManager::topics()) {
        if (sourcePath != info.resourcePath)
            continue;

        const QList<QTreeWidgetItem*> items
            = m_contentsTree->findItems(info.title,
                                        Qt::MatchExactly | Qt::MatchRecursive,
                                        0);

        if (items.isEmpty())
            return;

        m_syncingSelection = true;
        m_contentsTree->setCurrentItem(items.first());
        m_syncingSelection = false;
        return;
    }
}

QString HelpDialog::searchableText(HelpTopic topic) const
{
    QFile file(HelpManager::resourcePath(topic));

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QTextDocument document;
    document.setHtml(QString::fromUtf8(file.readAll()));

    return document.toPlainText();
}
