#include "MinifigsCatalogWidget.h"

#include "MinifigDetailsDialog.h"

#include "../../import/RebrickableMinifigCatalogImporter.h"
#include "../../models/MinifigCatalogSearchCriteria.h"
#include "../../models/MinifigCatalogSearchResult.h"
#include "../../repositories/MinifigCatalogRepository.h"
#include "../../services/images/MinifigImageService.h"
#include "../../settings/UserSettings.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

MinifigsCatalogWidget::MinifigsCatalogWidget(QWidget* parent)
    : QWidget(parent)
    , m_imageService(new MinifigImageService(this))
{
    auto* mainLayout = new QVBoxLayout(this);
    auto* titleLayout = new QHBoxLayout();
    titleLayout->addWidget(new QLabel("Minifigs Catalog", this));
    titleLayout->addStretch();
    m_importButton = new QPushButton("Import Rebrickable Minifigs (CSV/ZIP)", this);
    titleLayout->addWidget(m_importButton);

    auto* filterLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search by Minifig number or name");
    m_searchButton = new QPushButton("Search", this);
    filterLayout->addWidget(new QLabel("Search:", this));
    filterLayout->addWidget(m_searchEdit, 1);
    filterLayout->addWidget(m_searchButton);

    m_resultLabel = new QLabel("Import minifigs.csv/ZIP or enter search criteria.", this);
    m_resultsTable = new QTableWidget(this);
    m_resultsTable->setColumnCount(5);
    m_resultsTable->setHorizontalHeaderLabels(
        QStringList() << "Image" << "Minifig #" << "Name" << "Parts" << "Action");
    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultsTable->verticalHeader()->setVisible(false);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    auto* pagingLayout = new QHBoxLayout();
    m_summaryLabel = new QLabel(this);
    m_previousButton = new QPushButton("Previous", this);
    m_pageLabel = new QLabel(this);
    m_nextButton = new QPushButton("Next", this);
    pagingLayout->addWidget(m_summaryLabel);
    pagingLayout->addStretch();
    pagingLayout->addWidget(m_previousButton);
    pagingLayout->addWidget(m_pageLabel);
    pagingLayout->addWidget(m_nextButton);

    mainLayout->addLayout(titleLayout);
    mainLayout->addLayout(filterLayout);
    mainLayout->addWidget(m_resultLabel);
    mainLayout->addWidget(m_resultsTable, 1);
    mainLayout->addLayout(pagingLayout);

    connect(m_searchButton, &QPushButton::clicked, this, [this]() {
        m_currentPage = 0;
        searchMinifigs();
    });
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        m_currentPage = 0;
        searchMinifigs();
    });
    connect(m_previousButton, &QPushButton::clicked, this, &MinifigsCatalogWidget::previousPage);
    connect(m_nextButton, &QPushButton::clicked, this, &MinifigsCatalogWidget::nextPage);
    connect(m_importButton, &QPushButton::clicked, this, &MinifigsCatalogWidget::importMinifigs);
    connect(m_imageService,
            &MinifigImageService::imageReady,
            this,
            &MinifigsCatalogWidget::displayImage);
    connect(m_imageService,
            &MinifigImageService::imageFailed,
            this,
            [this](const QString& minifigNumber, const QString&) {
                for (int row = 0; row < m_resultsTable->rowCount(); ++row) {
                    const QTableWidgetItem* item = m_resultsTable->item(row, 1);
                    if (!item || item->text().compare(minifigNumber, Qt::CaseInsensitive) != 0)
                        continue;
                    auto* label = qobject_cast<QLabel*>(m_resultsTable->cellWidget(row, 0));
                    if (label)
                        label->setText("Unavailable");
                }
            });

    refresh();
}

void MinifigsCatalogWidget::refresh()
{
    m_currentPage = 0;
    MinifigCatalogRepository repository;
    if (repository.count() > 0) {
        searchMinifigs();
        return;
    }

    m_resultsTable->setRowCount(0);
    m_totalResultCount = 0;
    m_resultLabel->setText("No Minifigs Catalog has been imported yet.");
    updatePagingControls();
}

void MinifigsCatalogWidget::searchMinifigs()
{
    const QString effectiveSearchText = m_searchEdit->text().trimmed();
    if (effectiveSearchText != m_loadedSearchText) {
        m_currentPage = 0;
        m_loadedSearchText = effectiveSearchText;
    }

    MinifigCatalogSearchCriteria criteria;
    criteria.searchText = effectiveSearchText;
    criteria.provider = QStringLiteral("Rebrickable");
    criteria.limit = UserSettings::instance().resultsPerPage();
    criteria.offset = m_currentPage * criteria.limit;

    MinifigCatalogRepository repository;
    m_totalResultCount = repository.count(criteria);
    const QList<MinifigCatalogSearchResult> results = repository.search(criteria);
    m_imageService->clearQueuedRequests();
    m_resultsTable->setRowCount(0);

    for (const MinifigCatalogSearchResult& result : results) {
        const int row = m_resultsTable->rowCount();
        m_resultsTable->insertRow(row);
        m_resultsTable->setRowHeight(row, 76);

        auto* imageLabel = new QLabel("Loading...", m_resultsTable);
        imageLabel->setFixedSize(72, 72);
        imageLabel->setAlignment(Qt::AlignCenter);
        auto* numberItem = new QTableWidgetItem(result.rebrickableExternalId);
        numberItem->setData(Qt::UserRole, result.minifig.id());
        auto* nameItem = new QTableWidgetItem(result.minifig.name());
        auto* partsItem = new QTableWidgetItem(QString::number(result.minifig.numberOfParts()));
        partsItem->setTextAlignment(Qt::AlignCenter);
        auto* actionCombo = new QComboBox(m_resultsTable);
        actionCombo->addItem("Actions...");
        actionCombo->addItem("Details...");

        m_resultsTable->setCellWidget(row, 0, imageLabel);
        m_resultsTable->setItem(row, 1, numberItem);
        m_resultsTable->setItem(row, 2, nameItem);
        m_resultsTable->setItem(row, 3, partsItem);
        m_resultsTable->setCellWidget(row, 4, actionCombo);

        const int minifigCatalogId = result.minifig.id();
        connect(actionCombo, &QComboBox::currentIndexChanged, this,
                [this, actionCombo, minifigCatalogId](int index) {
                    if (index <= 0)
                        return;
                    actionCombo->setCurrentIndex(0);
                    MinifigDetailsDialog dialog(minifigCatalogId, this);
                    dialog.exec();
                });

        const QString cachedPath = m_imageService->cachedImagePath(
            result.rebrickableExternalId);
        if (!cachedPath.isEmpty())
            displayImage(result.rebrickableExternalId, cachedPath);
        else if (result.minifig.imageUrl().isEmpty())
            imageLabel->setText("No Image");
        else
            m_imageService->requestMinifigImage(result.rebrickableExternalId,
                                                 result.minifig.imageUrl());
    }

    if (results.isEmpty()) {
        m_resultLabel->setText(criteria.searchText.isEmpty()
                                   ? "No Minifigs are available in the Minifigs Catalog."
                                   : "No Minifigs match the current search.");
    } else {
        m_resultLabel->setText(QString("Showing results %1 - %2.")
                                   .arg(criteria.offset + 1)
                                   .arg(criteria.offset + results.size()));
    }
    updatePagingControls();
}

void MinifigsCatalogWidget::displayImage(const QString& minifigNumber,
                                         const QString& imagePath)
{
    QPixmap pixmap(imagePath);
    if (pixmap.isNull())
        return;

    for (int row = 0; row < m_resultsTable->rowCount(); ++row) {
        const QTableWidgetItem* numberItem = m_resultsTable->item(row, 1);
        if (!numberItem || numberItem->text().compare(minifigNumber, Qt::CaseInsensitive) != 0)
            continue;
        auto* label = qobject_cast<QLabel*>(m_resultsTable->cellWidget(row, 0));
        if (label) {
            label->setPixmap(pixmap.scaled(label->size(),
                                           Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
            label->setText(QString());
        }
    }
}

void MinifigsCatalogWidget::previousPage()
{
    if (m_searchEdit->text().trimmed() != m_loadedSearchText) {
        searchMinifigs();
        return;
    }
    if (m_currentPage <= 0)
        return;
    --m_currentPage;
    searchMinifigs();
}

void MinifigsCatalogWidget::nextPage()
{
    if (m_searchEdit->text().trimmed() != m_loadedSearchText) {
        searchMinifigs();
        return;
    }
    const int pageSize = UserSettings::instance().resultsPerPage();
    const int totalPages = qMax(1, (m_totalResultCount + pageSize - 1) / pageSize);
    if (m_currentPage + 1 >= totalPages)
        return;
    ++m_currentPage;
    searchMinifigs();
}

void MinifigsCatalogWidget::updatePagingControls()
{
    const int pageSize = UserSettings::instance().resultsPerPage();
    const int totalPages = qMax(1, (m_totalResultCount + pageSize - 1) / pageSize);
    m_previousButton->setEnabled(m_currentPage > 0);
    m_nextButton->setEnabled(m_currentPage + 1 < totalPages);
    m_summaryLabel->setText(QString("%1 %2")
                                .arg(QLocale().toString(m_totalResultCount))
                                .arg(m_totalResultCount == 1 ? "Minifig" : "Minifigs"));
    m_pageLabel->setText(QString("Page %1 of %2").arg(m_currentPage + 1).arg(totalPages));
}

void MinifigsCatalogWidget::importMinifigs()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "Import Rebrickable Minifigs Catalog",
        QString(),
        "Rebrickable Catalog Files (*.csv *.CSV *.zip *.ZIP);;"
        "CSV Files (*.csv *.CSV);;ZIP Files (*.zip *.ZIP)");
    if (fileName.isEmpty())
        return;

    const auto response = QMessageBox::question(
        this,
        "Import Minifigs Catalog",
        "Import/update the BrickSuite Minifigs Catalog from this Rebrickable CSV or ZIP?\n\n"
        "Entries missing from a complete later snapshot will be deactivated, not deleted.",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (response != QMessageBox::Yes)
        return;

    RebrickableMinifigCatalogImporter importer;
    const auto result = importer.importFile(fileName);
    if (!result.success) {
        QMessageBox::critical(this, "Import Minifigs Catalog", result.message);
        return;
    }

    QMessageBox::information(this,
                             "Import Minifigs Catalog",
                             QString("Minifigs Catalog import completed.\n\n"
                                     "Rows Read: %1\nNew: %2\nUpdated: %3\n"
                                     "Unchanged: %4\nDeactivated: %5")
                                 .arg(result.rowsRead)
                                 .arg(result.inserted)
                                 .arg(result.updated)
                                 .arg(result.unchanged)
                                 .arg(result.deactivated));
    refresh();
}
