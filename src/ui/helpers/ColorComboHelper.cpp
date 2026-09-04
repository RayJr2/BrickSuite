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

#include "ColorComboHelper.h"

#include <QColor>
#include <QComboBox>
#include <QPalette>
#include <QIcon>
#include <QPixmap>

#include <algorithm>
#include <cmath>

int ColorComboHelper::addColorItem(
    QComboBox* comboBox,
    const QString& name,
    int colorId,
    const QString& rgbHex,
    bool showCatalogSwatch)
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

    if (showCatalogSwatch) {
        QPixmap swatch(16, 16);
        swatch.fill(sourceColor);
        QIcon icon;
        // Catalog color is data: never let Qt synthesize a tinted selected
        // or disabled variant from the current UI palette.
        for (const auto mode : {QIcon::Normal, QIcon::Disabled, QIcon::Active, QIcon::Selected}) {
            icon.addPixmap(swatch, mode, QIcon::Off);
            icon.addPixmap(swatch, mode, QIcon::On);
        }
        comboBox->setItemIcon(index, icon);

        // The swatch now carries the catalog color. Leave the name on the
        // combo's palette-driven foreground so normal and selected text use
        // BrickSuite's theme-aware Text and HighlightedText colors.
        return index;
    }

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
