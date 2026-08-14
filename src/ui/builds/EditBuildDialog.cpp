#include "EditBuildDialog.h"

#include "../../models/Build.h"
#include "../../repositories/BuildRepository.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

EditBuildDialog::EditBuildDialog(int buildId, QWidget* parent)
    : QDialog(parent)
    , m_buildId(buildId)
{
    setWindowTitle("Edit Build");

    resize(520, 360);

    auto* mainLayout = new QVBoxLayout(this);

    auto* formLayout = new QFormLayout();

    m_typeLabel = new QLabel(this);

    m_setNumberLabel = new QLabel(this);

    m_inventoryModeLabel = new QLabel(this);

    m_nameEdit = new QLineEdit(this);

    m_statusCombo = new QComboBox(this);

    //
    // Match the status values already accepted by
    // BuildRepository.
    //
    m_statusCombo->addItem("Planned", "Planned");

    m_statusCombo->addItem("Pulling", "Pulling");

    m_statusCombo->addItem("Complete", "Complete");

    m_statusCombo->addItem("Disassembled", "Disassembled");

    m_notesEdit = new QTextEdit(this);

    m_notesEdit->setMaximumHeight(120);

    formLayout->addRow("Type:", m_typeLabel);

    formLayout->addRow("Set Number:", m_setNumberLabel);

    formLayout->addRow("Inventory Mode:", m_inventoryModeLabel);

    formLayout->addRow("Name:", m_nameEdit);

    formLayout->addRow("Status:", m_statusCombo);

    formLayout->addRow("Notes:", m_notesEdit);

    mainLayout->addLayout(formLayout);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);

    m_saveButton = buttonBox->button(QDialogButtonBox::Save);

    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &EditBuildDialog::saveBuild);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (!loadBuild()) {
        m_saveButton->setEnabled(false);
    }
}

bool EditBuildDialog::loadBuild()
{
    BuildRepository repository;

    const std::optional<Build> build = repository.getById(m_buildId);

    if (!build) {
        QMessageBox::critical(this, "Edit Build", "Unable to load the selected Build.");

        return false;
    }

    m_typeLabel->setText(build->buildType());

    m_setNumberLabel->setText(build->setNumber().isEmpty() ? "(None)" : build->setNumber());

    if (build->inventoryMode() == "CompleteSet") {
        m_inventoryModeLabel->setText("Complete Set");
    } else {
        m_inventoryModeLabel->setText("Build from Stock");
    }

    m_nameEdit->setText(build->name());

    const int statusIndex = m_statusCombo->findData(build->status());

    if (statusIndex >= 0) {
        m_statusCombo->setCurrentIndex(statusIndex);
    }

    m_notesEdit->setPlainText(build->notes());

    return true;
}

void EditBuildDialog::saveBuild()
{
    const QString name = m_nameEdit->text().trimmed();

    if (name.isEmpty()) {
        QMessageBox::warning(this, "Edit Build", "Enter a name for the Build.");

        return;
    }

    BuildRepository repository;

    std::optional<Build> build = repository.getById(m_buildId);

    if (!build) {
        QMessageBox::critical(this, "Edit Build", "Unable to reload the selected Build.");

        return;
    }

    build->setName(name);

    build->setStatus(m_statusCombo->currentData().toString());

    build->setNotes(m_notesEdit->toPlainText().trimmed());

    if (!repository.update(*build)) {
        QMessageBox::critical(this, "Edit Build", "Unable to update the Build.");

        return;
    }

    accept();
}