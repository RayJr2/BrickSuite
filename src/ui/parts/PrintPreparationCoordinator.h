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
