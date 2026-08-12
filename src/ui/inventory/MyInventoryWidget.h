#pragma once

#include <QHash>
#include <QWidget>

class WorkspaceContext;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QLabel;

class MyInventoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MyInventoryWidget(
        WorkspaceContext& workspaceContext,
        QWidget* parent = nullptr);

    void refresh();

private slots:
    void workspaceChanged(int workspaceId);
    void searchInventory();
    void previousPage();
    void nextPage();
    void importCsv();

private:
    void loadCategories();
    void loadColors();
    void loadStorageLocations();
    void updatePagingControls();

    QString storagePathForId(int storageLocationId) const;

    static constexpr int ResultsPerPage = 250;

    WorkspaceContext& m_workspaceContext;

    int m_currentPage = 0;
    int m_lastResultCount = 0;

    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_categoryCombo = nullptr;
    QComboBox* m_colorCombo = nullptr;
    QComboBox* m_storageCombo = nullptr;

    QPushButton* m_searchButton = nullptr;

    QTableWidget* m_resultsTable = nullptr;
    QLabel* m_resultLabel = nullptr;

    QPushButton* m_previousButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    QLabel* m_pageLabel = nullptr;

    QPushButton* m_importButton = nullptr;

    QHash<int, QString> m_storagePathById;
};