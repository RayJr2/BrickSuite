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

#include "../src/services/CredentialStore.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    const QByteArray originalPath = qgetenv("PATH");
    qputenv("PATH", directory.path().toUtf8());

    const auto lookup = [&](const QByteArray& script) {
        QFile tool(directory.filePath("secret-tool"));
        if (!tool.open(QIODevice::WriteOnly)
            || tool.write("#!/bin/sh\n" + script) < 0
            || !tool.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ExeOwner)) {
            return CredentialStore::ReadResult{};
        }
        tool.close();
        return CredentialStore::read(QStringLiteral("SyntheticLinuxAuditCredential"));
    };
    bool ok = true;
    const auto require = [&](bool passed, const char* message) {
        if (!passed) QTextStream(stderr) << "FAILED: " << message << '\n';
        ok &= passed;
    };

    const auto absent = lookup("exit 1\n");
    require(absent.success && !absent.found && absent.error.isEmpty(),
            "missing credential is a normal miss");
    const auto unavailable = lookup("printf 'Secret Service unavailable\\n' >&2\nexit 1\n");
    require(!unavailable.success && !unavailable.found
                && unavailable.error.contains("Secret Service unavailable"),
            "unavailable backend is an error even with empty stdout");
    const auto unexpected = lookup("exit 2\n");
    require(!unexpected.success && !unexpected.found
                && unexpected.error.contains("exit code 2"),
            "unexpected silent command failure is an error");
    const auto present = lookup("printf 'synthetic-value\\n'\n");
    require(present.success && present.found && present.value == "synthetic-value"
                && present.error.isEmpty(),
            "successful lookup preserves its value without logging it");
    qputenv("PATH", originalPath);
    return ok ? 0 : 1;
}
