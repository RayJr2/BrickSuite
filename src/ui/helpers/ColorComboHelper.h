#pragma once

#include <QString>

class QColor;
class QComboBox;

class ColorComboHelper
{
public:
    static int addColorItem(
        QComboBox* comboBox,
        const QString& name,
        int colorId,
        const QString& rgbHex);

    static QColor readableColor(
        const QColor& sourceColor,
        const QColor& backgroundColor);

private:
    static double relativeLuminance(
        const QColor& color);

    static double contrastRatio(
        const QColor& foreground,
        const QColor& background);
};