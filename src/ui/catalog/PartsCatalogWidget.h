#pragma once

#include <QWidget>

class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QLabel;

class PartsCatalogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PartsCatalogWidget(QWidget* parent = nullptr);
    void settingsChanged();

private slots:
    void searchParts();
    void previousPage();
    void nextPage();

signals:
    void addPartToInventoryRequested(int partId);

private:
    void loadCategories();
    void updatePagingControls();

    static constexpr int ResultsPerPage = 250;

    int m_currentPage = 0;
    int m_lastResultCount = 0;

    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_categoryCombo = nullptr;
    QPushButton* m_searchButton = nullptr;

    QTableWidget* m_resultsTable = nullptr;
    QLabel* m_resultLabel = nullptr;

    QPushButton* m_previousButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QLabel* m_pageLabel = nullptr;
};