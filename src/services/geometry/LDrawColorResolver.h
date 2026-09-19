#pragma once

#include <QVector4D>
#include <QString>
class QColor;

class LDrawColorResolver
{
public:
    static QVector4D faceColor(const QString& value);
    static QVector4D faceColor(const QString& value, const QColor& inheritedColor);
    static QVector4D edgeColor(const QString& value);
    static QVector4D edgeColor(const QString& value, const QColor& inheritedColor);
    static QVector4D wireframeColor();
    static QColor defaultModelColor();
};
