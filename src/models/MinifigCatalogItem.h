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
 */

#pragma once

#include <QDateTime>
#include <QString>

class MinifigCatalogItem
{
public:
    int id() const;
    void setId(int id);

    QString name() const;
    void setName(const QString& name);

    int numberOfParts() const;
    void setNumberOfParts(int numberOfParts);

    QString imageUrl() const;
    void setImageUrl(const QString& imageUrl);

    bool isActive() const;
    void setIsActive(bool isActive);

    QDateTime createdUtc() const;
    void setCreatedUtc(const QDateTime& createdUtc);

    QDateTime modifiedUtc() const;
    void setModifiedUtc(const QDateTime& modifiedUtc);

private:
    int m_id = 0;
    QString m_name;
    int m_numberOfParts = 0;
    QString m_imageUrl;
    bool m_isActive = true;
    QDateTime m_createdUtc;
    QDateTime m_modifiedUtc;
};
