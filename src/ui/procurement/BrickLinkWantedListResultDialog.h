/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include <QDialog>

class QLabel;
class QPlainTextEdit;

class BrickLinkWantedListResultDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BrickLinkWantedListResultDialog(
        const QString& xml,
        int itemRows,
        int totalPieces,
        const QString& buildNumber,
        const QString& buildName,
        QWidget* parent = nullptr);

private:
    QString defaultFileName() const;
    void copyXml();
    void saveXml();
    void openBrickLinkUpload();

    QString m_xml;
    QString m_buildNumber;
    QString m_buildName;

    QLabel* m_statusLabel = nullptr;
    QPlainTextEdit* m_xmlEdit = nullptr;
};
