#pragma once

#include <QDialog>

class QLabel;
class MinifigImageService;
class PartImageService;
class RebrickableMinifigPartsService;
class QPushButton;
class QTableWidget;
class WorkspaceContext;

class MinifigDetailsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MinifigDetailsDialog(int minifigCatalogId, WorkspaceContext& workspaceContext,
                                  QWidget* parent = nullptr);

signals:
    void createBuildRequested(int minifigCatalogId, const QString& buildName);
    void collectionItemCreated(int collectionItemId);

private:
    bool loadMinifig();
    void loadComposition();
    void importPartsList();
    void getPartsFromRebrickable();
    void setCompositionActionsEnabled(bool enabled);
    void createBuildFromStock();
    void addToCollection();
    void displayImage(const QString& imagePath);

    int m_minifigCatalogId = 0;
    WorkspaceContext& m_workspaceContext;
    QString m_minifigNumber;
    QString m_minifigName;
    QString m_imageUrl;
    QLabel* m_imageLabel = nullptr;
    QLabel* m_imageStatusLabel = nullptr;
    QLabel* m_numberLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_partsLabel = nullptr;
    QLabel* m_providerLabel = nullptr;
    QLabel* m_sourceLabel = nullptr;
    QLabel* m_compositionSummaryLabel = nullptr;
    QTableWidget* m_compositionTable = nullptr;
    QPushButton* m_importPartsButton = nullptr;
    QPushButton* m_getPartsButton = nullptr;
    QPushButton* m_createBuildButton = nullptr;
    QPushButton* m_addToCollectionButton = nullptr;
    int m_requiredPieces = 0;
    int m_sparePieces = 0;
    MinifigImageService* m_imageService = nullptr;
    PartImageService* m_partImageService = nullptr;
    RebrickableMinifigPartsService* m_rebrickablePartsService = nullptr;
};
