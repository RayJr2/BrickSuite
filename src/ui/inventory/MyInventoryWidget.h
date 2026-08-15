#pragma once

#include <QHash>
#include <QList>
#include <QSet>
#include <QWidget>

class WorkspaceContext;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QLabel;
class PartImageService;
class RebrickableApiClient;

class MyInventoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MyInventoryWidget(
        WorkspaceContext& workspaceContext,
        QWidget* parent = nullptr);

    void refresh();
    void settingsChanged();

    void updatePartColorImage(const QString& partNumber,
                              int rebrickableColorId,
                              const QString& imagePath);

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
    void addPart();

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

    QPushButton* m_addPartButton = nullptr;

    QHash<int, QString> m_storagePathById;

    PartImageService* m_partImageService = nullptr;
    RebrickableApiClient* m_rebrickableApiClient = nullptr;

    QHash<QString, QList<int>> m_rowsByPartNumber;
    QSet<QString> m_partDetailsRequested;

    QString partColorKey(const QString& partNumber, int rebrickableColorId) const;
    QHash<QString, QList<int>> m_rowsByPartColor;
};