#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

struct CollectionExportRow {
    qint64 collectionItemId=0;
    QString type,reference,name,nickname,state,condition,completeness,location,source;
    bool allowPartsSource=false;
    QString notes,sourceBuildReference,sourceBuildName,createdUtc,modifiedUtc;
};

struct CollectionExportFieldDescriptor {
    QString id,label,header;
    bool defaultEnabled=false;
};

struct CollectionExportConfiguration {
    QStringList fieldOrder;
    QSet<QString> enabledFields;
};

struct CollectionExportProjection {
    QList<CollectionExportFieldDescriptor> fields;
    QStringList headers;
    QList<QStringList> rows;
};
