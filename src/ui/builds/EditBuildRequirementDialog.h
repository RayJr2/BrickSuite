#pragma once

#include <QDialog>

class QLabel;
class QSpinBox;
class QCheckBox;

class EditBuildRequirementDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditBuildRequirementDialog(int requirementId, QWidget* parent = nullptr);

private:
    void loadRequirement();
    void saveRequirement();

    int m_requirementId = 0;

    QLabel* m_partLabel = nullptr;
    QLabel* m_colorLabel = nullptr;

    QSpinBox* m_quantitySpin = nullptr;
    QCheckBox* m_spareCheck = nullptr;
};
