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

#include "../src/services/Updater.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    const auto check = [&](bool condition, const char* message) {
        if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; }
    };
    const QJsonObject downloads{
        {"windows", QJsonObject{{"url", "https://example.test/windows"}}},
        {"linux64", QJsonObject{{"url", "https://example.test/linux"}}},
        {"macos-arm64", QJsonObject{{"url", "https://example.test/apple"}}},
        {"macos-x86_64", QJsonObject{{"url", "https://example.test/intel"}}},
        {"macos", QJsonObject{{"url", "https://example.test/generic"}}},
        {"linuxarm", QJsonObject{{"url", "https://example.test/unsupported"}}}
    };
    const QJsonObject manifest{{"version", "0.4.0"}, {"downloads", downloads}};
    const auto select = [&](const QString& os, const QString& architecture) {
        return Updater::pickDownloadUrl(manifest, Updater::platformKey(os, architecture));
    };
    check(select("macos", "arm64") == "https://example.test/apple", "ARM64 selects only Apple Silicon");
    check(select("macos", "x86_64") == "https://example.test/intel", "Intel process selects Intel, including under Rosetta");
    check(select("windows", "x86_64") == "https://example.test/windows", "Windows x64 preserved");
    check(select("linux", "x86_64") == "https://example.test/linux", "Linux x64 preserved");
    for (const auto& architecture : {"unknown", "i386", "power64", ""})
        check(select("macos", architecture).isEmpty(), "unsupported macOS fails safely");
    check(select("linux", "arm64").isEmpty(), "unsupported Linux ARM does not select stale entry");
    check(select("windows", "arm64").isEmpty(), "unsupported Windows ARM does not select x64");
    check(select("unknown", "x86_64").isEmpty(), "unsupported OS fails safely");

    for (const QString key : {QStringLiteral("macos-arm64"), QStringLiteral("macos-x86_64")}) {
        auto missing = downloads;
        missing.remove(key);
        check(Updater::pickDownloadUrl(QJsonObject{{"downloads", missing}}, key).isEmpty(),
              "missing exact entry never falls back to generic or other architecture");
        for (const auto& invalid : {QJsonValue(), QJsonValue(42), QJsonValue(QJsonObject()),
             QJsonValue(QJsonObject{{"url", ""}}), QJsonValue(QJsonObject{{"url", 42}}),
             QJsonValue("relative.zip"), QJsonValue("file:///tmp/package.zip"),
             QJsonValue("https://")}) {
            auto malformed = downloads;
            malformed.insert(key, invalid);
            check(Updater::pickDownloadUrl(QJsonObject{{"downloads", malformed}}, key).isEmpty(),
                  "malformed exact entry rejected despite other available artifacts");
        }
    }
    const QJsonObject legacy{{"platforms", QJsonObject{
        {"windows", QJsonObject{{"downloadUrl", "https://example.test/legacy"}}},
        {"macos", QJsonObject{{"downloadUrl", "https://example.test/legacy-mac"}}}}}};
    check(Updater::pickDownloadUrl(legacy, "windows") == "https://example.test/legacy", "legacy Windows object supported");
    check(Updater::pickDownloadUrl(legacy, "macos-arm64").isEmpty(), "legacy generic Mac is unsafe");
    check(Updater::pickDownloadUrl(legacy, "macos-x86_64").isEmpty(), "legacy generic Mac is unsafe for Intel too");
    for (const auto& entry : {QJsonValue(" https://example.test/string "),
                            QJsonValue(QJsonObject{{"downloadUrl", "https://example.test/string"}})})
        check(Updater::pickDownloadUrl(QJsonObject{{"downloads", QJsonObject{{"windows", entry}}}}, "windows")
              == "https://example.test/string", "existing string/downloadUrl entry contract preserved");
    check(Updater::pickDownloadUrl(QJsonObject{{"downloads", 42}}, "windows").isEmpty(), "malformed downloads map rejected");
    check(Updater::pickDownloadUrl({}, "windows").isEmpty(), "missing map rejected");

    QFile file(app.arguments().value(1));
    check(file.open(QIODevice::ReadOnly), "repository manifest readable");
    QJsonParseError error;
    const auto current = QJsonDocument::fromJson(file.readAll(), &error);
    check(error.error == QJsonParseError::NoError && current.isObject(), "repository manifest JSON valid");
    check(!Updater::pickDownloadUrl(current.object(), "windows").isEmpty(), "published Windows representation preserved");
    check(!Updater::pickDownloadUrl(current.object(), "linux64").isEmpty(), "existing Linux representation preserved");
    check(!current.object().value("version").toString().isEmpty(), "shared release version present");
    const auto entries = current.object().value("downloads").toObject();
    check(entries.contains("macos-arm64") && entries.contains("macos-x86_64"), "both explicit Mac keys represented");
    check(!entries.contains("macos") && !entries.contains("linuxarm"), "ambiguous and unsupported keys retired");
    return ok ? 0 : 1;
}
