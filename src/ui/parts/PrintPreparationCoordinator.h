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

#include "../../services/geometry/print/LDrawPrintPreparationService.h"

#include <QObject>
#include <memory>

class PrintPreparationCoordinator : public QObject
{
    Q_OBJECT
public:
    explicit PrintPreparationCoordinator(QObject* parent=nullptr);
    bool start(quint64 generation,const PrintGeometry::PrintPreparationRequest& request);
    void cancel();
    bool busy()const{return m_busy;}
    std::shared_ptr<PrintGeometry::PrintPreparationCache> cache()const{return m_cache;}
signals:
    void progress(quint64 generation,PrintGeometry::PrintPreparationProgress progress);
    void completed(quint64 generation,PrintGeometry::PrintPreparationResult result);
    void busyChanged(bool busy);
private:
    std::shared_ptr<PrintGeometry::PrintPreparationCache>m_cache;
    std::shared_ptr<PrintGeometry::CancellationState>m_cancellation;
    std::shared_ptr<PrintGeometry::LDrawPrintPreparationService>m_service;
    bool m_busy=false;
};
