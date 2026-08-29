/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 */

#pragma once

#include "../../models/PartReferenceEntry.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class PartReferenceManifest
{
public:
    static constexpr int ExpectedEntryCount = 2955;

    bool load(QString* errorMessage = nullptr);

    bool isLoaded() const;
    int entryCount() const;

    const QList<PartReferenceEntry>& entries() const;
    QStringList catalogs() const;
    QList<PartReferenceEntry> entriesForCatalog(const QString& catalog) const;
    QList<PartReferenceEntry> search(const QString& text) const;
    const PartReferenceEntry* findByPartNumber(const QString& partNumber) const;

private:
    void clear();
    static QString normalizedKey(const QString& value);

    QList<PartReferenceEntry> m_entries;
    QStringList m_catalogs;
    QHash<QString, int> m_entryIndexByPartNumber;
    bool m_loaded = false;
};
