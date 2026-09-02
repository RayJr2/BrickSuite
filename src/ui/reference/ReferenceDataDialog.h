#pragma once

#include <QDialog>

class QPushButton;
class QTableWidget;

class ReferenceDataDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ReferenceDataDialog(QWidget* parent = nullptr);

signals:
    void manufacturersChanged();

private:
    int selectedManufacturerId() const;
    void reload();
    void addManufacturer();
    void editManufacturer();
    void changeActiveState(bool active);
    void updateActions();

    QTableWidget* m_table = nullptr;
    QPushButton* m_edit = nullptr;
    QPushButton* m_activate = nullptr;
    QPushButton* m_deactivate = nullptr;
};
