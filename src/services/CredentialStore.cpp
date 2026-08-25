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

#include "CredentialStore.h"

#include <QByteArray>
#include <QProcess>
#include <QStandardPaths>

#if defined(Q_OS_WIN)
#  include <windows.h>
#  include <wincred.h>
#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)
#  include <Security/Security.h>
#endif

namespace {

constexpr auto kServiceName = "RFStateSide.BrickSuite";

QString targetName(const QString& credentialName)
{
    return QStringLiteral("%1/%2")
        .arg(QString::fromLatin1(kServiceName), credentialName);
}

#if defined(Q_OS_LINUX)

QString secretToolPath()
{
    return QStandardPaths::findExecutable(QStringLiteral("secret-tool"));
}

QStringList secretToolAttributes(const QString& credentialName)
{
    return {
        QStringLiteral("application"),
        QStringLiteral("BrickSuite"),
        QStringLiteral("credential"),
        credentialName
    };
}

bool waitForProcess(QProcess& process, QString* error)
{
    if (!process.waitForStarted()) {
        if (error) {
            *error = QStringLiteral("Unable to start secret-tool.");
        }
        return false;
    }

    if (!process.waitForFinished()) {
        process.kill();
        process.waitForFinished();

        if (error) {
            *error = QStringLiteral("secret-tool did not finish.");
        }
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit) {
        if (error) {
            *error = QStringLiteral("secret-tool terminated unexpectedly.");
        }
        return false;
    }

    return true;
}

#endif

} // namespace

CredentialStore::ReadResult CredentialStore::read(const QString& credentialName)
{
    ReadResult result;

#if defined(Q_OS_WIN)

    const std::wstring target = targetName(credentialName).toStdWString();

    PCREDENTIALW credential = nullptr;

    if (!CredReadW(target.c_str(),
                   CRED_TYPE_GENERIC,
                   0,
                   &credential)) {
        const DWORD code = GetLastError();

        if (code == ERROR_NOT_FOUND) {
            result.success = true;
            result.found = false;
            return result;
        }

        result.error =
            QStringLiteral("Windows Credential Manager read failed (error %1).")
                .arg(code);
        return result;
    }

    const QByteArray bytes(
        reinterpret_cast<const char*>(credential->CredentialBlob),
        static_cast<int>(credential->CredentialBlobSize));

    result.success = true;
    result.found = true;
    result.value = QString::fromUtf8(bytes);

    CredFree(credential);
    return result;

#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)

    const QByteArray service = QByteArray(kServiceName);
    const QByteArray account = credentialName.toUtf8();

    UInt32 passwordLength = 0;
    void* passwordData = nullptr;
    SecKeychainItemRef item = nullptr;

    const OSStatus status =
        SecKeychainFindGenericPassword(
            nullptr,
            static_cast<UInt32>(service.size()),
            service.constData(),
            static_cast<UInt32>(account.size()),
            account.constData(),
            &passwordLength,
            &passwordData,
            &item);

    if (status == errSecItemNotFound) {
        result.success = true;
        result.found = false;
        return result;
    }

    if (status != errSecSuccess) {
        result.error =
            QStringLiteral("macOS Keychain read failed (status %1).")
                .arg(status);
        return result;
    }

    result.success = true;
    result.found = true;
    result.value =
        QString::fromUtf8(
            static_cast<const char*>(passwordData),
            static_cast<int>(passwordLength));

    SecKeychainItemFreeContent(nullptr, passwordData);

    if (item) {
        CFRelease(item);
    }

    return result;

#elif defined(Q_OS_LINUX)

    const QString tool = secretToolPath();

    if (tool.isEmpty()) {
        result.error =
            QStringLiteral(
                "Linux Secret Service client 'secret-tool' is not installed.");
        return result;
    }

    QProcess process;
    QStringList arguments;
    arguments << QStringLiteral("lookup");
    arguments << secretToolAttributes(credentialName);

    process.start(tool, arguments);

    if (!waitForProcess(process, &result.error)) {
        return result;
    }

    if (process.exitCode() != 0) {
        // secret-tool lookup returns a non-zero code when no matching secret
        // exists. Treat an empty stdout result as a normal "not found".
        if (process.readAllStandardOutput().trimmed().isEmpty()) {
            result.success = true;
            result.found = false;
            return result;
        }

        result.error =
            QStringLiteral("Linux Secret Service lookup failed: %1")
                .arg(QString::fromUtf8(
                    process.readAllStandardError()).trimmed());
        return result;
    }

    const QByteArray output = process.readAllStandardOutput();

    result.success = true;
    result.found = true;
    result.value = QString::fromUtf8(output).trimmed();
    return result;

#else

    result.error =
        QStringLiteral("No secure credential backend is available on this platform.");
    return result;

#endif
}

bool CredentialStore::write(const QString& credentialName,
                            const QString& value,
                            QString* error)
{
    const QString trimmedValue = value.trimmed();

    if (trimmedValue.isEmpty()) {
        return remove(credentialName, error);
    }

#if defined(Q_OS_WIN)

    const std::wstring target = targetName(credentialName).toStdWString();
    const std::wstring username = credentialName.toStdWString();
    QByteArray bytes = trimmedValue.toUtf8();

    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName =
        const_cast<LPWSTR>(target.c_str());
    credential.UserName =
        const_cast<LPWSTR>(username.c_str());
    credential.CredentialBlobSize =
        static_cast<DWORD>(bytes.size());
    credential.CredentialBlob =
        reinterpret_cast<LPBYTE>(bytes.data());
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;

    if (!CredWriteW(&credential, 0)) {
        if (error) {
            *error =
                QStringLiteral(
                    "Windows Credential Manager write failed (error %1).")
                    .arg(GetLastError());
        }
        return false;
    }

    return true;

#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)

    const QByteArray service = QByteArray(kServiceName);
    const QByteArray account = credentialName.toUtf8();
    const QByteArray password = trimmedValue.toUtf8();

    SecKeychainItemRef item = nullptr;
    UInt32 existingLength = 0;
    void* existingData = nullptr;

    OSStatus status =
        SecKeychainFindGenericPassword(
            nullptr,
            static_cast<UInt32>(service.size()),
            service.constData(),
            static_cast<UInt32>(account.size()),
            account.constData(),
            &existingLength,
            &existingData,
            &item);

    if (status == errSecSuccess) {
        SecKeychainItemFreeContent(nullptr, existingData);

        status =
            SecKeychainItemModifyAttributesAndData(
                item,
                nullptr,
                static_cast<UInt32>(password.size()),
                password.constData());

        if (item) {
            CFRelease(item);
        }
    } else if (status == errSecItemNotFound) {
        status =
            SecKeychainAddGenericPassword(
                nullptr,
                static_cast<UInt32>(service.size()),
                service.constData(),
                static_cast<UInt32>(account.size()),
                account.constData(),
                static_cast<UInt32>(password.size()),
                password.constData(),
                nullptr);
    }

    if (status != errSecSuccess) {
        if (error) {
            *error =
                QStringLiteral("macOS Keychain write failed (status %1).")
                    .arg(status);
        }
        return false;
    }

    return true;

#elif defined(Q_OS_LINUX)

    const QString tool = secretToolPath();

    if (tool.isEmpty()) {
        if (error) {
            *error =
                QStringLiteral(
                    "Linux Secret Service client 'secret-tool' is not installed.");
        }
        return false;
    }

    QProcess process;
    QStringList arguments;
    arguments
        << QStringLiteral("store")
        << QStringLiteral("--label=BrickSuite API credential");
    arguments << secretToolAttributes(credentialName);

    process.start(tool, arguments);

    if (!process.waitForStarted()) {
        if (error) {
            *error = QStringLiteral("Unable to start secret-tool.");
        }
        return false;
    }

    process.write(trimmedValue.toUtf8());
    process.write("\n");
    process.closeWriteChannel();

    if (!process.waitForFinished()) {
        process.kill();
        process.waitForFinished();

        if (error) {
            *error = QStringLiteral("secret-tool did not finish.");
        }
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit
        || process.exitCode() != 0) {
        if (error) {
            const QString detail =
                QString::fromUtf8(
                    process.readAllStandardError()).trimmed();

            *error =
                detail.isEmpty()
                    ? QStringLiteral("Linux Secret Service store failed.")
                    : QStringLiteral("Linux Secret Service store failed: %1")
                          .arg(detail);
        }
        return false;
    }

    return true;

#else

    if (error) {
        *error =
            QStringLiteral(
                "No secure credential backend is available on this platform.");
    }
    return false;

#endif
}

bool CredentialStore::remove(const QString& credentialName,
                             QString* error)
{
#if defined(Q_OS_WIN)

    const std::wstring target = targetName(credentialName).toStdWString();

    if (!CredDeleteW(target.c_str(),
                     CRED_TYPE_GENERIC,
                     0)) {
        const DWORD code = GetLastError();

        if (code == ERROR_NOT_FOUND) {
            return true;
        }

        if (error) {
            *error =
                QStringLiteral(
                    "Windows Credential Manager delete failed (error %1).")
                    .arg(code);
        }
        return false;
    }

    return true;

#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)

    const QByteArray service = QByteArray(kServiceName);
    const QByteArray account = credentialName.toUtf8();

    UInt32 passwordLength = 0;
    void* passwordData = nullptr;
    SecKeychainItemRef item = nullptr;

    OSStatus status =
        SecKeychainFindGenericPassword(
            nullptr,
            static_cast<UInt32>(service.size()),
            service.constData(),
            static_cast<UInt32>(account.size()),
            account.constData(),
            &passwordLength,
            &passwordData,
            &item);

    if (status == errSecItemNotFound) {
        return true;
    }

    if (status != errSecSuccess) {
        if (error) {
            *error =
                QStringLiteral("macOS Keychain lookup failed (status %1).")
                    .arg(status);
        }
        return false;
    }

    SecKeychainItemFreeContent(nullptr, passwordData);

    status = SecKeychainItemDelete(item);

    if (item) {
        CFRelease(item);
    }

    if (status != errSecSuccess) {
        if (error) {
            *error =
                QStringLiteral("macOS Keychain delete failed (status %1).")
                    .arg(status);
        }
        return false;
    }

    return true;

#elif defined(Q_OS_LINUX)

    const QString tool = secretToolPath();

    if (tool.isEmpty()) {
        if (error) {
            *error =
                QStringLiteral(
                    "Linux Secret Service client 'secret-tool' is not installed.");
        }
        return false;
    }

    QProcess process;
    QStringList arguments;
    arguments << QStringLiteral("clear");
    arguments << secretToolAttributes(credentialName);

    process.start(tool, arguments);

    if (!waitForProcess(process, error)) {
        return false;
    }

    // Treat "nothing to clear" as success.
    if (process.exitCode() != 0) {
        const QString detail =
            QString::fromUtf8(
                process.readAllStandardError()).trimmed();

        if (!detail.isEmpty()) {
            if (error) {
                *error =
                    QStringLiteral("Linux Secret Service delete failed: %1")
                        .arg(detail);
            }
            return false;
        }
    }

    return true;

#else

    if (error) {
        *error =
            QStringLiteral(
                "No secure credential backend is available on this platform.");
    }
    return false;

#endif
}

QString CredentialStore::backendName()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("Windows Credential Manager");
#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    return QStringLiteral("macOS Keychain");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("Linux Secret Service");
#else
    return QStringLiteral("Unavailable");
#endif
}
