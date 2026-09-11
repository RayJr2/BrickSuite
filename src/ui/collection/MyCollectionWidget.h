#pragma once

#include <QWidget>
#include <QPointer>

class WorkspaceContext;
class MinifigImageService;
class SetImageService;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class CollectionApplicationService;
class RemoteReadApplicationServices;
class RemoteCollectionMutationApplicationService;
class QDialog;
namespace RemoteReadDto { struct CollectionSummary; struct CollectionDetail; }

class MyCollectionWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MyCollectionWidget(WorkspaceContext& workspaceContext,
                                CollectionApplicationService& collectionService,
                                RemoteReadApplicationServices* remoteReads = nullptr,
                                RemoteCollectionMutationApplicationService* remoteMutations = nullptr,
                                QWidget* parent = nullptr);
    void refresh();
    void refreshRemoteCurrentPage(bool refreshLocations = false);
    void refreshRemoteLocations();
    void selectCollectionItem(int collectionItemId);
    void setRemoteSessionConnected(bool connected);

signals:
    void remoteCollectionRefreshFinished(bool succeeded);
    void remoteLocationsRefreshFinished(bool succeeded);
    void hostCollectionMutationCommitted(int workspaceId, int collectionItemId);

private:
    void loadLocations();
    void loadPage(bool criteriaChanged = false, const QString& loadingMessage = QString());
    void updatePaging();
    void requestRemotePage();
    void populateRemotePage(const QList<RemoteReadDto::CollectionSummary>& rows);
    void showRemoteDetails(int itemId);
    void openRemoteMutation(const QString& operation, int itemId);
    void handleAction(int itemId, bool active, const QString& action);
    QString effectiveCriteriaKey() const;

    WorkspaceContext& m_workspaceContext;
    CollectionApplicationService& m_collectionService;
    RemoteReadApplicationServices* m_remoteReads = nullptr;
    RemoteCollectionMutationApplicationService* m_remoteMutations = nullptr;
    QPointer<QDialog> m_remoteMutationDialog;
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
    quint64 m_collectionRequestToken = 0;
    quint64 m_storageRequestToken = 0;
    quint64 m_detailRequestToken = 0;
};
