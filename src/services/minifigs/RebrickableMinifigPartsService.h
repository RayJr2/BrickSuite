#pragma once

#include "../../api/rebrickable/RebrickableService.h"

#include <QObject>

class RebrickableMinifigPartsService : public QObject
{
    Q_OBJECT
public:
    struct Result
    {
        bool success = false;
        QString figNumber;
        int compositionRows = 0;
        int requiredPieces = 0;
        int sparePieces = 0;
        QString message;
    };

    explicit RebrickableMinifigPartsService(QObject* parent = nullptr);
    bool isBusy() const;
    void retrieveAndReplace(int minifigCatalogId,
                            const QString& figNumber,
                            const QString& apiKey);

signals:
    void finished(const RebrickableMinifigPartsService::Result& result);

private:
    RebrickableService* m_api = nullptr;
    bool m_busy = false;
    int m_minifigCatalogId = 0;
    QString m_figNumber;
};

Q_DECLARE_METATYPE(RebrickableMinifigPartsService::Result)
