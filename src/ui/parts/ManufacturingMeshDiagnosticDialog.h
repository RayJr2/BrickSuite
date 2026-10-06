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
