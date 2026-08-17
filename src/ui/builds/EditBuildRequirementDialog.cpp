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

#include "EditBuildRequirementDialog.h"

#include "../../models/BuildRequirement.h"
#include "../../models/Color.h"
#include "../../models/Part.h"

#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/PartRepository.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <optional>

EditBuildRequirementDialog::EditBuildRequirementDialog(int requirementId, QWidget* parent)
    : QDialog(parent)
    , m_requirementId(requirementId)
{
    setWindowTitle("Edit Build Requirement");

    setModal(true);

    resize(420, 220);

    auto* mainLayout = new QVBoxLayout(this);

    auto* formLayout = new QFormLayout();

    m_partLabel = new QLabel(this);

    m_colorLabel = new QLabel(this);

    m_quantitySpin = new QSpinBox(this);

    m_quantitySpin->setRange(1, 99999);

    m_spareCheck = new QCheckBox("Spare part", this);

    formLayout->addRow("Part:", m_partLabel);

    formLayout->addRow("Color:", m_colorLabel);

    formLayout->addRow("Qty Required:", m_quantitySpin);

    formLayout->addRow(QString(), m_spareCheck);

    mainLayout->addLayout(formLayout);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);

    mainLayout->addWidget(buttonBox);

    connect(buttonBox,
            &QDialogButtonBox::accepted,
            this,
            &EditBuildRequirementDialog::saveRequirement);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadRequirement();
}

void EditBuildRequirementDialog::loadRequirement()
{
    BuildRequirementRepository requirementRepository;

    const std::optional<BuildRequirement> requirement = requirementRepository.getById(
        m_requirementId);

    if (!requirement) {
        QMessageBox::critical(this, "BrickSuite", "Unable to load the build requirement.");

        reject();

        return;
    }

    PartRepository partRepository;

    ColorRepository colorRepository;

    const std::optional<Part> part = partRepository.getById(requirement->partId());

    const std::optional<Color> color = colorRepository.getById(requirement->colorId());

    if (part) {
        m_partLabel->setText(QString("%1 — %2").arg(part->partNumber(), part->name()));
    } else {
        m_partLabel->setText("(Part unavailable)");
    }

    if (color) {
        m_colorLabel->setText(color->name());
    } else {
        m_colorLabel->setText("(Color unavailable)");
    }

    m_quantitySpin->setValue(requirement->quantityRequired());

    m_spareCheck->setChecked(requirement->isSpare());
}

void EditBuildRequirementDialog::saveRequirement()
{
    BuildRequirementRepository repository;

    std::optional<BuildRequirement> requirement = repository.getById(m_requirementId);

    if (!requirement) {
        QMessageBox::critical(this, "BrickSuite", "Unable to load the build requirement.");

        return;
    }

    requirement->setQuantityRequired(m_quantitySpin->value());

    requirement->setIsSpare(m_spareCheck->isChecked());

    if (!repository.update(*requirement)) {
        QMessageBox::critical(this, "BrickSuite", "Unable to update the build requirement.");

        return;
    }

    accept();
}