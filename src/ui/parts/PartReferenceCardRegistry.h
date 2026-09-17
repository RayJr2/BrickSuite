/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
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
