/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "../../models/procurement/BrickLinkWantedListOptions.h"
#include "../../models/procurement/ProcurementDraft.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

class ProcurementPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProcurementPreviewDialog(const ProcurementDraft& draft,
                                      QWidget* parent = nullptr);

    const ProcurementDraft& draft() const;
    BrickLinkWantedListOptions brickLinkOptions() const;

private:
    struct BrickLinkColorChoice
    {
        QString externalId;
        QString displayName;
    };

    void buildUi();
    void populateRows();
    QList<BrickLinkColorChoice> loadMappedBrickLinkColors() const;

    void updatePartRow(int row);
    void updateColorRow(int row);
    void updateRowStatus(int row);
    void updateSummary();
    bool persistRememberedPartOverrides();
    void generateBrickLinkXml();

    ProcurementDraft m_draft;

    QLabel* m_buildLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;

    QComboBox* m_conditionCombo = nullptr;
    QComboBox* m_notifyCombo = nullptr;
    QComboBox* m_wantedShowCombo = nullptr;
    QComboBox* m_remarksCombo = nullptr;
    QLineEdit* m_customRemarksEdit = nullptr;

    QTableWidget* m_table = nullptr;
    QList<QCheckBox*> m_rememberPartOverrideChecks;
    QPushButton* m_generateButton = nullptr;
};
