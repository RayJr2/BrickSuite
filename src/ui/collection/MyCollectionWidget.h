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

class MyCollectionWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MyCollectionWidget(WorkspaceContext& workspaceContext, QWidget* parent = nullptr);
    void refresh();

private:
    void loadLocations();
    void loadPage(bool criteriaChanged = false);
    void updatePaging();
    void handleAction(int itemId, bool active, const QString& action);
    QString effectiveCriteriaKey() const;

    WorkspaceContext& m_workspaceContext;
    SetImageService* m_setImages = nullptr;
    MinifigImageService* m_minifigImages = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_typeCombo = nullptr;
    QComboBox* m_stateCombo = nullptr;
    QComboBox* m_locationCombo = nullptr;
    QComboBox* m_activeCombo = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_pageLabel = nullptr;
    QLabel* m_messageLabel = nullptr;
    QPushButton* m_previousButton = nullptr;
    QPushButton* m_nextButton = nullptr;
    int m_page = 0;
    int m_total = 0;
    QString m_loadedCriteriaKey;
};
