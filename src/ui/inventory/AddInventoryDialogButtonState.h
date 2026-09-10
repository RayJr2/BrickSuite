#pragma once

#include <QDialogButtonBox>
#include <QPushButton>

namespace AddInventoryDialogButtonState
{
inline QPushButton* submitButton(QDialogButtonBox* buttonBox)
{
    // The dialog uses the standard Ok role and changes only its visible text to "Add".
    return buttonBox ? buttonBox->button(QDialogButtonBox::Ok) : nullptr;
}

inline void setSubmissionPending(QDialogButtonBox* buttonBox, bool pending)
{
    if (QPushButton* button = submitButton(buttonBox))
        button->setEnabled(!pending);
}
}
