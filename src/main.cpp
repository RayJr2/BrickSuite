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

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QProgressBar>
#include <QFont>
#include <QIcon>
#include <QLockFile>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QSplashScreen>
#include <QStandardPaths>

#include "app/Application.h"
#include "core/AppConstants.h"
#include "core/AppVersion.h"
#include "services/Logger.h"
#include "settings/ThemeManager.h"

namespace {

QPixmap createSplashPixmap(const QApplication& application)
{
    constexpr int splashWidth = 520;
    constexpr int splashHeight = 300;

    const QPalette palette = application.palette();
    const QColor backgroundColor = palette.color(QPalette::Window);
    const QColor textColor = palette.color(QPalette::WindowText);
    const QColor accentColor(0, 96, 96);

    QPixmap pixmap(splashWidth, splashHeight);
    pixmap.fill(backgroundColor);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    painter.setPen(QPen(accentColor, 4));
    painter.drawRect(pixmap.rect().adjusted(2, 2, -3, -3));

    const QPixmap logo = QIcon(QStringLiteral(":/icons/bricksuite.ico")).pixmap(128, 128);
    painter.drawPixmap((splashWidth - logo.width()) / 2, 30, logo);

    QFont titleFont = application.font();
    titleFont.setPointSize(26);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(textColor);
    painter.drawText(QRect(20, 165, splashWidth - 40, 45),
                     Qt::AlignCenter,
                     AppConstants::name());

    QFont detailFont = application.font();
    detailFont.setPointSize(11);
    painter.setFont(detailFont);
    painter.drawText(QRect(20, 215, splashWidth - 40, 24),
                     Qt::AlignCenter,
                     QString("Version %1").arg(AppVersion::version()));

    return pixmap;
}

} // namespace

int main(int argc, char *argv[])
{
    QElapsedTimer startupTimer;
    startupTimer.start();
    QApplication qtApplication(argc, argv);

    QApplication::setApplicationName(AppConstants::name());
    QApplication::setApplicationVersion(AppVersion::version());
    QApplication::setOrganizationName(AppConstants::OrganizationName());

    qtApplication.setWindowIcon(QIcon(QStringLiteral(":/icons/bricksuite.ico")));

    // Check for existing instance of BrickSuite using a lock file in the temporary directory.
    const QString lockPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                 .filePath("RFStateSide_BrickSuite.lock");

    QLockFile instanceLock(lockPath);
    instanceLock.setStaleLockTime(0);

    if (!instanceLock.tryLock()) {
        QMessageBox::information(nullptr,
                                 "BrickSuite",
                                 "BrickSuite is already running.\n\n"
                                 "Only one instance may run at a time.");

        return 0;
    }

    //
    // Install the application-wide Qt message handler after the Qt
    // application identity is set so AppLocalDataLocation resolves to the
    // BrickSuite data folder. Existing qDebug/qInfo/qWarning/qCritical calls
    // are captured automatically from this point forward.
    //
    Logger::init();

    qInfo() << "BrickSuite version:" << QApplication::applicationVersion();

    ThemeManager::applySavedTheme(qtApplication);

    QSplashScreen splash(createSplashPixmap(qtApplication));
    QLabel startupStatus(QStringLiteral("Starting BrickSuite..."), &splash);
    startupStatus.setGeometry(20, 244, 480, 24);
    startupStatus.setAlignment(Qt::AlignCenter);
    QProgressBar startupProgress(&splash);
    startupProgress.setGeometry(30, 276, 460, 12);
    startupProgress.setRange(0, 5);
    startupProgress.setValue(0);
    startupProgress.setTextVisible(false);
    splash.show();

    // Ensure the splash is painted before synchronous database and window
    // initialization begins. This processes pending events without delaying
    // startup or introducing a timer.
    qtApplication.processEvents();

    Application application;

    if (!application.initialize([&](int stage, const QString& status) {
            startupStatus.setText(status);
            startupProgress.setValue(stage);
            // Paint real stage boundaries without a timer or nested event dispatch.
            splash.repaint();
        })) {
        splash.close();
        qCritical() << "BrickSuite application initialization failed.";
        Logger::shutdown();
        return EXIT_FAILURE;
    }

    splash.finish(application.mainWindow());
    qInfo().noquote() << QStringLiteral("BrickSuite startup completed in %1 seconds.")
                            .arg(startupTimer.elapsed() / 1000.0, 0, 'f', 2);

    const int result = qtApplication.exec();

    Logger::shutdown();

    return result;
}
