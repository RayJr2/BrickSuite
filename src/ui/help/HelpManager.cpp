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

#include "HelpManager.h"

#include "HelpDialog.h"

#include <QPointer>

namespace
{

QPointer<HelpDialog> s_helpDialog;

const QList<HelpTopicInfo>& helpTopics()
{
    static const QList<HelpTopicInfo> topics = {
        {HelpTopic::Home, "Help Home", ":/help/index.html"},
        {HelpTopic::GettingStarted, "Getting Started", ":/help/getting_started.html"},
        {HelpTopic::QuickStart, "Quick Start", ":/help/quick_start.html"},
        {HelpTopic::Storage, "Storage", ":/help/storage.html"},
        {HelpTopic::PartsCatalog, "Parts Catalog", ":/help/parts_catalog.html"},
        {HelpTopic::SetsCatalog, "Sets Catalog", ":/help/sets_catalog.html"},
        {HelpTopic::MinifigsCatalog, "Minifigs Catalog", ":/help/minifigs_catalog.html"},
        {HelpTopic::Inventory, "My Inventory", ":/help/inventory.html"},
        {HelpTopic::RebrickableImport, "Rebrickable Import", ":/help/rebrickable_import.html"},
        {HelpTopic::Builds, "Builds", ":/help/builds.html"},
        {HelpTopic::Mocs, "MOCs", ":/help/mocs.html"},
        {HelpTopic::AllocateAvailable, "Allocate Available", ":/help/allocate_available.html"},
        {HelpTopic::MissingParts, "Missing Parts", ":/help/missing_parts.html"},
        {HelpTopic::LostFound, "Lost / Found", ":/help/lost_found.html"},
        {HelpTopic::BackupRestore, "Backup / Restore", ":/help/backup_restore.html"},
        {HelpTopic::DatabaseStatus, "Database Status & Integrity", ":/help/database_status.html"},
        {HelpTopic::ReferenceData, "Lists & Reference Data", ":/help/reference_data.html"},
        {HelpTopic::Settings, "Settings", ":/help/settings.html"},
        {HelpTopic::Logging, "Application Log", ":/help/logging.html"},
        {HelpTopic::Troubleshooting, "Troubleshooting", ":/help/troubleshooting.html"},
    };

    return topics;
}

} // namespace

void HelpManager::showTopic(HelpTopic topic, QWidget* parent)
{
    if (!s_helpDialog) {
        s_helpDialog = new HelpDialog(parent);
        s_helpDialog->setAttribute(Qt::WA_DeleteOnClose);

        QObject::connect(s_helpDialog, &QObject::destroyed, []() {
            s_helpDialog = nullptr;
        });
    }

    s_helpDialog->showTopic(topic);
    s_helpDialog->show();
    s_helpDialog->raise();
    s_helpDialog->activateWindow();
}

QList<HelpTopicInfo> HelpManager::topics()
{
    return helpTopics();
}

HelpTopicInfo HelpManager::topicInfo(HelpTopic topic)
{
    for (const HelpTopicInfo& info : helpTopics()) {
        if (info.topic == topic)
            return info;
    }

    return {HelpTopic::Home, "Help Home", ":/help/index.html"};
}

QString HelpManager::resourcePath(HelpTopic topic)
{
    return topicInfo(topic).resourcePath;
}

QString HelpManager::title(HelpTopic topic)
{
    return topicInfo(topic).title;
}
