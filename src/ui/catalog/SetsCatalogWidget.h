#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

class SetsCatalogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SetsCatalogWidget(QWidget* parent = nullptr);

    void refresh();

private slots:
    void searchSets();
    void previousPage();
    void nextPage();
    void importSetsCsv();

signals:
    void createBuildRequested(int setCatalogId, const QString& inventoryMode);

private:
    void loadYears();
    void updatePagingControls();

    int m_currentPage = 0;
    int m_lastResultCount = 0;
    int m_totalResultCount = 0;

    QLineEdit* m_searchEdit = nullptr;

    QComboBox* m_yearCombo = nullptr;

    QPushButton* m_searchButton = nullptr;
    QPushButton* m_importButton = nullptr;

    QLabel* m_catalogLabel = nullptr;
    QLabel* m_resultLabel = nullptr;

    QTableWidget* m_resultsTable = nullptr;

    QPushButton* m_previousButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QLabel* m_pageLabel = nullptr;
};