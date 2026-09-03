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

#include "HelpTopic.h"

#include <QList>
#include <QString>

#include <optional>

class QWidget;

struct HelpTopicInfo
{
    HelpTopic topic;
    QString title;
    QString resourcePath;
};

class HelpManager
{
public:
    static void showTopic(HelpTopic topic, QWidget* parent = nullptr);

    static QList<HelpTopicInfo> topics();
    static HelpTopicInfo topicInfo(HelpTopic topic);

    static QString resourcePath(HelpTopic topic);
    static QString title(HelpTopic topic);

    static void setContextTopic(QWidget* window, HelpTopic topic);
    static std::optional<HelpTopic> contextTopic(const QWidget* window);
};
