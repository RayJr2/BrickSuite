#pragma once

#include <QDialog>

#include "../../services/RebrickableApiClient.h"

class QLabel;
class QListWidget;
class QPushButton;
class QTableWidget;
class QTabWidget;

class PartImageService;
class RebrickableApiClient;

class PartDetailsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PartDetailsDialog(
        int partId,
        QWidget* parent = nullptr);

private:
    bool loadLocalPart();
    void loadCachedImage();
    void requestRebrickableDetails();

    void populateRelatedParts(
        const RebrickableApiClient::PartDetailsResult& result);

    void populateRelatedList(QListWidget* listWidget, const QStringList& partNumbers);

    void populateExternalIds(
        const RebrickableApiClient::PartDetailsResult& result);

    int m_partId = 0;

    QString m_partNumber;

    QLabel* m_imageLabel = nullptr;

    QLabel* m_partNumberLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_categoryLabel = nullptr;
    QLabel* m_materialLabel = nullptr;
    QLabel* m_yearsLabel = nullptr;
    QLabel* m_rebrickableStatusLabel = nullptr;

    QTabWidget* m_tabWidget = nullptr;

    QListWidget* m_moldsList = nullptr;
    QListWidget* m_alternatesList = nullptr;
    QListWidget* m_printsList = nullptr;

    QTableWidget* m_externalIdsTable = nullptr;

    QPushButton* m_openRebrickableButton = nullptr;
    QPushButton* m_closeButton = nullptr;

    QString m_rebrickableUrl;

    RebrickableApiClient* m_rebrickableApiClient = nullptr;
    PartImageService* m_partImageService = nullptr;
};