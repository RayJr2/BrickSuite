#pragma once

#include <QDialog>
#include <QFutureWatcher>
#include "../../services/geometry/PartMesh.h"

class QLabel;
class QPushButton;

class LDrawModelInfoDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LDrawModelInfoDialog(const QString& ldrawId, QWidget* parent = nullptr);

private:
    void startLoad();
    void applyResult(const LDrawGeometry::Result& result);
    void exportObj();

    QString m_ldrawId;
    LDrawGeometry::PartMesh m_mesh;
    QLabel* m_status = nullptr;
    QLabel* m_source = nullptr;
    QLabel* m_dimensions = nullptr;
    QLabel* m_counts = nullptr;
    QLabel* m_bfc = nullptr;
    QPushButton* m_reload = nullptr;
    QPushButton* m_export = nullptr;
    QFutureWatcher<LDrawGeometry::Result>* m_watcher = nullptr;
};
