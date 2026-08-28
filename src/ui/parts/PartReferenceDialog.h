/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 */

#pragma once

#include <QDialog>
#include <QHash>
#include <QList>
#include <QPair>
#include <QStringList>

class QCloseEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;
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

    struct PageDefinition
    {
        QString name;
        bool dimensionGrid = false;
        QList<int> rebrickableCategoryIds;
        QList<DimensionEntry> dimensionEntries;
        int galleryLimit = 160;
        QWidget* page = nullptr;
        bool loaded = false;
    };

    struct GroupDefinition
    {
        QString name;
        QTabWidget* tabs = nullptr;
        QList<PageDefinition> pages;
    };

    void initializeUi();
    void initializeDefinitions();
    void restoreUiState();
    void saveUiState();

    PageDefinition* currentPageDefinition();
    void ensureCurrentPageLoaded();
    void buildDimensionPage(PageDefinition& definition);
    void buildGalleryPage(PageDefinition& definition);

    QList<int> localCategoryIds(const QList<int>& rebrickableCategoryIds) const;
    bool partMatchesCategories(int partCategoryId, const QList<int>& localCategoryIds) const;
    bool isPrintedPartNumber(const QString& partNumber) const;

    QToolButton* createPartCard(QWidget* parent,
                                const QString& partNumber,
                                const QString& partName);
    void setCardImage(const QString& partNumber, const QString& imagePath);
    void requestMissingImages(const QStringList& partNumbers);
    void applySearchFilter();
    void selectPart(const QString& partNumber, const QString& partName);

    static QList<DimensionEntry> makeDimensionEntries(
        const QStringList& columnLabels,
        const QList<QPair<QString, QStringList>>& rows);

    QLineEdit* m_searchEdit = nullptr;
    QTabWidget* m_groupTabs = nullptr;
    QLabel* m_selectedLabel = nullptr;
    QPushButton* m_copyButton = nullptr;
    QPushButton* m_sendButton = nullptr;

    QList<GroupDefinition> m_groups;

    QString m_selectedPartNumber;
    QString m_selectedPartName;
    bool m_addInventoryAvailable = false;

    QHash<QString, QList<QToolButton*>> m_cardsByPartNumber;
    QList<QToolButton*> m_currentCards;

    PartImageService* m_partImageService = nullptr;
    RebrickableApiClient* m_rebrickableApiClient = nullptr;

    static constexpr int ImageBatchSize = 20;
};
