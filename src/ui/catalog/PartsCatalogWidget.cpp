#include "PartsCatalogWidget.h"

#include "../../models/Part.h"
#include "../../models/PartCategory.h"
#include "../../models/PartSearchCriteria.h"
#include "../../models/PartSearchResult.h"
#include "../../repositories/PartCategoryRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../settings/UserSettings.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

PartsCatalogWidget::PartsCatalogWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* titleLabel = new QLabel("Parts Catalog", this);

    auto* filterLayout = new QHBoxLayout();

    m_searchEdit = new QLineEdit(this);

    m_searchEdit->setPlaceholderText("Search by part number or name");

    m_categoryCombo = new QComboBox(this);

    m_searchButton = new QPushButton("Search", this);

    filterLayout->addWidget(new QLabel("Search:", this));

    filterLayout->addWidget(m_searchEdit, 2);

    filterLayout->addWidget(new QLabel("Category:", this));

    filterLayout->addWidget(m_categoryCombo, 1);

    filterLayout->addWidget(m_searchButton);

    m_resultLabel = new QLabel(this);

    m_resultsTable = new QTableWidget(this);

    m_resultsTable->setColumnCount(5);

    m_resultsTable->setHorizontalHeaderLabels(QStringList() << "Part #"
                                                            << "Name"
                                                            << "Category"
                                                            << "Material"
                                                            << "Action");

    m_resultsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_resultsTable->setSelectionMode(QAbstractItemView::SingleSelection);

    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_resultsTable->verticalHeader()->setVisible(false);

    m_resultsTable->horizontalHeader()->setStretchLastSection(false);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    m_resultsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    // Pagination controls
    m_previousButton = new QPushButton("Previous", this);

    m_nextButton = new QPushButton("Next", this);

    m_pageLabel = new QLabel(this);

    auto* pagingLayout = new QHBoxLayout();

    pagingLayout->addStretch();

    pagingLayout->addWidget(m_previousButton);

    pagingLayout->addWidget(m_pageLabel);

    pagingLayout->addWidget(m_nextButton);

    mainLayout->addWidget(titleLabel);

    mainLayout->addLayout(filterLayout);

    mainLayout->addWidget(m_resultLabel);

    mainLayout->addWidget(m_resultsTable);

    mainLayout->addLayout(pagingLayout);

    connect(m_searchButton, &QPushButton::clicked, this, [this]() {
        m_currentPage = 0;
        searchParts();
    });

    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        m_currentPage = 0;
        searchParts();
    });

    connect(m_categoryCombo, &QComboBox::currentIndexChanged, this, [this]() {
        m_currentPage = 0;
        searchParts();
    });

    connect(m_previousButton, &QPushButton::clicked, this, &PartsCatalogWidget::previousPage);

    connect(m_nextButton, &QPushButton::clicked, this, &PartsCatalogWidget::nextPage);

    loadCategories();

    m_resultLabel->setText("Enter search criteria or select a category.");

    updatePagingControls();
}

void PartsCatalogWidget::loadCategories()
{
    m_categoryCombo->clear();

    m_categoryCombo->addItem("All Categories", 0);

    PartCategoryRepository repository;

    const QList<PartCategory> categories = repository.getAll();

    for (const PartCategory& category : categories) {
        m_categoryCombo->addItem(category.name(), category.id());
    }
}

void PartsCatalogWidget::searchParts()
{
    PartSearchCriteria criteria;

    criteria.searchText = m_searchEdit->text().trimmed();

    criteria.categoryId = m_categoryCombo->currentData().toInt();

    const int resultsPerPage = UserSettings::instance().resultsPerPage();

    criteria.limit = resultsPerPage;

    criteria.offset = m_currentPage * resultsPerPage;

    PartRepository repository;

    const QList<PartSearchResult> results = repository.search(criteria);

    m_lastResultCount = results.size();

    m_resultsTable->setRowCount(0);

    int row = 0;

    for (const PartSearchResult& result : results) {
        const Part& part = result.part;

        m_resultsTable->insertRow(row);

        auto* partNumberItem = new QTableWidgetItem(part.partNumber());

        partNumberItem->setData(Qt::UserRole, part.id());

        auto* nameItem = new QTableWidgetItem(part.name());

        auto* categoryItem = new QTableWidgetItem(result.categoryName);

        auto* materialItem = new QTableWidgetItem(part.material());

        auto* addButton = new QPushButton("Add to Inventory", m_resultsTable);

        const int partId = part.id();

        connect(addButton, &QPushButton::clicked, this, [this, partId]() {
            emit addPartToInventoryRequested(partId);
        });

        m_resultsTable->setItem(row, 0, partNumberItem);

        m_resultsTable->setItem(row, 1, nameItem);

        m_resultsTable->setItem(row, 2, categoryItem);

        m_resultsTable->setItem(row, 3, materialItem);

        m_resultsTable->setCellWidget(row, 4, addButton);

        ++row;
    }

    if (results.isEmpty()) {
        if (m_currentPage > 0) {
            m_resultLabel->setText("No more matching parts.");
        } else {
            m_resultLabel->setText("No matching parts found.");
        }
    } else {
        const int firstResult = criteria.offset + 1;

        const int lastResult = criteria.offset + results.size();

        m_resultLabel->setText(QString("Showing results %1 - %2.").arg(firstResult).arg(lastResult));
    }

    updatePagingControls();
}

void PartsCatalogWidget::previousPage()
{
    if (m_currentPage <= 0)
        return;

    --m_currentPage;

    searchParts();
}

void PartsCatalogWidget::nextPage()
{
    if (m_lastResultCount < ResultsPerPage) {
        return;
    }

    ++m_currentPage;

    searchParts();

    // If the next page is empty,
    // return to the last valid page.
    if (m_lastResultCount == 0) {
        --m_currentPage;

        searchParts();
    }
}

void PartsCatalogWidget::settingsChanged()
{
    m_currentPage = 0;

    searchParts();
}

void PartsCatalogWidget::updatePagingControls()
{
    m_previousButton->setEnabled(m_currentPage > 0);

    const int resultsPerPage = UserSettings::instance().resultsPerPage();

    m_nextButton->setEnabled(m_lastResultCount >= resultsPerPage);

    m_pageLabel->setText(QString("Page %1").arg(m_currentPage + 1));
}