/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 */

#pragma once

#include "../../models/PartReferenceEntry.h"
#include "../../services/parts/PartReferenceManifest.h"

#include <QDialog>
#include <QHash>
#include <QList>
#include <QPair>
#include <QSet>
#include <QStringList>

class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QToolButton;
class QWidget;
class PartImageService;
class RebrickableApiClient;

class PartReferenceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PartReferenceDialog(QWidget* parent = nullptr);
    ~PartReferenceDialog() override;

    void setAddInventoryAvailable(bool available);

signals:
    void sendToAddInventoryRequested(const QString& partNumber);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    struct DimensionEntry
    {
        int row = 0;
        int column = 0;
        QString rowLabel;
        QString columnLabel;
        QString partNumber;
    };

    struct DimensionDefinition
    {
        QString catalog;
        QString section;
        QString title;
        QList<DimensionEntry> entries;
    };

    enum class ViewMode
    {
        Gallery,
        DimensionGrid
    };

    void initializeUi();
    void initializeDimensionDefinitions();
    void restoreUiState();
    void saveUiState();

    void populateCatalogList();
    void catalogSelectionChanged();
    void updateViewSelector();
    QString currentCatalog() const;
    ViewMode currentViewMode() const;

    QWidget* ensureCatalogPage(const QString& catalog, ViewMode mode);
    QWidget* buildCatalogGalleryPage(const QString& catalog);
    QWidget* buildCatalogDimensionPage(const QString& catalog);
    QWidget* buildSearchPage(const QList<PartReferenceEntry>& entries,
                             int totalMatches);
    QWidget* buildGalleryContent(const QList<PartReferenceEntry>& entries,
                                 QWidget* parent,
                                 bool includeSectionHeadings = true);

    bool catalogSupportsDimensionGrid(const QString& catalog) const;
    QList<DimensionDefinition> dimensionDefinitionsForCatalog(const QString& catalog) const;
    QList<PartReferenceEntry> entriesNotInDimensionGrid(
        const QList<PartReferenceEntry>& catalogEntries,
        const QList<DimensionDefinition>& definitions) const;

    QToolButton* createPartCard(QWidget* parent,
                                const QString& partNumber,
                                const QString& partName);
    void setCardImage(const QString& partNumber, const QString& imagePath);
    void loadCardImageOrQueue(const QString& partNumber, QSet<QString>& missingImages);
    void requestMissingImages(const QStringList& partNumbers);
    void refreshSearchResults();
    void showCurrentCatalogPage();
    void selectPart(const QString& partNumber, const QString& partName);
    void setCardSelected(QToolButton* card, bool selected);
    void setPartCardsSelected(const QString& partNumber, bool selected);
    void copySelectedPart();
    void sendSelectedPartToInventory();

    static QList<DimensionEntry> makeDimensionEntries(
        const QStringList& columnLabels,
        const QList<QPair<QString, QStringList>>& rows);
    static QString pageKey(const QString& catalog, ViewMode mode);

    QLineEdit* m_searchEdit = nullptr;
    QListWidget* m_catalogList = nullptr;
    QComboBox* m_viewCombo = nullptr;
    QLabel* m_catalogTitleLabel = nullptr;
    QLabel* m_catalogCountLabel = nullptr;
    QSplitter* m_splitter = nullptr;
    QStackedWidget* m_contentStack = nullptr;
    QLabel* m_selectedLabel = nullptr;
    QPushButton* m_copyButton = nullptr;
    QPushButton* m_sendButton = nullptr;

    QList<DimensionDefinition> m_dimensionDefinitions;
    QHash<QString, QWidget*> m_pagesByKey;
    QWidget* m_searchPage = nullptr;

    QString m_selectedPartNumber;
    QString m_selectedPartName;
    bool m_addInventoryAvailable = false;
    bool m_restoringUiState = false;

    QHash<QString, QList<QToolButton*>> m_cardsByPartNumber;

    PartReferenceManifest m_manifest;

    PartImageService* m_partImageService = nullptr;
    RebrickableApiClient* m_rebrickableApiClient = nullptr;

    static constexpr int ImageBatchSize = 20;
    static constexpr int GalleryColumns = 6;
    static constexpr int SearchDisplayLimit = 240;
};
