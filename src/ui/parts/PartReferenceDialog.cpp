/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 */

#include "PartReferenceDialog.h"

#include "../../services/RebrickableApiClient.h"
#include "../../services/images/PartImageService.h"
#include "../../settings/UserSettings.h"

#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>

namespace {

QString normalizedKey(const QString& value)
{
    return value.trimmed().toLower();
}

QString viewModeName(bool dimensionGrid)
{
    return dimensionGrid ? QStringLiteral("DimensionGrid") : QStringLiteral("Gallery");
}

} // namespace

PartReferenceDialog::PartReferenceDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Part Reference"));
    setWindowFlag(Qt::Window, true);
    setWindowModality(Qt::NonModal);
    setModal(false);
    resize(1180, 800);

    m_partImageService = new PartImageService(this);
    m_rebrickableApiClient = new RebrickableApiClient(this);

    QString manifestError;
    if (!m_manifest.load(&manifestError)) {
        QMessageBox::warning(this,
                             tr("Part Reference"),
                             tr("The Part Reference manifest could not be loaded.\n\n%1")
                                 .arg(manifestError));
    }

    initializeDimensionDefinitions();
    initializeUi();
    restoreUiState();

    connect(m_partImageService,
            &PartImageService::imageReady,
            this,
            &PartReferenceDialog::setCardImage);

    connect(m_rebrickableApiClient,
            &RebrickableApiClient::partImageUrlsFinished,
            this,
            [this](const RebrickableApiClient::PartImageUrlsResult& result) {
                if (!result.success)
                    return;

                for (const auto& part : result.parts) {
                    if (part.partImageUrl.trimmed().isEmpty())
                        continue;

                    m_partImageService->requestPartImage(part.partNumber,
                                                         part.partImageUrl);
                }
            });

    showCurrentCatalogPage();
}

PartReferenceDialog::~PartReferenceDialog()
{
    saveUiState();
}

void PartReferenceDialog::setAddInventoryAvailable(bool available)
{
    m_addInventoryAvailable = available;

    if (m_sendButton)
        m_sendButton->setEnabled(available && !m_selectedPartNumber.isEmpty());
}

void PartReferenceDialog::closeEvent(QCloseEvent* event)
{
    saveUiState();
    QDialog::closeEvent(event);
}

void PartReferenceDialog::initializeUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* searchRow = new QHBoxLayout();
    auto* searchLabel = new QLabel(tr("Find:"), this);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Part number, name, catalog, or section"));
    searchRow->addWidget(searchLabel);
    searchRow->addWidget(m_searchEdit, 1);
    mainLayout->addLayout(searchRow);

    m_splitter = new QSplitter(Qt::Horizontal, this);

    auto* navigationPanel = new QWidget(m_splitter);
    auto* navigationLayout = new QVBoxLayout(navigationPanel);
    navigationLayout->setContentsMargins(0, 0, 0, 0);

    auto* catalogsLabel = new QLabel(tr("Catalogs"), navigationPanel);
    QFont catalogsFont = catalogsLabel->font();
    catalogsFont.setBold(true);
    catalogsLabel->setFont(catalogsFont);
    navigationLayout->addWidget(catalogsLabel);

    m_catalogList = new QListWidget(navigationPanel);
    m_catalogList->setMinimumWidth(205);
    m_catalogList->setMaximumWidth(330);
    navigationLayout->addWidget(m_catalogList, 1);

    auto* contentPanel = new QWidget(m_splitter);
    auto* contentLayout = new QVBoxLayout(contentPanel);
    contentLayout->setContentsMargins(8, 0, 0, 0);

    auto* headingRow = new QHBoxLayout();
    m_catalogTitleLabel = new QLabel(contentPanel);
    QFont titleFont = m_catalogTitleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    m_catalogTitleLabel->setFont(titleFont);

    m_catalogCountLabel = new QLabel(contentPanel);
    m_catalogCountLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* viewLabel = new QLabel(tr("View:"), contentPanel);
    m_viewCombo = new QComboBox(contentPanel);

    headingRow->addWidget(m_catalogTitleLabel, 1);
    headingRow->addWidget(m_catalogCountLabel);
    headingRow->addSpacing(12);
    headingRow->addWidget(viewLabel);
    headingRow->addWidget(m_viewCombo);
    contentLayout->addLayout(headingRow);

    m_contentStack = new QStackedWidget(contentPanel);
    contentLayout->addWidget(m_contentStack, 1);

    m_splitter->addWidget(navigationPanel);
    m_splitter->addWidget(contentPanel);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(m_splitter, 1);

    auto* selectedRow = new QHBoxLayout();
    m_selectedLabel = new QLabel(tr("Selected: None"), this);
    m_selectedLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_copyButton = new QPushButton(tr("Copy Part #"), this);
    m_sendButton = new QPushButton(tr("Send to Add Inventory"), this);
    m_copyButton->setEnabled(false);
    m_sendButton->setEnabled(false);

    selectedRow->addWidget(m_selectedLabel, 1);
    selectedRow->addWidget(m_copyButton);
    selectedRow->addWidget(m_sendButton);
    mainLayout->addLayout(selectedRow);

    populateCatalogList();

    connect(m_catalogList, &QListWidget::currentRowChanged, this, [this](int) {
        catalogSelectionChanged();
    });

    connect(m_viewCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_restoringUiState)
            return;

        const QString catalog = currentCatalog();
        if (!catalog.isEmpty()) {
            UserSettings::instance().setPartReferenceViewMode(
                catalog,
                viewModeName(currentViewMode() == ViewMode::DimensionGrid));
        }
        showCurrentCatalogPage();
    });

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString&) {
        refreshSearchResults();
    });

    connect(m_copyButton, &QPushButton::clicked, this, [this]() {
        if (m_selectedPartNumber.isEmpty())
            return;

        if (QClipboard* clipboard = QGuiApplication::clipboard())
            clipboard->setText(m_selectedPartNumber);
    });

    connect(m_sendButton, &QPushButton::clicked, this, [this]() {
        if (m_selectedPartNumber.isEmpty() || !m_addInventoryAvailable)
            return;

        emit sendToAddInventoryRequested(m_selectedPartNumber);
    });
}

QList<PartReferenceDialog::DimensionEntry> PartReferenceDialog::makeDimensionEntries(
    const QStringList& columnLabels,
    const QList<QPair<QString, QStringList>>& rows)
{
    QList<DimensionEntry> entries;

    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const QString rowLabel = rows.at(rowIndex).first;
        const QStringList values = rows.at(rowIndex).second;

        for (int columnIndex = 0;
             columnIndex < values.size() && columnIndex < columnLabels.size();
             ++columnIndex) {
            const QString partNumber = values.at(columnIndex).trimmed();
            if (partNumber.isEmpty())
                continue;

            DimensionEntry entry;
            entry.row = rowIndex;
            entry.column = columnIndex;
            entry.rowLabel = rowLabel;
            entry.columnLabel = columnLabels.at(columnIndex);
            entry.partNumber = partNumber;
            entries.append(entry);
        }
    }

    return entries;
}

void PartReferenceDialog::initializeDimensionDefinitions()
{
    const QStringList studLengths = {
        "1", "2", "3", "4", "5", "6", "8", "10", "11", "12", "14", "16", "18", "24"
    };

    DimensionDefinition bricks;
    bricks.catalog = QStringLiteral("Bricks");
    bricks.section = QStringLiteral("Standard");
    bricks.title = tr("Standard Bricks — Width × Length");
    bricks.entries = makeDimensionEntries(
        studLengths,
        {
            {"1", {"3005", "3004", "3622", "3010", "", "3009", "3008", "6111", "", "6112", "", "2465", "", ""}},
            {"2", {"", "3003", "3002", "3001", "", "2456", "3007", "3006", "", "", "", "", "", ""}},
            {"4", {"", "", "", "", "", "2356", "", "6212", "", "4202", "", "", "30400", ""}},
            {"8", {"", "", "", "", "", "", "4201", "", "", "", "", "4204", "", ""}}
        });
    m_dimensionDefinitions.append(bricks);

    DimensionDefinition plates;
    plates.catalog = QStringLiteral("Plates");
    plates.section = QStringLiteral("Standard");
    plates.title = tr("Standard Plates — Width × Length");
    plates.entries = makeDimensionEntries(
        studLengths,
        {
            {"1", {"3024", "3023", "3623", "3710", "78329", "3666", "3460", "4477", "", "60479", "", "", "", ""}},
            {"2", {"", "3022", "3021", "3020", "", "3795", "3034", "3832", "", "2445", "91988", "4282", "", ""}},
            {"3", {"", "", "11212", "", "", "", "", "", "", "", "", "", "", ""}},
            {"4", {"", "", "", "3031", "", "3032", "3035", "3030", "", "3029", "", "", "", ""}},
            {"6", {"", "", "", "", "", "3958", "3036", "3033", "", "3028", "3456", "3027", "", "3026"}},
            {"8", {"", "", "", "", "", "", "41539", "", "728", "", "", "92438", "", ""}},
            {"16", {"", "", "", "", "", "", "", "", "", "", "", "91405", "", ""}}
        });
    m_dimensionDefinitions.append(plates);

    DimensionDefinition tiles;
    tiles.catalog = QStringLiteral("Tiles");
    tiles.section = QStringLiteral("Standard");
    tiles.title = tr("Standard Tiles — Width × Length");
    tiles.entries = makeDimensionEntries(
        studLengths,
        {
            {"1", {"3070b", "3069b", "63864", "2431", "", "6636", "4162", "", "", "", "", "", "", ""}},
            {"2", {"", "3068b", "26603", "87079", "", "69729", "", "", "", "", "", "", "", ""}},
            {"4", {"", "", "", "1751", "", "", "8165", "", "", "", "", "", "", ""}},
            {"6", {"", "", "", "", "", "", "6320", "", "", "", "", "", "", ""}}
        });
    m_dimensionDefinitions.append(tiles);

    const QStringList beamLengths = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"
    };

    DimensionDefinition technic;
    technic.catalog = QStringLiteral("Technic Beams & Liftarms");
    technic.section = QString();
    technic.title = tr("Straight Technic Beams — Thickness × Length");
    technic.entries = makeDimensionEntries(
        beamLengths,
        {
            {tr("Thin"), {"", "41677", "6632", "32449", "32017", "32063", "32065", "", "", "", "", "", "", "", ""}},
            {tr("Thick"), {"", "43857", "32523", "", "32316", "", "32524", "", "40490", "", "32525", "", "41239", "", "32278"}}
        });
    m_dimensionDefinitions.append(technic);
}

void PartReferenceDialog::populateCatalogList()
{
    if (!m_catalogList)
        return;

    m_catalogList->clear();
    for (const QString& catalog : m_manifest.catalogs())
        m_catalogList->addItem(catalog);

    if (m_catalogList->count() > 0)
        m_catalogList->setCurrentRow(0);
}

QString PartReferenceDialog::currentCatalog() const
{
    if (!m_catalogList || !m_catalogList->currentItem())
        return {};
    return m_catalogList->currentItem()->text();
}

PartReferenceDialog::ViewMode PartReferenceDialog::currentViewMode() const
{
    if (!m_viewCombo)
        return ViewMode::Gallery;

    return m_viewCombo->currentData().toString() == QStringLiteral("DimensionGrid")
               ? ViewMode::DimensionGrid
               : ViewMode::Gallery;
}

void PartReferenceDialog::catalogSelectionChanged()
{
    if (m_restoringUiState)
        return;

    const QString catalog = currentCatalog();
    if (catalog.isEmpty())
        return;

    UserSettings::instance().setPartReferenceCatalog(catalog);
    updateViewSelector();
    showCurrentCatalogPage();
}

void PartReferenceDialog::updateViewSelector()
{
    if (!m_viewCombo)
        return;

    const QString catalog = currentCatalog();
    const QString savedMode = UserSettings::instance().partReferenceViewMode(catalog);

    QSignalBlocker blocker(m_viewCombo);
    m_viewCombo->clear();
    m_viewCombo->addItem(tr("Gallery"), QStringLiteral("Gallery"));

    if (catalogSupportsDimensionGrid(catalog))
        m_viewCombo->addItem(tr("Dimension Grid"), QStringLiteral("DimensionGrid"));

    const int savedIndex = m_viewCombo->findData(savedMode);
    m_viewCombo->setCurrentIndex(savedIndex >= 0 ? savedIndex : 0);
    m_viewCombo->setVisible(m_viewCombo->count() > 1);
}

bool PartReferenceDialog::catalogSupportsDimensionGrid(const QString& catalog) const
{
    for (const DimensionDefinition& definition : m_dimensionDefinitions) {
        if (definition.catalog == catalog)
            return true;
    }
    return false;
}

QList<PartReferenceDialog::DimensionDefinition>
PartReferenceDialog::dimensionDefinitionsForCatalog(const QString& catalog) const
{
    QList<DimensionDefinition> result;
    for (const DimensionDefinition& definition : m_dimensionDefinitions) {
        if (definition.catalog == catalog)
            result.append(definition);
    }
    return result;
}

QString PartReferenceDialog::pageKey(const QString& catalog, ViewMode mode)
{
    return catalog + QLatin1Char('|')
           + (mode == ViewMode::DimensionGrid ? QStringLiteral("dimension")
                                              : QStringLiteral("gallery"));
}

QWidget* PartReferenceDialog::ensureCatalogPage(const QString& catalog, ViewMode mode)
{
    const QString key = pageKey(catalog, mode);
    if (m_pagesByKey.contains(key))
        return m_pagesByKey.value(key);

    QWidget* page = (mode == ViewMode::DimensionGrid)
                        ? buildCatalogDimensionPage(catalog)
                        : buildCatalogGalleryPage(catalog);
    if (!page)
        return nullptr;

    m_pagesByKey.insert(key, page);
    m_contentStack->addWidget(page);
    return page;
}

QWidget* PartReferenceDialog::buildCatalogGalleryPage(const QString& catalog)
{
    auto* page = new QWidget(m_contentStack);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(page);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(buildGalleryContent(m_manifest.entriesForCatalog(catalog),
                                              scrollArea,
                                              true));
    layout->addWidget(scrollArea);
    return page;
}

QWidget* PartReferenceDialog::buildGalleryContent(const QList<PartReferenceEntry>& entries,
                                                  QWidget* parent,
                                                  bool includeSectionHeadings)
{
    auto* content = new QWidget(parent);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setAlignment(Qt::AlignTop);

    QSet<QString> missingImages;
    QString currentSection;
    QGridLayout* sectionGrid = nullptr;
    int sectionIndex = 0;

    for (const PartReferenceEntry& entry : entries) {
        if (!sectionGrid || (includeSectionHeadings && entry.section != currentSection)) {
            currentSection = entry.section;
            sectionIndex = 0;

            if (includeSectionHeadings) {
                auto* sectionLabel = new QLabel(currentSection, content);
                QFont font = sectionLabel->font();
                font.setBold(true);
                sectionLabel->setFont(font);
                sectionLabel->setContentsMargins(0, 8, 0, 2);
                contentLayout->addWidget(sectionLabel);
            }

            auto* sectionWidget = new QWidget(content);
            sectionGrid = new QGridLayout(sectionWidget);
            sectionGrid->setContentsMargins(0, 0, 0, 0);
            sectionGrid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
            sectionGrid->setHorizontalSpacing(10);
            sectionGrid->setVerticalSpacing(10);
            contentLayout->addWidget(sectionWidget);
        }

        QToolButton* card = createPartCard(content, entry.partNumber, entry.partName);
        sectionGrid->addWidget(card, sectionIndex / GalleryColumns, sectionIndex % GalleryColumns);
        loadCardImageOrQueue(entry.partNumber, missingImages);
        ++sectionIndex;
    }

    if (entries.isEmpty()) {
        auto* emptyLabel = new QLabel(tr("No Part Reference entries are available."), content);
        emptyLabel->setWordWrap(true);
        contentLayout->addWidget(emptyLabel);
    }

    contentLayout->addStretch(1);
    requestMissingImages(missingImages.values());
    return content;
}

QList<PartReferenceEntry> PartReferenceDialog::entriesNotInDimensionGrid(
    const QList<PartReferenceEntry>& catalogEntries,
    const QList<DimensionDefinition>& definitions) const
{
    QSet<QString> gridParts;
    for (const DimensionDefinition& definition : definitions) {
        for (const DimensionEntry& entry : definition.entries)
            gridParts.insert(normalizedKey(entry.partNumber));
    }

    QList<PartReferenceEntry> remaining;
    for (const PartReferenceEntry& entry : catalogEntries) {
        if (!gridParts.contains(normalizedKey(entry.partNumber)))
            remaining.append(entry);
    }
    return remaining;
}

QWidget* PartReferenceDialog::buildCatalogDimensionPage(const QString& catalog)
{
    auto* page = new QWidget(m_contentStack);
    auto* outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(page);
    scrollArea->setWidgetResizable(true);
    auto* content = new QWidget(scrollArea);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setAlignment(Qt::AlignTop);

    const QList<DimensionDefinition> definitions = dimensionDefinitionsForCatalog(catalog);
    QSet<QString> missingImages;

    for (const DimensionDefinition& definition : definitions) {
        auto* title = new QLabel(definition.title, content);
        QFont titleFont = title->font();
        titleFont.setBold(true);
        title->setFont(titleFont);
        title->setContentsMargins(0, 8, 0, 4);
        contentLayout->addWidget(title);

        auto* matrixWidget = new QWidget(content);
        auto* grid = new QGridLayout(matrixWidget);
        grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(8);

        QHash<int, QString> columnLabels;
        QHash<int, QString> rowLabels;
        for (const DimensionEntry& entry : definition.entries) {
            columnLabels.insert(entry.column, entry.columnLabel);
            rowLabels.insert(entry.row, entry.rowLabel);
        }

        QList<int> columns = columnLabels.keys();
        QList<int> rows = rowLabels.keys();
        std::sort(columns.begin(), columns.end());
        std::sort(rows.begin(), rows.end());

        auto* corner = new QLabel(tr("Width / Length"), matrixWidget);
        corner->setAlignment(Qt::AlignCenter);
        grid->addWidget(corner, 0, 0);

        for (int column : columns) {
            auto* label = new QLabel(columnLabels.value(column), matrixWidget);
            label->setAlignment(Qt::AlignCenter);
            grid->addWidget(label, 0, column + 1);
        }
        for (int row : rows) {
            auto* label = new QLabel(rowLabels.value(row), matrixWidget);
            label->setAlignment(Qt::AlignCenter);
            grid->addWidget(label, row + 1, 0);
        }

        for (const DimensionEntry& gridEntry : definition.entries) {
            const PartReferenceEntry* manifestEntry = m_manifest.findByPartNumber(gridEntry.partNumber);
            if (!manifestEntry) {
                qWarning() << "Part Reference dimension grid references unknown part:"
                           << gridEntry.partNumber;
                continue;
            }

            QToolButton* card = createPartCard(matrixWidget,
                                               manifestEntry->partNumber,
                                               manifestEntry->partName);
            grid->addWidget(card, gridEntry.row + 1, gridEntry.column + 1);
            loadCardImageOrQueue(manifestEntry->partNumber, missingImages);
        }

        contentLayout->addWidget(matrixWidget);
    }

    const QList<PartReferenceEntry> remaining = entriesNotInDimensionGrid(
        m_manifest.entriesForCatalog(catalog), definitions);
    if (!remaining.isEmpty()) {
        auto* otherLabel = new QLabel(tr("Other %1").arg(catalog), content);
        QFont font = otherLabel->font();
        font.setBold(true);
        otherLabel->setFont(font);
        otherLabel->setContentsMargins(0, 14, 0, 4);
        contentLayout->addWidget(otherLabel);

        QWidget* remainingGallery = buildGalleryContent(remaining, content, true);
        remainingGallery->layout()->setContentsMargins(0, 0, 0, 0);
        contentLayout->addWidget(remainingGallery);
    }

    contentLayout->addStretch(1);
    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);

    requestMissingImages(missingImages.values());
    return page;
}

QWidget* PartReferenceDialog::buildSearchPage(const QList<PartReferenceEntry>& entries,
                                              int totalMatches)
{
    auto* page = new QWidget(m_contentStack);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    if (totalMatches > entries.size()) {
        auto* note = new QLabel(
            tr("Showing the first %1 of %2 matching reference parts. Refine Find to narrow the results.")
                .arg(entries.size())
                .arg(totalMatches),
            page);
        note->setWordWrap(true);
        layout->addWidget(note);
    }

    auto* scrollArea = new QScrollArea(page);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(buildGalleryContent(entries, scrollArea, true));
    layout->addWidget(scrollArea, 1);
    return page;
}

void PartReferenceDialog::showCurrentCatalogPage()
{
    if (!m_contentStack)
        return;

    if (m_searchEdit && !m_searchEdit->text().trimmed().isEmpty()) {
        refreshSearchResults();
        return;
    }

    const QString catalog = currentCatalog();
    if (catalog.isEmpty())
        return;

    const QList<PartReferenceEntry> entries = m_manifest.entriesForCatalog(catalog);
    m_catalogTitleLabel->setText(catalog);
    m_catalogCountLabel->setText(tr("%1 parts").arg(entries.size()));

    ViewMode mode = currentViewMode();
    if (mode == ViewMode::DimensionGrid && !catalogSupportsDimensionGrid(catalog))
        mode = ViewMode::Gallery;

    if (QWidget* page = ensureCatalogPage(catalog, mode))
        m_contentStack->setCurrentWidget(page);
}

void PartReferenceDialog::refreshSearchResults()
{
    if (!m_searchEdit || !m_contentStack)
        return;

    const QString search = m_searchEdit->text().trimmed();
    if (search.isEmpty()) {
        if (m_catalogList)
            m_catalogList->setEnabled(true);
        if (m_viewCombo)
            m_viewCombo->setEnabled(true);
        showCurrentCatalogPage();
        return;
    }

    if (m_catalogList)
        m_catalogList->setEnabled(false);
    if (m_viewCombo)
        m_viewCombo->setEnabled(false);

    const QList<PartReferenceEntry> allMatches = m_manifest.search(search);
    const QList<PartReferenceEntry> displayed = allMatches.mid(0, SearchDisplayLimit);

    m_catalogTitleLabel->setText(tr("Search Results"));
    m_catalogCountLabel->setText(tr("%1 matches").arg(allMatches.size()));

    if (m_searchPage) {
        m_contentStack->removeWidget(m_searchPage);
        m_searchPage->deleteLater();
        m_searchPage = nullptr;
    }

    m_searchPage = buildSearchPage(displayed, allMatches.size());
    m_contentStack->addWidget(m_searchPage);
    m_contentStack->setCurrentWidget(m_searchPage);
}

QToolButton* PartReferenceDialog::createPartCard(QWidget* parent,
                                                 const QString& partNumber,
                                                 const QString& partName)
{
    auto* button = new QToolButton(parent);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setIconSize(QSize(88, 88));
    button->setMinimumSize(145, 135);
    button->setMaximumWidth(175);
    button->setText(QString("%1\n%2").arg(partNumber, partName));
    button->setToolTip(QString("%1 — %2").arg(partNumber, partName));
    button->setProperty("partNumber", partNumber);
    button->setProperty("partName", partName);

    m_cardsByPartNumber[normalizedKey(partNumber)].append(button);

    connect(button, &QObject::destroyed, this, [this, key = normalizedKey(partNumber), button]() {
        auto it = m_cardsByPartNumber.find(key);
        if (it == m_cardsByPartNumber.end())
            return;
        it.value().removeAll(button);
        if (it.value().isEmpty())
            m_cardsByPartNumber.erase(it);
    });

    connect(button, &QToolButton::clicked, this, [this, partNumber, partName]() {
        selectPart(partNumber, partName);
    });

    return button;
}

void PartReferenceDialog::loadCardImageOrQueue(const QString& partNumber,
                                               QSet<QString>& missingImages)
{
    const QString cachedPath = m_partImageService->cachedImagePath(partNumber);
    if (!cachedPath.isEmpty())
        setCardImage(partNumber, cachedPath);
    else
        missingImages.insert(partNumber);
}

void PartReferenceDialog::setCardImage(const QString& partNumber, const QString& imagePath)
{
    const QString key = normalizedKey(partNumber);
    if (!m_cardsByPartNumber.contains(key))
        return;

    QPixmap pixmap(imagePath);
    if (pixmap.isNull())
        return;

    const QIcon icon(pixmap.scaled(88,
                                   88,
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation));

    const QList<QToolButton*> buttons = m_cardsByPartNumber.value(key);
    for (QToolButton* button : buttons) {
        if (button)
            button->setIcon(icon);
    }
}

void PartReferenceDialog::requestMissingImages(const QStringList& partNumbers)
{
    if (partNumbers.isEmpty())
        return;

    const QString apiKey = UserSettings::instance().rebrickableApiKey().trimmed();
    if (apiKey.isEmpty() || RebrickableApiClient::isSessionBlocked())
        return;

    QStringList unique = partNumbers;
    unique.removeDuplicates();

    for (int first = 0; first < unique.size(); first += ImageBatchSize) {
        m_rebrickableApiClient->getPartImageUrls(
            unique.mid(first, ImageBatchSize),
            apiKey,
            RebrickableApiClient::RequestPriority::Background);
    }
}

void PartReferenceDialog::selectPart(const QString& partNumber, const QString& partName)
{
    m_selectedPartNumber = partNumber.trimmed();
    m_selectedPartName = partName.trimmed();

    m_selectedLabel->setText(
        tr("Selected: %1 — %2").arg(m_selectedPartNumber, m_selectedPartName));

    m_copyButton->setEnabled(!m_selectedPartNumber.isEmpty());
    m_sendButton->setEnabled(m_addInventoryAvailable && !m_selectedPartNumber.isEmpty());
}

void PartReferenceDialog::restoreUiState()
{
    m_restoringUiState = true;

    UserSettings& settings = UserSettings::instance();
    const QByteArray geometry = settings.partReferenceGeometry();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    const QByteArray splitterState = settings.partReferenceSplitterState();
    if (!splitterState.isEmpty() && m_splitter)
        m_splitter->restoreState(splitterState);

    const QString savedCatalog = settings.partReferenceCatalog();
    if (m_catalogList && !savedCatalog.isEmpty()) {
        const QList<QListWidgetItem*> matches = m_catalogList->findItems(savedCatalog, Qt::MatchExactly);
        if (!matches.isEmpty())
            m_catalogList->setCurrentItem(matches.first());
    }

    updateViewSelector();
    m_restoringUiState = false;
}

void PartReferenceDialog::saveUiState()
{
    UserSettings& settings = UserSettings::instance();
    settings.setPartReferenceGeometry(saveGeometry());

    if (m_splitter)
        settings.setPartReferenceSplitterState(m_splitter->saveState());

    const QString catalog = currentCatalog();
    if (!catalog.isEmpty()) {
        settings.setPartReferenceCatalog(catalog);
        settings.setPartReferenceViewMode(
            catalog,
            viewModeName(currentViewMode() == ViewMode::DimensionGrid));
    }
}
