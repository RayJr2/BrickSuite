#pragma once

#include "../../models/Manufacturer.h"
#include <QDialog>

class QCheckBox;
class QLineEdit;
class QTextEdit;

class ManufacturerEditDialog : public QDialog
{
public:
    explicit ManufacturerEditDialog(const Manufacturer* manufacturer = nullptr,
                                    QWidget* parent = nullptr);
    Manufacturer manufacturer() const;

private:
    Manufacturer m_original;
    QLineEdit* m_name = nullptr;
    QLineEdit* m_code = nullptr;
    QLineEdit* m_website = nullptr;
    QCheckBox* m_elementIds = nullptr;
    QTextEdit* m_notes = nullptr;
};
