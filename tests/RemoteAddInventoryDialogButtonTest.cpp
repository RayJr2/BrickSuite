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

#include "../src/ui/inventory/AddInventoryDialogButtonState.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QPushButton>
#include <cstdio>

namespace {
bool require(bool value, const char* message)
{
    if (!value)
        std::fprintf(stderr, "%s\n", message);
    return value;
}
}

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QPushButton* const addButton = buttonBox.button(QDialogButtonBox::Ok);
    addButton->setText(QStringLiteral("Add"));

    bool ok = require(buttonBox.button(QDialogButtonBox::Save) == nullptr,
                      "The regression precondition changed: Save unexpectedly exists.");
    ok &= require(AddInventoryDialogButtonState::submitButton(&buttonBox) == addButton,
                  "Remote submission did not resolve the dialog's actual Add button.");

    AddInventoryDialogButtonState::setSubmissionPending(&buttonBox, true);
    ok &= require(!addButton->isEnabled(), "Add was not disabled while submission is pending.");

    AddInventoryDialogButtonState::setSubmissionPending(&buttonBox, false);
    ok &= require(addButton->isEnabled(), "Add was not restored after submission completed.");

    int acceptedCount = 0;
    QObject::connect(&buttonBox, &QDialogButtonBox::accepted,
                     [&acceptedCount]() { ++acceptedCount; });
    addButton->click();
    ok &= require(acceptedCount == 1, "One Add click did not produce exactly one submission signal.");

    AddInventoryDialogButtonState::setSubmissionPending(nullptr, true);
    return ok ? 0 : 1;
}
