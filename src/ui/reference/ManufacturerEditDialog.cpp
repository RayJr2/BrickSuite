#include "ManufacturerEditDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

ManufacturerEditDialog::ManufacturerEditDialog(const Manufacturer* manufacturer, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(manufacturer ? "Edit Manufacturer" : "Add Manufacturer");
    if (manufacturer)
        m_original = *manufacturer;
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    m_name = new QLineEdit(manufacturer ? manufacturer->name() : QString(), this);
    m_code = new QLineEdit(manufacturer ? manufacturer->code() : QString(), this);
    m_website = new QLineEdit(manufacturer ? manufacturer->websiteUrl() : QString(), this);
    m_elementIds = new QCheckBox("Supports LEGO Element IDs", this);
    m_elementIds->setChecked(manufacturer && manufacturer->supportsLegoElementIds());
    m_notes = new QTextEdit(manufacturer ? manufacturer->notes() : QString(), this);
    m_notes->setMaximumHeight(100);
    form->addRow("Name:", m_name);
    form->addRow("Code:", m_code);
    form->addRow("Website URL:", m_website);
    form->addRow(QString(), m_elementIds);
    form->addRow("Notes:", m_notes);
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (m_name->text().trimmed().isEmpty() || m_code->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Manufacturer", "Name and Code are required.");
            return;
        }
        accept();
    });
}

Manufacturer ManufacturerEditDialog::manufacturer() const
{
    Manufacturer value = m_original;
    value.setName(m_name->text());
    value.setCode(m_code->text());
    value.setWebsiteUrl(m_website->text());
    value.setSupportsLegoElementIds(m_elementIds->isChecked());
    value.setNotes(m_notes->toPlainText());
    return value;
}
