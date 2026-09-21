#pragma once

#include "../../services/geometry/LDrawLoadResult.h"
#include "../../services/geometry/PrintOrientation.h"
#include "../../services/geometry/print/PreparedMesh.h"

#include <QColor>
#include <QDialog>
#include <memory>

class QComboBox;
class QLabel;
class QPushButton;

namespace PrintGeometry { struct ManufacturingMesh; }

class ManufacturingMeshDiagnosticDialog final : public QDialog
{
    Q_OBJECT
public:
    ManufacturingMeshDiagnosticDialog(const LDrawGeometry::LDrawLoadResult& source,
                                      const PrintGeometry::PreparedMesh& prepared,
                                      double uniformScale,
                                      const QColor& modelColor,
                                      const PrintOrientation& printOrientation,
                                      QWidget* parent = nullptr);

private:
    void populateProfiles();
    void updateProfileDetails();
    void generate();
    void exportMesh();
    void showResult(const PrintGeometry::ManufacturingMesh& mesh);

    LDrawGeometry::LDrawLoadResult m_source;
    PrintGeometry::PreparedMesh m_prepared;
    double m_uniformScale = 1.0;
    QColor m_modelColor;
    PrintOrientation m_printOrientation;
    std::shared_ptr<const PrintGeometry::ManufacturingMesh> m_manufacturingMesh;
    QComboBox* m_profiles = nullptr;
    QLabel* m_profileDetails = nullptr;
    QLabel* m_result = nullptr;
    QPushButton* m_generate = nullptr;
    QPushButton* m_export = nullptr;
    bool m_generating = false;
};
