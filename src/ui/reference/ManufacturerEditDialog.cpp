/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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
