#pragma once

#include <QDialog>

class QLabel;
class MinifigImageService;

class MinifigDetailsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MinifigDetailsDialog(int minifigCatalogId, QWidget* parent = nullptr);

private:
    bool loadMinifig();
    void displayImage(const QString& imagePath);

    int m_minifigCatalogId = 0;
    QString m_minifigNumber;
    QString m_imageUrl;
    QLabel* m_imageLabel = nullptr;
    QLabel* m_imageStatusLabel = nullptr;
    QLabel* m_numberLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_partsLabel = nullptr;
    QLabel* m_providerLabel = nullptr;
    QLabel* m_sourceLabel = nullptr;
    MinifigImageService* m_imageService = nullptr;
};
