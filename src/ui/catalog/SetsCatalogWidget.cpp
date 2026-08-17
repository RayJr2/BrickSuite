#include "SetsCatalogWidget.h"
#include "SetDetailsDialog.h"

#include "../../import/RebrickableSetCatalogImporter.h"

#include "../../models/SetCatalogItem.h"
#include "../../models/SetCatalogSearchCriteria.h"

#include "../../repositories/SetCatalogRepository.h"

#include "../../settings/UserSettings.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

SetsCatalogWidget::SetsCatalogWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* titleLayout = new QHBoxLayout();

    auto* titleLabel = new QLabel("Sets Catalog", this);

    m_importButton = new QPushButton("Import Rebrickable sets.csv", this);

    titleLayout->addWidget(titleLabel);

    titleLayout->addStretch();

    titleLayout->addWidget(m_importButton);

    auto* filterLayout = new QHBoxLayout();

    m_searchEdit = new QLineEdit(this);

    m_searchEdit->setPlaceholderText("Search by set number or name");

    m_yearCombo = new QComboBox(this);

    m_searchButton = new QPushButton("Search", this);

    filterLayout->addWidget(new QLabel("Search:", this));

    filterLayout->addWidget(m_searchEdit, 1);

    filterLayout->addWidget(new QLabel("Year:", this));

    filterLayout->addWidget(m_yearCombo);

    filterLayout->addWidget(m_searchButton);

    m_resultLabel = new QLabel("Import sets.csv or enter search criteria.", this);

    m_resultsTable = new QTableWidget(this);

    m_resultsTable->setColumnCount(6);

    m_resultsTable->setHorizontalHeaderLabels(QStringList() << "Set #"
                                                            << "Name"
                                                            << "Year"
                                                            << "Theme ID"
                                                            << "Parts"
                                                            << "Action");

    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_resultsTable->setSelectionMode(QAbstractItemView::SingleSelection);

    m_resultsTable->verticalHeader()->setVisible(false);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    for (int column = 2; column <= 5; ++column) {
        m_resultsTable->horizontalHeader()->setSectionResizeMode(column,
                                                                 QHeaderView::ResizeToContents);
    }

    auto* pagingLayout = new QHBoxLayout();

    m_previousButton = new QPushButton("Previous", this);

    m_nextButton = new QPushButton("Next", this);

    m_pageLabel = new QLabel(this);
    m_summaryLabel = new QLabel(this);

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
        searchSets();
    });

    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        m_currentPage = 0;
        searchSets();
    });

    connect(m_yearCombo, &QComboBox::currentIndexChanged, this, [this]() {
        m_currentPage = 0;
        searchSets();
    });

    connect(m_previousButton, &QPushButton::clicked, this, &SetsCatalogWidget::previousPage);

    connect(m_nextButton, &QPushButton::clicked, this, &SetsCatalogWidget::nextPage);

    connect(m_importButton, &QPushButton::clicked, this, &SetsCatalogWidget::importSetsCsv);

    refresh();
}

void SetsCatalogWidget::refresh()
{
    SetCatalogRepository repository;

    const int total = repository.count();

    loadYears();

    m_currentPage = 0;

    if (total > 0) {
        searchSets();
    } else {
        m_resultsTable->setRowCount(0);

        m_resultLabel->setText("No Sets Catalog has been imported yet.");

        m_lastResultCount = 0;
        m_totalResultCount = 0;

        updatePagingControls();
    }
}

void SetsCatalogWidget::loadYears()
{
    const int currentYear = m_yearCombo->currentData().toInt();

    m_yearCombo->blockSignals(true);

    m_yearCombo->clear();

    m_yearCombo->addItem("All Years", 0);

    SetCatalogRepository repository;

    const QList<int> years = repository.getYears();

    for (int year : years) {
        m_yearCombo->addItem(QString::number(year), year);
    }

    const int index = m_yearCombo->findData(currentYear);

    if (index >= 0) {
        m_yearCombo->setCurrentIndex(index);
    }

    m_yearCombo->blockSignals(false);
}

void SetsCatalogWidget::searchSets()
{
    SetCatalogSearchCriteria criteria;

    criteria.searchText = m_searchEdit->text().trimmed();

    criteria.year = m_yearCombo->currentData().toInt();

    const int resultsPerPage = UserSettings::instance().resultsPerPage();

    criteria.limit = resultsPerPage;

    criteria.offset = m_currentPage * resultsPerPage;

    SetCatalogRepository repository;

    m_totalResultCount = repository.count(criteria);

    const QList<SetCatalogItem> results = repository.search(criteria);

    m_lastResultCount = results.size();

    m_resultsTable->setRowCount(0);

    int row = 0;

    for (const SetCatalogItem& set : results) {
        m_resultsTable->insertRow(row);

        auto* setNumberItem = new QTableWidgetItem(set.setNumber());

        setNumberItem->setData(Qt::UserRole, set.id());

        auto* nameItem = new QTableWidgetItem(set.name());

        auto* yearItem = new QTableWidgetItem(QString::number(set.year()));

        auto* themeItem = new QTableWidgetItem(QString::number(set.themeId()));

        auto* partsItem = new QTableWidgetItem(QString::number(set.numberOfParts()));

        auto* actionCombo = new QComboBox(m_resultsTable);

        actionCombo->addItem("Actions...", QString());

        actionCombo->addItem("Details...", "details");

        actionCombo->addItem("Create Build from Stock", "Stock");

        actionCombo->addItem("Add as Complete Set", "CompleteSet");

        yearItem->setTextAlignment(Qt::AlignCenter);

        themeItem->setTextAlignment(Qt::AlignCenter);

        partsItem->setTextAlignment(Qt::AlignCenter);

        m_resultsTable->setItem(row, 0, setNumberItem);

        m_resultsTable->setItem(row, 1, nameItem);

        m_resultsTable->setItem(row, 2, yearItem);

        m_resultsTable->setItem(row, 3, themeItem);

        m_resultsTable->setItem(row, 4, partsItem);

        m_resultsTable->setCellWidget(row, 5, actionCombo);

        const int setCatalogId = set.id();

        connect(actionCombo,
                &QComboBox::currentIndexChanged,
                this,
                [this, actionCombo, setCatalogId](int index) {
                    if (index <= 0)
                        return;

                    const QString action = actionCombo->itemData(index).toString();

                    actionCombo->setCurrentIndex(0);

                    if (action == "details") {
                        SetDetailsDialog dialog(setCatalogId, this);

                        dialog.exec();

                        return;
                    }

                    if (action == "Stock" || action == "CompleteSet") {
                        emit createBuildRequested(setCatalogId, action);
                    }
                });

        ++row;
    } // <-- closes the for loop

    //
    // These operate on the complete result set,
    // so they belong outside the for loop.
    //
    if (results.isEmpty()) {
        if (m_currentPage > 0) {
            m_resultLabel->setText("No more matching sets.");
        } else {
            const bool hasSearchText = !criteria.searchText.isEmpty();
            const bool hasYearFilter = criteria.year > 0;

            if (hasSearchText || hasYearFilter) {
                m_resultLabel->setText("No sets match the current search or filters.");
            } else {
                m_resultLabel->setText("No sets are available in the Sets Catalog.");
            }
        }
    } else {
        const int firstResult = criteria.offset + 1;

        const int lastResult = criteria.offset + results.size();

        m_resultLabel->setText(QString("Showing results %1 - %2.").arg(firstResult).arg(lastResult));
    }

    updatePagingControls();
}
void SetsCatalogWidget::previousPage()
{
    if (m_currentPage <= 0)
        return;

    --m_currentPage;

    searchSets();
}

void SetsCatalogWidget::nextPage()
{
    const int resultsPerPage = UserSettings::instance().resultsPerPage();
    const int totalPages = qMax(1, (m_totalResultCount + resultsPerPage - 1) / resultsPerPage);

    if (m_currentPage + 1 >= totalPages)
        return;

    ++m_currentPage;

    searchSets();
}

void SetsCatalogWidget::updatePagingControls()
{
    const int resultsPerPage = UserSettings::instance().resultsPerPage();
    const int totalPages = qMax(1, (m_totalResultCount + resultsPerPage - 1) / resultsPerPage);
    const int displayPage = qMin(m_currentPage + 1, totalPages);

    m_previousButton->setEnabled(m_currentPage > 0);
    m_nextButton->setEnabled(m_currentPage + 1 < totalPages);

    m_summaryLabel->setText(
        QString("%1 %2")
            .arg(QLocale().toString(m_totalResultCount))
            .arg(m_totalResultCount == 1 ? "Set" : "Sets"));

    m_pageLabel->setText(QString("Page %1 of %2").arg(displayPage).arg(totalPages));
}

void SetsCatalogWidget::importSetsCsv()
{
    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          "Import Rebrickable sets.csv",
                                                          QString(),
                                                          "CSV Files (*.csv)");

    if (fileName.isEmpty())
        return;

    const QMessageBox::StandardButton response
        = QMessageBox::question(this,
                                "Import Sets Catalog",
                                "Import/update the BrickSuite Sets Catalog "
                                "from this Rebrickable sets.csv file?\n\n"
                                "Existing sets will be updated when provider "
                                "data changes. Existing BrickSuite records "
                                "will not be deleted.",
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::No);

    if (response != QMessageBox::Yes)
        return;

    RebrickableSetCatalogImporter importer;

    const RebrickableSetCatalogImporter::Result result = importer.importFile(fileName);

    if (!result.success) {
        QMessageBox::critical(this, "Import Sets Catalog", result.message);

        return;
    }

    QMessageBox::information(this,
                             "Import Sets Catalog",
                             QString("Sets Catalog import completed.\n\n"
                                     "Rows Read: %1\n"
                                     "New: %2\n"
                                     "Updated: %3\n"
                                     "Unchanged: %4\n"
                                     "Skipped: %5")
                                 .arg(result.rowsRead)
                                 .arg(result.inserted)
                                 .arg(result.updated)
                                 .arg(result.unchanged)
                                 .arg(result.skipped));

    refresh();
}