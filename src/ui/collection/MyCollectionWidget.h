#pragma once

#include <QWidget>
#include <QPointer>
#include <functional>
#include "../../models/CollectionSearchCriteria.h"
#include "../../models/export/CollectionExportTypes.h"

class WorkspaceContext;
class MinifigImageService;
class SetImageService;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class CollectionApplicationService;
class SessionStorageSelectionService;
class RemoteReadApplicationServices;
class RemoteCollectionMutationApplicationService;
class QDialog;
namespace RemoteReadDto { struct CollectionSearchRequest; struct CollectionSummary; struct CollectionDetail; struct CollectionDisassemblyPlan; }
namespace RemoteCollectionMutationDto { struct DisassemblyReturn; }

class MyCollectionWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MyCollectionWidget(WorkspaceContext& workspaceContext,
                                SessionStorageSelectionService& sessionStorageSelectionService,
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
    void localCollectionDisassembled(int workspaceId, int collectionItemId, int buildId);
    void statusMessageRequested(const QString& message, int timeoutMs = 0);

private:
    void loadLocations();
    void loadPage(bool criteriaChanged = false, const QString& loadingMessage = QString());
    void updatePaging();
    void requestRemotePage();
    void populateRemotePage(const QList<RemoteReadDto::CollectionSummary>& rows);
    void showRemoteDetails(int itemId);
    void openRemoteMutation(const QString& operation, int itemId);
    void openRemoteDisassembly(int itemId);
    void submitRemoteDisassembly(
        const RemoteReadDto::CollectionDisassemblyPlan& plan,
        const QList<RemoteCollectionMutationDto::DisassemblyReturn>& returns);
    void handleAction(int itemId, bool active, const QString& action);
    QString effectiveCriteriaKey() const;
    CollectionSearchCriteria currentCriteria(int pageSize=100,int offset=0) const;
    RemoteReadDto::CollectionSearchRequest currentRemoteCriteria(int page) const;
    QString currentFilterSummary() const;
    void exportCsv();
    void requestRemoteCollectionExport(QObject* context,
        std::function<void(bool,QList<CollectionExportRow>,QString)> completion);
    void showCollectionExport(QList<CollectionExportRow> rows,const QString& summary);

    WorkspaceContext& m_workspaceContext;
    SessionStorageSelectionService& m_sessionStorageSelectionService;
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
    QPushButton* m_exportButton = nullptr;
    int m_page = 0;
    int m_total = 0;
    QString m_loadedCriteriaKey;
    bool m_refreshInProgress = false;
    bool m_exportPreparationActive = false;
    quint64 m_collectionRequestToken = 0;
    quint64 m_storageRequestToken = 0;
    quint64 m_detailRequestToken = 0;
};
