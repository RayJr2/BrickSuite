#pragma once

#include <QHash>
#include <QWidget>

class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QLabel;
class PartImageService;
class RebrickableApiClient;

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
    void importPartsCsv();

signals:
    void addPartToInventoryRequested(int partId);

private:
    void loadCategories();
    void updatePagingControls();
    void requestMissingPartImages(const QStringList& partNumbers);

    static constexpr int ResultsPerPage = 250;
    static constexpr int PartImageBatchSize = 20;

    int m_currentPage = 0;
    int m_lastResultCount = 0;
    int m_totalResultCount = 0;

    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_categoryCombo = nullptr;
    QPushButton* m_searchButton = nullptr;

    QTableWidget* m_resultsTable = nullptr;
    QLabel* m_resultLabel = nullptr;

    QPushButton* m_previousButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QLabel* m_pageLabel = nullptr;

    PartImageService* m_partImageService = nullptr;
    RebrickableApiClient* m_rebrickableApiClient = nullptr;

    QHash<QString, int> m_rowByPartNumber;

    QPushButton* m_importPartsButton = nullptr;
};