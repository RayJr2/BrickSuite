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
#include <QPointer>

// Tracks one live top-level window per logical key. QPointer makes destruction
// self-invalidating, so asynchronous work can never resurrect an old window.
template<typename Key, typename Window>
class SingleInstanceWindowRegistry
{
public:
    Window* find(const Key& key) const
    {
        return m_windows.value(key).data();
    }

    void track(const Key& key, Window* window)
    {
        m_windows.insert(key, window);
    }

    void forget(const Key& key)
    {
        m_windows.remove(key);
    }

private:
    QHash<Key, QPointer<Window>> m_windows;
};
