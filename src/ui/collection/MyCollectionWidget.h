#pragma once

#include <QWidget>

class WorkspaceContext;
class MinifigImageService;
class SetImageService;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class CollectionApplicationService;

class MyCollectionWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MyCollectionWidget(WorkspaceContext& workspaceContext,
                                CollectionApplicationService& collectionService,
                                QWidget* parent = nullptr);
    void refresh();
    void selectCollectionItem(int collectionItemId);

private:
    void loadLocations();
    void loadPage(bool criteriaChanged = false, const QString& loadingMessage = QString());
    void updatePaging();
    void handleAction(int itemId, bool active, const QString& action);
    QString effectiveCriteriaKey() const;

    WorkspaceContext& m_workspaceContext;
    CollectionApplicationService& m_collectionService;
    SetImageService* m_setImages = nullptr;
    MinifigImageService* m_minifigImages = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_typeCombo = nullptr;
    QComboBox* m_stateCombo = nullptr;
    QComboBox* m_conditionCombo = nullptr;
    QComboBox* m_completenessCombo = nullptr;
    QComboBox* m_locationCombo = nullptr;
    QComboBox* m_activeCombo = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_pageLabel = nullptr;
    QLabel* m_messageLabel = nullptr;
    QPushButton* m_searchButton = nullptr;
    QPushButton* m_previousButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    int m_page = 0;
    int m_total = 0;
    QString m_loadedCriteriaKey;
    bool m_refreshInProgress = false;
};
