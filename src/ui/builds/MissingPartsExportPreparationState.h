#pragma once

class MissingPartsExportPreparationState
{
public:
    bool begin()
    {
        if (m_active)
            return false;
        m_active = true;
        return true;
    }

    void finish() { m_active = false; }
    bool active() const { return m_active; }

private:
    bool m_active = false;
};
