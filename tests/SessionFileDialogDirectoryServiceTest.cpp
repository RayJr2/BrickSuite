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

#include "../src/ui/common/SessionFileDialogDirectoryService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
bool require(bool condition, const char* message)
{
    if (!condition)
        QTextStream(stderr) << "FAIL: " << message << '\n';
    return condition;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    QTemporaryDir openDir;
    QTemporaryDir saveDir;
    ok &= require(openDir.isValid() && saveDir.isValid(), "temporary directories available");

    SessionFileDialogDirectoryService service;
    ok &= require(service.rememberedDirectory(FileDialogDirectoryCategory::OpenImport).isEmpty(),
                  "new session starts without OpenImport memory");
    ok &= require(service.rememberedDirectory(FileDialogDirectoryCategory::SaveExport).isEmpty(),
                  "new session starts without SaveExport memory");

    service.rememberSelectedFile(FileDialogDirectoryCategory::OpenImport,
                                 QDir(openDir.path()).filePath("parts.csv"));
    service.rememberSelectedDirectory(FileDialogDirectoryCategory::SaveExport,
                                      saveDir.path());
    ok &= require(service.rememberedDirectory(FileDialogDirectoryCategory::OpenImport)
                      == QDir::cleanPath(openDir.path()),
                  "file selection remembers its parent directory");
    ok &= require(service.rememberedDirectory(FileDialogDirectoryCategory::SaveExport)
                      == QDir::cleanPath(saveDir.path()),
                  "directory selection remembers itself");
    ok &= require(service.initialFilePath(FileDialogDirectoryCategory::SaveExport,
                                         QStringLiteral("3037.obj"))
                      == QDir(saveDir.path()).filePath(QStringLiteral("3037.obj")),
                  "remembered directory combines with a new generated filename");

    service.rememberSelectedFile(FileDialogDirectoryCategory::OpenImport, QString());
    ok &= require(service.rememberedDirectory(FileDialogDirectoryCategory::OpenImport)
                      == QDir::cleanPath(openDir.path()),
                  "cancel does not replace memory");

    const QString removedPath = openDir.path();
    openDir.remove();
    ok &= require(service.initialDirectory(FileDialogDirectoryCategory::OpenImport,
                                           QStringLiteral("fallback"))
                      == QStringLiteral("fallback"),
                  "missing remembered directory uses fallback");
    ok &= require(!QFileInfo::exists(removedPath), "missing directory is not recreated");

    SessionFileDialogDirectoryService restarted;
    ok &= require(restarted.rememberedDirectory(FileDialogDirectoryCategory::SaveExport).isEmpty(),
                  "new service session starts empty");

    if (ok)
        QTextStream(stdout) << "SessionFileDialogDirectoryServiceTest passed\n";
    return ok ? 0 : 1;
}
