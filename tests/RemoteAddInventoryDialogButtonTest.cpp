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
