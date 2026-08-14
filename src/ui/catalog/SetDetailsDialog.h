#pragma once

#include <QDialog>

class QLabel;
class SetImageService;

class SetDetailsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SetDetailsDialog(int setCatalogId, QWidget* parent = nullptr);

private:
    bool loadSet();
    void loadCachedImage();
    void requestImage();

    int m_setCatalogId = 0;

    QString m_setNumber;
    QString m_imageUrl;

    QLabel* m_imageLabel = nullptr;

    QLabel* m_setNumberLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_yearLabel = nullptr;
    QLabel* m_themeLabel = nullptr;
    QLabel* m_partsLabel = nullptr;
    QLabel* m_imageStatusLabel = nullptr;

    SetImageService* m_imageService = nullptr;
};
