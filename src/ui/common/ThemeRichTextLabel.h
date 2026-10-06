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

#include "../../settings/ThemeContrast.h"
#include <QEvent>
#include <QLabel>

// QLabel's parsed rich-text anchors do not follow palette changes on their own.
// Keep the original markup so live theme changes never accumulate CSS wrappers.
class ThemeRichTextLabel : public QLabel
{
public:
    explicit ThemeRichTextLabel(const QString& text, QWidget* parent = nullptr)
        : QLabel(parent), m_source(text)
    {
        refreshText();
    }

    void setText(const QString& text)
    {
        m_source = text;
        refreshText();
    }

protected:
    void changeEvent(QEvent* event) override
    {
        QLabel::changeEvent(event);
        if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
            refreshText();
    }

private:
    void refreshText()
    {
        const int start = selectionStart();
        const int length = selectedText().size();
        const QColor link = ThemeContrast::readableLink(palette().color(QPalette::Link),
                                                        palette().color(backgroundRole()));
        const QString styled = m_source.contains("<a ", Qt::CaseInsensitive)
            ? QString("<style>a { color: %1; }</style>").arg(link.name()) + m_source
            : m_source;
        if (text() == styled) return;
        QLabel::setText(styled);
        if (start >= 0 && length > 0) setSelection(start, length);
    }

    QString m_source;
};
