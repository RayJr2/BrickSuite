#pragma once

#include "../../api/rebrickable/RebrickableService.h"

#include <QObject>

class RebrickableSetPartsService : public QObject
{
    Q_OBJECT
public:
    struct Result {
        bool success = false; QString setNumber; int compositionRows = 0;
        int requiredPieces = 0; int sparePieces = 0; QString message;
    };
    explicit RebrickableSetPartsService(QObject* parent = nullptr);
    bool isBusy() const;
    void retrieveAndReplace(int setCatalogId, const QString& setNumber, const QString& apiKey);
signals:
    void finished(const RebrickableSetPartsService::Result& result);
private:
    RebrickableService* m_api = nullptr;
    bool m_busy = false;
    int m_setCatalogId = 0;
    QString m_setNumber;
};
Q_DECLARE_METATYPE(RebrickableSetPartsService::Result)
