#include "ColorComboHelper.h"

#include <QColor>
#include <QComboBox>
#include <QPalette>

#include <algorithm>
#include <cmath>

int ColorComboHelper::addColorItem(
    QComboBox* comboBox,
    const QString& name,
    int colorId,
    const QString& rgbHex)
{
    if (!comboBox)
        return -1;

    comboBox->addItem(
        name,
        colorId);

    const int index =
        comboBox->count() - 1;

    QString normalizedRgb = rgbHex.trimmed();

    if (!normalizedRgb.isEmpty() && !normalizedRgb.startsWith('#')) {
        normalizedRgb.prepend('#');
    }

    QColor sourceColor(normalizedRgb);

    if (!sourceColor.isValid())
        return index;

    if (!sourceColor.isValid())
        return index;

    const QColor backgroundColor =
        comboBox->palette().color(
            QPalette::Base);

    const QColor displayColor =
        readableColor(
            sourceColor,
            backgroundColor);

    comboBox->setItemData(
        index,
        displayColor,
        Qt::ForegroundRole);

    return index;
}

QColor ColorComboHelper::readableColor(
    const QColor& sourceColor,
    const QColor& backgroundColor)
{
    if (!sourceColor.isValid())
        return sourceColor;

    QColor result =
        sourceColor;

    //
    // 4.5:1 is a good target for ordinary text.
    //
    constexpr double MinimumContrast =
        4.5;

    if (contrastRatio(
            result,
            backgroundColor) >=
        MinimumContrast)
    {
        return result;
    }

    const bool backgroundIsDark =
        relativeLuminance(
            backgroundColor) < 0.5;

    //
    // Preserve the original hue as much as possible.
    // Gradually move the text color toward white on
    // dark backgrounds or black on light backgrounds.
    //
    for (int amount = 110;
         amount <= 300;
         amount += 10)
    {
        result =
            backgroundIsDark
                ? sourceColor.lighter(amount)
                : sourceColor.darker(amount);

        if (contrastRatio(
                result,
                backgroundColor) >=
            MinimumContrast)
        {
            return result;
        }
    }

    //
    // Final defensive fallback.
    //
    return backgroundIsDark
               ? QColor(Qt::white)
               : QColor(Qt::black);
}

double ColorComboHelper::relativeLuminance(
    const QColor& color)
{
    auto channel =
        [](double value)
        {
            value /= 255.0;

            return value <= 0.04045
                       ? value / 12.92
                       : std::pow(
                             (value + 0.055) /
                                 1.055,
                             2.4);
        };

    const double red =
        channel(color.red());

    const double green =
        channel(color.green());

    const double blue =
        channel(color.blue());

    return
        0.2126 * red +
        0.7152 * green +
        0.0722 * blue;
}

double ColorComboHelper::contrastRatio(
    const QColor& foreground,
    const QColor& background)
{
    const double foregroundLuminance =
        relativeLuminance(
            foreground);

    const double backgroundLuminance =
        relativeLuminance(
            background);

    const double lighter =
        std::max(
            foregroundLuminance,
            backgroundLuminance);

    const double darker =
        std::min(
            foregroundLuminance,
            backgroundLuminance);

    return
        (lighter + 0.05) /
        (darker + 0.05);
}