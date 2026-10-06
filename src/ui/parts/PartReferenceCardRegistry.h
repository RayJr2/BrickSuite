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

#include <QHash>
#include <QList>
#include <QPointer>
#include <QString>
#include <QToolButton>

class PartReferenceCardRegistry
{
public:
    void add(const QString& key, QToolButton* card)
    {
        auto& entries = m_cards[key];
        entries.removeIf([](const QPointer<QToolButton>& entry) { return entry.isNull(); });
        entries.append(QPointer<QToolButton>(card));
    }

    QList<QPointer<QToolButton>> cards(const QString& key) const
    {
        QList<QPointer<QToolButton>> liveCards;
        const auto entries = m_cards.value(key);
        for (const QPointer<QToolButton>& card : entries) {
            if (card)
                liveCards.append(card);
        }
        return liveCards;
    }

    bool contains(const QString& key) const
    {
        return !cards(key).isEmpty();
    }

    void clear()
    {
        m_cards.clear();
    }

private:
    QHash<QString, QList<QPointer<QToolButton>>> m_cards;
};
