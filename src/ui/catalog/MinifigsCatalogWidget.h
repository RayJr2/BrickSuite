#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class MinifigImageService;

class MinifigsCatalogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MinifigsCatalogWidget(QWidget* parent = nullptr);
    void refresh();

private slots:
    void searchMinifigs();
    void previousPage();
    void nextPage();
    void importMinifigs();
    void displayImage(const QString& minifigNumber, const QString& imagePath);

private:
    void updatePagingControls();

    int m_currentPage = 0;
    int m_totalResultCount = 0;
    QString m_loadedSearchText;
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_searchButton = nullptr;
    QPushButton* m_importButton = nullptr;
    QLabel* m_resultLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QTableWidget* m_resultsTable = nullptr;
    QPushButton* m_previousButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QLabel* m_pageLabel = nullptr;
    MinifigImageService* m_imageService = nullptr;
};
