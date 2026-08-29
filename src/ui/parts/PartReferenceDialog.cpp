/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 */

#include "PartReferenceDialog.h"

#include "../../models/Part.h"
#include "../../models/PartCategory.h"
#include "../../repositories/PartCategoryRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../services/RebrickableApiClient.h"
#include "../../services/images/PartImageService.h"
#include "../../settings/UserSettings.h"

#include <QApplication>
#include <algorithm>
#include <QClipboard>
#include <QCloseEvent>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <optional>

namespace {

QString normalizedKey(const QString& value)
{
    return value.trimmed().toLower();
}

} // namespace

PartReferenceDialog::PartReferenceDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Part Reference"));
    setWindowFlag(Qt::Window, true);
    setWindowModality(Qt::NonModal);
    setModal(false);
    resize(1150, 780);

    m_partImageService = new PartImageService(this);
    m_rebrickableApiClient = new RebrickableApiClient(this);

    QString manifestError;
    if (!m_manifest.load(&manifestError)) {
        QMessageBox::warning(this,
                             tr("Part Reference"),
                             tr("The Part Reference manifest could not be loaded.\n\n%1")
                                 .arg(manifestError));
    }

    initializeDefinitions();
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

    ensureCurrentPageLoaded();
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
    m_searchEdit->setPlaceholderText(tr("Part number or name"));
    searchRow->addWidget(searchLabel);
    searchRow->addWidget(m_searchEdit, 1);
    mainLayout->addLayout(searchRow);

    m_groupTabs = new QTabWidget(this);

    for (GroupDefinition& group : m_groups) {
        auto* groupPage = new QWidget(m_groupTabs);
        auto* groupLayout = new QVBoxLayout(groupPage);
        groupLayout->setContentsMargins(0, 0, 0, 0);

        group.tabs = new QTabWidget(groupPage);

        for (PageDefinition& page : group.pages) {
            page.page = new QWidget(group.tabs);
            group.tabs->addTab(page.page, page.name);
        }

        groupLayout->addWidget(group.tabs);
        m_groupTabs->addTab(groupPage, group.name);

        connect(group.tabs, &QTabWidget::currentChanged, this, [this](int) {
            ensureCurrentPageLoaded();
            applySearchFilter();
        });
    }

    connect(m_groupTabs, &QTabWidget::currentChanged, this, [this](int) {
        ensureCurrentPageLoaded();
        applySearchFilter();
    });

    mainLayout->addWidget(m_groupTabs, 1);

    auto* selectedRow = new QHBoxLayout();
    m_selectedLabel = new QLabel(tr("Selected: None"), this);
    m_selectedLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_copyButton = new QPushButton(tr("Copy Part #"), this);
    m_sendButton = new QPushButton(tr("Send to Add Part"), this);
    m_copyButton->setEnabled(false);
    m_sendButton->setEnabled(false);

    selectedRow->addWidget(m_selectedLabel, 1);
    selectedRow->addWidget(m_copyButton);
    selectedRow->addWidget(m_sendButton);
    mainLayout->addLayout(selectedRow);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &PartReferenceDialog::applySearchFilter);

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

void PartReferenceDialog::initializeDefinitions()
{
    const QStringList normalWidths = {
        "1", "2", "3", "4", "5", "6", "8", "10", "12", "14", "16", "20", "24"
    };

    GroupDefinition bricks;
    bricks.name = "Bricks";

    PageDefinition brickStandard;
    brickStandard.name = "Standard";
    brickStandard.dimensionGrid = true;
    brickStandard.rebrickableCategoryIds = {11};
    brickStandard.dimensionEntries = makeDimensionEntries(
        normalWidths,
        {
            {"1", {"3005", "3004", "3622", "3010", "", "3009", "3008", "6111", "6112", "", "2465", "", ""}},
            {"2", {"3004", "3003", "3002", "3001", "", "2456", "3007", "3006", "", "", "", "", ""}},
            {"4", {"3010", "3001", "", "", "", "3565", "", "6212", "4212", "", "", "", ""}}
        });
    bricks.pages.append(brickStandard);

    PageDefinition brickSpecial;
    brickSpecial.name = "Special";
    brickSpecial.rebrickableCategoryIds = {5};
    brickSpecial.galleryLimit = 140;
    bricks.pages.append(brickSpecial);

    PageDefinition brickSlope;
    brickSlope.name = "Sloped / Curved";
    brickSlope.rebrickableCategoryIds = {3, 37};
    brickSlope.galleryLimit = 140;
    bricks.pages.append(brickSlope);

    PageDefinition brickWedge;
    brickWedge.name = "Wedged";
    brickWedge.rebrickableCategoryIds = {6};
    brickWedge.galleryLimit = 120;
    bricks.pages.append(brickWedge);

    PageDefinition brickRound;
    brickRound.name = "Round";
    brickRound.rebrickableCategoryIds = {20};
    brickRound.galleryLimit = 120;
    bricks.pages.append(brickRound);

    m_groups.append(bricks);

    GroupDefinition plates;
    plates.name = "Plates";

    PageDefinition plateStandard;
    plateStandard.name = "Standard";
    plateStandard.dimensionGrid = true;
    plateStandard.rebrickableCategoryIds = {14};
    plateStandard.dimensionEntries = makeDimensionEntries(
        normalWidths,
        {
            {"1", {"3024", "3023", "3623", "3710", "78329", "3666", "3460", "4477", "60479", "", "", "", ""}},
            {"2", {"3023", "3022", "3021", "3020", "", "3795", "3034", "3832", "2445", "91988", "4282", "", "3026"}},
            {"3", {"", "", "11212", "", "", "", "", "", "", "", "", "", ""}},
            {"4", {"3710", "3020", "", "3031", "", "3032", "3035", "3030", "3029", "", "", "", ""}},
            {"6", {"", "", "", "3032", "", "3958", "3036", "3033", "3028", "", "", "", ""}},
            {"8", {"", "", "", "", "", "", "", "", "", "", "3027", "", ""}}
        });
    plates.pages.append(plateStandard);

    PageDefinition plateSpecial;
    plateSpecial.name = "Special";
    plateSpecial.rebrickableCategoryIds = {9};
    plateSpecial.galleryLimit = 140;
    plates.pages.append(plateSpecial);

    PageDefinition plateWedge;
    plateWedge.name = "Wedged";
    plateWedge.rebrickableCategoryIds = {49};
    plateWedge.galleryLimit = 120;
    plates.pages.append(plateWedge);

    PageDefinition plateRound;
    plateRound.name = "Round / Curved";
    plateRound.rebrickableCategoryIds = {21};
    plateRound.galleryLimit = 120;
    plates.pages.append(plateRound);

    m_groups.append(plates);

    GroupDefinition tiles;
    tiles.name = "Tiles";

    PageDefinition tileStandard;
    tileStandard.name = "Standard";
    tileStandard.dimensionGrid = true;
    tileStandard.rebrickableCategoryIds = {19};
    tileStandard.dimensionEntries = makeDimensionEntries(
        normalWidths,
        {
            {"1", {"3070", "3069", "63864", "2431", "", "3894", "4162", "", "", "", "", "", ""}},
            {"2", {"3069", "3068", "26603", "87079", "", "69729", "", "", "", "", "", "", ""}},
            {"4", {"2431", "87087", "", "1753", "", "", "", "", "", "", "", "", ""}}
        });
    tiles.pages.append(tileStandard);

    PageDefinition tileSpecial;
    tileSpecial.name = "Special";
    tileSpecial.rebrickableCategoryIds = {15};
    tileSpecial.galleryLimit = 140;
    tiles.pages.append(tileSpecial);

    PageDefinition tileRound;
    tileRound.name = "Round / Curved";
    tileRound.rebrickableCategoryIds = {67};
    tileRound.galleryLimit = 120;
    tiles.pages.append(tileRound);

    m_groups.append(tiles);

    GroupDefinition technic;
    technic.name = "Technic";

    PageDefinition technicBricks;
    technicBricks.name = "Bricks";
    technicBricks.dimensionGrid = true;
    technicBricks.rebrickableCategoryIds = {8};
    technicBricks.dimensionEntries = makeDimensionEntries(
        normalWidths,
        {
            {"1", {"6541", "3700", "5565", "3701", "", "3894", "3702", "2730", "3895", "32018", "3703", "", ""}}
        });
    technic.pages.append(technicBricks);

    const QStringList beamLengths = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"
    };

    PageDefinition technicBeams;
    technicBeams.name = "Beams";
    technicBeams.dimensionGrid = true;
    technicBeams.rebrickableCategoryIds = {51, 55};
    technicBeams.dimensionEntries = makeDimensionEntries(
        beamLengths,
        {
            {"Thin (1/3)", {"", "41677", "6632", "32449", "32017", "", "", "", "", "", "", "", "", "", ""}},
            {"Thick", {"", "60483", "32523", "", "32316", "", "32524", "", "40490", "", "32525", "", "41239", "", "32278"}}
        });
    technic.pages.append(technicBeams);

    PageDefinition technicConnectors;
    technicConnectors.name = "Connectors";
    technicConnectors.rebrickableCategoryIds = {12};
    technicConnectors.galleryLimit = 160;
    technic.pages.append(technicConnectors);

    PageDefinition technicPins;
    technicPins.name = "Pins / Bushes";
    technicPins.rebrickableCategoryIds = {53, 54};
    technicPins.galleryLimit = 120;
    technic.pages.append(technicPins);

    PageDefinition technicAxles;
    technicAxles.name = "Axles";
    technicAxles.rebrickableCategoryIds = {46};
    technicAxles.galleryLimit = 120;
    technic.pages.append(technicAxles);

    PageDefinition technicGears;
    technicGears.name = "Gears";
    technicGears.rebrickableCategoryIds = {52};
    technicGears.galleryLimit = 120;
    technic.pages.append(technicGears);

    PageDefinition technicPanels;
    technicPanels.name = "Panels";
    technicPanels.rebrickableCategoryIds = {40};
    technicPanels.galleryLimit = 140;
    technic.pages.append(technicPanels);

    m_groups.append(technic);

    GroupDefinition other;
    other.name = "Other";

    PageDefinition hinges;
    hinges.name = "Hinges / Arms / Turntables";
    hinges.rebrickableCategoryIds = {18};
    hinges.galleryLimit = 140;
    other.pages.append(hinges);

    PageDefinition bars;
    bars.name = "Bars / Ladders / Fences";
    bars.rebrickableCategoryIds = {32};
    bars.galleryLimit = 140;
    other.pages.append(bars);

    PageDefinition panels;
    panels.name = "Panels";
    panels.rebrickableCategoryIds = {23};
    panels.galleryLimit = 120;
    other.pages.append(panels);

    PageDefinition windows;
    windows.name = "Windows / Doors";
    windows.rebrickableCategoryIds = {16};
    windows.galleryLimit = 140;
    other.pages.append(windows);

    m_groups.append(other);
}

void PartReferenceDialog::restoreUiState()
{
    const QByteArray geometry = UserSettings::instance().partReferenceGeometry();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    const int groupIndex = UserSettings::instance().partReferenceGroupIndex();
    if (groupIndex >= 0 && groupIndex < m_groupTabs->count())
        m_groupTabs->setCurrentIndex(groupIndex);

    if (GroupDefinition* group = (groupIndex >= 0 && groupIndex < m_groups.size())
                                     ? &m_groups[groupIndex]
                                     : nullptr) {
        const int pageIndex = UserSettings::instance().partReferencePageIndex(groupIndex);
        if (group->tabs && pageIndex >= 0 && pageIndex < group->tabs->count())
            group->tabs->setCurrentIndex(pageIndex);
    }
}

void PartReferenceDialog::saveUiState()
{
    UserSettings::instance().setPartReferenceGeometry(saveGeometry());

    if (!m_groupTabs)
        return;

    const int groupIndex = m_groupTabs->currentIndex();
    UserSettings::instance().setPartReferenceGroupIndex(groupIndex);

    if (groupIndex >= 0 && groupIndex < m_groups.size()) {
        const GroupDefinition& group = m_groups.at(groupIndex);
        if (group.tabs)
            UserSettings::instance().setPartReferencePageIndex(groupIndex,
                                                               group.tabs->currentIndex());
    }
}

PartReferenceDialog::PageDefinition* PartReferenceDialog::currentPageDefinition()
{
    if (!m_groupTabs)
        return nullptr;

    const int groupIndex = m_groupTabs->currentIndex();
    if (groupIndex < 0 || groupIndex >= m_groups.size())
        return nullptr;

    GroupDefinition& group = m_groups[groupIndex];
    if (!group.tabs)
        return nullptr;

    const int pageIndex = group.tabs->currentIndex();
    if (pageIndex < 0 || pageIndex >= group.pages.size())
        return nullptr;

    return &group.pages[pageIndex];
}

void PartReferenceDialog::ensureCurrentPageLoaded()
{
    PageDefinition* definition = currentPageDefinition();
    if (!definition || definition->loaded)
        return;

    if (definition->dimensionGrid)
        buildDimensionPage(*definition);
    else
        buildGalleryPage(*definition);

    definition->loaded = true;
}

QList<int> PartReferenceDialog::localCategoryIds(
    const QList<int>& rebrickableCategoryIds) const
{
    QList<int> ids;
    PartCategoryRepository repository;

    for (const int rebrickableId : rebrickableCategoryIds) {
        const std::optional<PartCategory> category = repository.getByRebrickableId(rebrickableId);
        if (category)
            ids.append(category->id());
    }

    return ids;
}

bool PartReferenceDialog::partMatchesCategories(int partCategoryId,
                                                const QList<int>& localIds) const
{
    return localIds.isEmpty() || localIds.contains(partCategoryId);
}

bool PartReferenceDialog::isPrintedPartNumber(const QString& partNumber) const
{
    return normalizedKey(partNumber).contains("pr");
}

void PartReferenceDialog::buildDimensionPage(PageDefinition& definition)
{
    if (!definition.page)
        return;

    auto* outerLayout = new QVBoxLayout(definition.page);
    auto* scrollArea = new QScrollArea(definition.page);
    scrollArea->setWidgetResizable(true);

    auto* content = new QWidget(scrollArea);
    auto* grid = new QGridLayout(content);
    grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);

    const QList<int> validCategoryIds = localCategoryIds(definition.rebrickableCategoryIds);
    PartRepository repository;
    QSet<QString> missingImages;

    QHash<int, QString> rowLabels;
    QHash<int, QString> columnLabels;

    for (const DimensionEntry& entry : definition.dimensionEntries) {
        rowLabels.insert(entry.row, entry.rowLabel);
        columnLabels.insert(entry.column, entry.columnLabel);
    }

    grid->addWidget(new QLabel(tr("Size"), content), 0, 0, Qt::AlignCenter);

    QList<int> sortedColumns = columnLabels.keys();
    std::sort(sortedColumns.begin(), sortedColumns.end());
    for (const int column : sortedColumns) {
        auto* label = new QLabel(columnLabels.value(column), content);
        label->setAlignment(Qt::AlignCenter);
        grid->addWidget(label, 0, column + 1);
    }

    QList<int> sortedRows = rowLabels.keys();
    std::sort(sortedRows.begin(), sortedRows.end());
    for (const int row : sortedRows) {
        auto* label = new QLabel(rowLabels.value(row), content);
        label->setAlignment(Qt::AlignCenter);
        grid->addWidget(label, row + 1, 0);
    }

    for (const DimensionEntry& entry : definition.dimensionEntries) {
        if (isPrintedPartNumber(entry.partNumber))
            continue;

        const std::optional<Part> part = repository.getByPartNumber(entry.partNumber);
        if (!part)
            continue;

        if (!partMatchesCategories(part->partCategoryId(), validCategoryIds))
            continue;

        QToolButton* card = createPartCard(content, part->partNumber(), part->name());
        grid->addWidget(card, entry.row + 1, entry.column + 1);

        const QString cachedPath = m_partImageService->cachedImagePath(part->partNumber());
        if (!cachedPath.isEmpty())
            setCardImage(part->partNumber(), cachedPath);
        else
            missingImages.insert(part->partNumber());
    }

    content->setLayout(grid);
    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);

    requestMissingImages(missingImages.values());
}

void PartReferenceDialog::buildGalleryPage(PageDefinition& definition)
{
    if (!definition.page)
        return;

    auto* outerLayout = new QVBoxLayout(definition.page);
    auto* scrollArea = new QScrollArea(definition.page);
    scrollArea->setWidgetResizable(true);

    auto* content = new QWidget(scrollArea);
    auto* grid = new QGridLayout(content);
    grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    const QList<int> ids = localCategoryIds(definition.rebrickableCategoryIds);
    PartRepository repository;
    const QList<Part> parts = repository.getReferencePartsByCategoryIds(ids,
                                                                        definition.galleryLimit);

    QSet<QString> missingImages;
    constexpr int columns = 6;
    int index = 0;

    for (const Part& part : parts) {
        if (isPrintedPartNumber(part.partNumber()))
            continue;

        QToolButton* card = createPartCard(content, part.partNumber(), part.name());
        grid->addWidget(card, index / columns, index % columns);

        const QString cachedPath = m_partImageService->cachedImagePath(part.partNumber());
        if (!cachedPath.isEmpty())
            setCardImage(part.partNumber(), cachedPath);
        else
            missingImages.insert(part.partNumber());

        ++index;
    }

    if (index == 0) {
        auto* emptyLabel = new QLabel(
            tr("No non-printed reference parts are available for this category."),
            content);
        emptyLabel->setWordWrap(true);
        grid->addWidget(emptyLabel, 0, 0);
    }

    content->setLayout(grid);
    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);

    requestMissingImages(missingImages.values());
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
    m_currentCards.append(button);

    connect(button, &QToolButton::clicked, this, [this, partNumber, partName]() {
        selectPart(partNumber, partName);
    });


    return button;
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

void PartReferenceDialog::applySearchFilter()
{
    ensureCurrentPageLoaded();

    const QString search = normalizedKey(m_searchEdit ? m_searchEdit->text() : QString());

    PageDefinition* currentDefinition = currentPageDefinition();
    QWidget* currentPage = currentDefinition ? currentDefinition->page : nullptr;

    for (auto it = m_cardsByPartNumber.cbegin(); it != m_cardsByPartNumber.cend(); ++it) {
        for (QToolButton* card : it.value()) {
            if (!card)
                continue;

            // Only filter cards belonging to the currently visible leaf page.
            QWidget* ancestor = card;
            bool onCurrentPage = false;
            while (ancestor) {
                if (ancestor == currentPage) {
                    onCurrentPage = true;
                    break;
                }
                ancestor = ancestor->parentWidget();
            }

            if (!onCurrentPage)
                continue;

            const QString number = normalizedKey(card->property("partNumber").toString());
            const QString name = normalizedKey(card->property("partName").toString());
            card->setVisible(search.isEmpty()
                             || number.contains(search)
                             || name.contains(search));
        }
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
