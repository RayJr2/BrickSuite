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
#include "../help/HelpManager.h"
#include "BricksetInstructionsDialog.h"

#include <QAbstractItemView>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

BricksetInstructionsDialog::BricksetInstructionsDialog(
    const QString& setNumber,
    const QList<BricksetService::Instruction>& instructions,
    QWidget* parent)
    : QDialog(parent)
    , m_setNumber(setNumber)
{
    HelpManager::setContextTopic(this, HelpTopic::SetsCatalog);
    setWindowTitle(QString("Instructions — %1").arg(setNumber));
    resize(820, 440);

    auto* layout = new QVBoxLayout(this);

    auto* noteLabel = new QLabel(
        "Instruction files are provided by Brickset and hosted by LEGO. "
        "Open launches the selected PDF in your default browser.",
        this);
    noteLabel->setWordWrap(true);
    layout->addWidget(noteLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({"Type", "Description", "Action"});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);

    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    layout->addWidget(m_table);

    populateTable(instructions);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}

bool BricksetInstructionsDialog::isCoreInstruction(
    const BricksetService::Instruction& instruction)
{
    return instruction.url.contains(QStringLiteral("/product.bi.core.pdf/"),
                                    Qt::CaseInsensitive);
}

void BricksetInstructionsDialog::populateTable(
    const QList<BricksetService::Instruction>& instructions)
{
    QList<BricksetService::Instruction> ordered = instructions;

    std::stable_sort(
        ordered.begin(),
        ordered.end(),
        [](const BricksetService::Instruction& left,
           const BricksetService::Instruction& right) {
            return BricksetInstructionsDialog::isCoreInstruction(left)
                   && !BricksetInstructionsDialog::isCoreInstruction(right);
        });

    m_table->setRowCount(ordered.size());

    for (int row = 0; row < ordered.size(); ++row) {
        const BricksetService::Instruction& instruction = ordered.at(row);
        const bool core = isCoreInstruction(instruction);

        auto* typeItem =
            new QTableWidgetItem(core ? "Core Instructions"
                                     : "Additional / Translated");
        m_table->setItem(row, 0, typeItem);

        auto* descriptionItem =
            new QTableWidgetItem(instruction.description.isEmpty()
                                     ? instruction.url
                                     : instruction.description);
        descriptionItem->setToolTip(instruction.url);
        m_table->setItem(row, 1, descriptionItem);

        auto* openButton = new QPushButton("Open", m_table);

        connect(openButton, &QPushButton::clicked, this, [this, instruction]() {
            const QUrl url(instruction.url);

            if (!url.isValid())
                return;

            QDesktopServices::openUrl(url);
        });

        m_table->setCellWidget(row, 2, openButton);
    }
}
