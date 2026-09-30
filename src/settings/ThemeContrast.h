#pragma once

#include <QColor>
#include <algorithm>
#include <cmath>

// Shared palette-derived contrast for Qt rich text and selection roles.
namespace ThemeContrast
{
inline double luminance(const QColor& color)
{
    const auto linear = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
    return .2126 * linear(color.redF()) + .7152 * linear(color.greenF()) + .0722 * linear(color.blueF());
}

inline QColor contrastingText(const QColor& background)
{
    return luminance(background) > .179 ? QColor(Qt::black) : QColor(Qt::white);
}

inline QColor readableLink(QColor link, const QColor& background)
{
    const double base = luminance(background);
    const QColor target = contrastingText(background);
    // Retain the platform link color when readable; otherwise move it towards
    // the contrasting endpoint without losing the link's underline or target.
    const QColor original = link;
    for (int step = 0; step < 100; ++step) {
        const double text = luminance(link);
        if ((std::max(text, base) + .05) / (std::min(text, base) + .05) >= 4.5)
            return link;
        const double blend = (step + 1) / 100.0;
        link = QColor::fromRgbF(original.redF() + (target.redF() - original.redF()) * blend,
                                original.greenF() + (target.greenF() - original.greenF()) * blend,
                                original.blueF() + (target.blueF() - original.blueF()) * blend);
    }
    return target;
}

} // namespace ThemeContrast
