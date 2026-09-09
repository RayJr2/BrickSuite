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
