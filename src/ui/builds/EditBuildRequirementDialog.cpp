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
 */

#include "EditBuildRequirementDialog.h"

#include "../../models/Build.h"
#include "../../models/BuildRequirement.h"
#include "../../models/Color.h"
#include "../../models/Part.h"

#include "../../repositories/BuildAllocationRepository.h"
#include "../../repositories/BuildRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/PartRepository.h"

#include "../../ui/helpers/ColorComboHelper.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <optional>

EditBuildRequirementDialog::EditBuildRequirementDialog(int requirementId, QWidget* parent)
    : QDialog(parent)
    , m_requirementId(requirementId)
{
    setWindowTitle("Edit Build Requirement");
    setModal(true);
    resize(520, 360);

    auto* mainLayout = new QVBoxLayout(this);

    auto* originalGroup = new QGroupBox("Original Requirement", this);
    auto* originalLayout = new QFormLayout(originalGroup);

    m_originalPartLabel = new QLabel(originalGroup);
    m_originalPartLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_originalColorLabel = new QLabel(originalGroup);
    m_originalColorLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    originalLayout->addRow("Part:", m_originalPartLabel);
    originalLayout->addRow("Color:", m_originalColorLabel);

    mainLayout->addWidget(originalGroup);

    auto* useGroup = new QGroupBox("Use for this Build", this);
    auto* useLayout = new QFormLayout(useGroup);

    m_usePartEdit = new QLineEdit(useGroup);
    m_usePartEdit->setPlaceholderText("Part number");

    m_useColorCombo = new QComboBox(useGroup);

    m_quantitySpin = new QSpinBox(useGroup);
    m_quantitySpin->setRange(1, 99999);

    m_spareCheck = new QCheckBox("Spare part", useGroup);

    m_resetButton = new QPushButton("Reset to Original", useGroup);

    useLayout->addRow("Part:", m_usePartEdit);
    useLayout->addRow("Color:", m_useColorCombo);
    useLayout->addRow("Qty Required:", m_quantitySpin);
    useLayout->addRow(QString(), m_spareCheck);
    useLayout->addRow(QString(), m_resetButton);

    mainLayout->addWidget(useGroup);

    auto* noteLabel = new QLabel(
        "Changing Part or Color records a substitution for this Build. "
        "The original requirement is preserved.",
        this);
    noteLabel->setWordWrap(true);
    mainLayout->addWidget(noteLabel);

    auto* buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    connect(m_resetButton,
            &QPushButton::clicked,
            this,
            &EditBuildRequirementDialog::resetToOriginal);

    connect(buttonBox,
            &QDialogButtonBox::accepted,
            this,
            &EditBuildRequirementDialog::saveRequirement);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadColors();
    loadRequirement();
}

void EditBuildRequirementDialog::loadColors()
{
    m_useColorCombo->clear();

    ColorRepository repository;
    const QList<Color> colors = repository.getAll();

    for (const Color& color : colors) {
        ColorComboHelper::addColorItem(m_useColorCombo,
                                       color.name(),
                                       color.id(),
                                       color.rgb());
    }
}

void EditBuildRequirementDialog::loadRequirement()
{
    BuildRequirementRepository requirementRepository;

    const std::optional<BuildRequirement> requirement =
        requirementRepository.getById(m_requirementId);

    if (!requirement) {
        QMessageBox::critical(this, "BrickSuite", "Unable to load the build requirement.");
        reject();
        return;
    }

    BuildRepository buildRepository;
    const std::optional<Build> build = buildRepository.getById(requirement->buildId());

    if (!build) {
        QMessageBox::critical(this, "BrickSuite", "Unable to load the Build.");
        reject();
        return;
    }

    if (build->inventoryMode() != "Stock") {
        QMessageBox::information(
            this,
            "Edit Build Requirement",
            "Requirement substitutions are available only for Build from Stock.");
        reject();
        return;
    }

    m_originalPartId = requirement->partId();
    m_originalColorId = requirement->colorId();

    PartRepository partRepository;
    ColorRepository colorRepository;

    const std::optional<Part> originalPart =
        partRepository.getById(m_originalPartId);
    const std::optional<Color> originalColor =
        colorRepository.getById(m_originalColorId);

    if (originalPart) {
        m_originalPartLabel->setText(
            QString("%1 — %2").arg(originalPart->partNumber(), originalPart->name()));
    } else {
        m_originalPartLabel->setText("(Part unavailable)");
    }

    m_originalColorLabel->setText(
        originalColor ? originalColor->name() : QString("(Color unavailable)"));

    const int effectivePartId = requirement->effectivePartId();
    const int effectiveColorId = requirement->effectiveColorId();

    const std::optional<Part> effectivePart =
        partRepository.getById(effectivePartId);

    if (effectivePart) {
        m_usePartEdit->setText(effectivePart->partNumber());
    } else if (originalPart) {
        m_usePartEdit->setText(originalPart->partNumber());
    }

    const int colorIndex = m_useColorCombo->findData(effectiveColorId);
    if (colorIndex >= 0)
        m_useColorCombo->setCurrentIndex(colorIndex);

    m_quantitySpin->setValue(requirement->quantityRequired());
    m_spareCheck->setChecked(requirement->isSpare());
}

void EditBuildRequirementDialog::resetToOriginal()
{
    PartRepository partRepository;

    const std::optional<Part> originalPart =
        partRepository.getById(m_originalPartId);

    if (originalPart)
        m_usePartEdit->setText(originalPart->partNumber());

    const int colorIndex = m_useColorCombo->findData(m_originalColorId);

    if (colorIndex >= 0)
        m_useColorCombo->setCurrentIndex(colorIndex);
}

void EditBuildRequirementDialog::saveRequirement()
{
    BuildRequirementRepository repository;

    std::optional<BuildRequirement> requirement =
        repository.getById(m_requirementId);

    if (!requirement) {
        QMessageBox::critical(this, "BrickSuite", "Unable to load the build requirement.");
        return;
    }

    BuildRepository buildRepository;
    const std::optional<Build> build =
        buildRepository.getById(requirement->buildId());

    if (!build || build->inventoryMode() != "Stock") {
        QMessageBox::information(
            this,
            "Edit Build Requirement",
            "Requirement substitutions are available only for Build from Stock.");
        return;
    }

    const QString partNumber = m_usePartEdit->text().trimmed();

    if (partNumber.isEmpty()) {
        QMessageBox::warning(this,
                             "Edit Build Requirement",
                             "Enter a Part Number to use for this Build.");
        m_usePartEdit->setFocus();
        return;
    }

    PartRepository partRepository;
    const std::optional<Part> selectedPart =
        partRepository.getByPartNumber(partNumber);

    if (!selectedPart) {
        QMessageBox::warning(
            this,
            "Edit Build Requirement",
            QString("Part %1 was not found in the BrickSuite Parts Catalog.")
                .arg(partNumber));
        m_usePartEdit->setFocus();
        m_usePartEdit->selectAll();
        return;
    }

    const int selectedColorId = m_useColorCombo->currentData().toInt();

    if (selectedColorId <= 0) {
        QMessageBox::warning(this,
                             "Edit Build Requirement",
                             "Select a valid Color to use for this Build.");
        return;
    }

    const int newSubstitutePartId =
        selectedPart->id() == requirement->partId() ? 0 : selectedPart->id();

    const int newSubstituteColorId =
        selectedColorId == requirement->colorId() ? 0 : selectedColorId;

    const bool partOrColorChanged =
        newSubstitutePartId != requirement->substitutePartId()
        || newSubstituteColorId != requirement->substituteColorId();

    if (partOrColorChanged) {
        BuildAllocationRepository allocationRepository;

        //
        // Phase 1 linkage is authoritative for new requirement-aware
        // allocations. The part/color fallback also protects legacy
        // schema-v21 allocations, whose build_requirement_id is NULL.
        //
        const int linkedAllocated =
            allocationRepository.totalAllocatedForRequirement(requirement->id());

        const int legacyOrBuildAllocated =
            allocationRepository.totalAllocatedForPartColorForBuild(
                requirement->buildId(),
                requirement->effectivePartId(),
                requirement->effectiveColorId());

        if (linkedAllocated > 0 || legacyOrBuildAllocated > 0) {
            QMessageBox::information(
                this,
                "Edit Build Requirement",
                "This requirement already has inventory allocated to it.\n\n"
                "Part or Color cannot be changed until the allocation-aware "
                "substitution phase is implemented. Quantity and Spare may "
                "still be edited without changing the substitution.");
            return;
        }
    }

    requirement->setSubstitutePartId(newSubstitutePartId);
    requirement->setSubstituteColorId(newSubstituteColorId);
    requirement->setQuantityRequired(m_quantitySpin->value());
    requirement->setIsSpare(m_spareCheck->isChecked());

    if (!repository.update(*requirement)) {
        QMessageBox::critical(this, "BrickSuite", "Unable to update the build requirement.");
        return;
    }

    accept();
}
