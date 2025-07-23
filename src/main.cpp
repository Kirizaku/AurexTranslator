/******************************************************************************
    Copyright (C) 2025 by Daniil Nabiulin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <QLocale>
#include <QDir>
#include <QMenu>
#include <QTranslator>
#include <QSystemTrayIcon>
#include <QStandardPaths>
#include <QApplication>

#ifdef Q_OS_LINUX
#include <sys/utsname.h>
#endif

#include "src/UI/mainwindow.h"
#include "src/utils/logger.h"
#include "src/utils/config.h"
#include "src/data.h"

namespace
{
    QString getAppSettingsPath()
    {
        QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir dir(defaultDir);

        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                Log(Logger::Level::Warning, QString("[config] Failed to create config directory: %1").arg(defaultDir));
                defaultDir = QDir::currentPath();
            }
        }
        return defaultDir + QDir::separator();
    }

#ifdef __linux__
    QString getKernelVersion()
    {
        struct utsname info;
        if (uname(&info) < 0)
            return "Unknown";

        return QString::fromUtf8(info.release);
    }

    QString getDistroName() {
        QFile file("/etc/os-release");
        if (!file.open(QFile::ReadOnly)) {
            return "Unknown";
        }

        QTextStream stream(&file);
        QString line;
        while (stream.readLineInto(&line)) {
            if (line.startsWith("NAME=")) {
                QStringList parts = line.split("=");
                if (parts.size() > 1) {
                    QString distroName = parts.at(1);
                    distroName.remove('"');
                    return distroName;
                }
            }
        }
        return "Unknown";
    }
#endif

    void logSystemInfo()
    {
#ifdef __linux__
        QString sessionType = qEnvironmentVariable("XDG_SESSION_TYPE");
        QString sessionDesktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");

        if (sessionType == "wayland") {
            qputenv("QT_QPA_PLATFORM", "xcb");
            Log(Logger::Level::Info, "Session Type: Wayland");
        } else if (sessionType == "x11") {
            Log(Logger::Level::Info, "Session Type: Wayland");
        }

        Log(Logger::Level::Info, QString("Distribution: %1").arg(getDistroName()));
        Log(Logger::Level::Info, QString("Kernel Version: %1").arg(getKernelVersion()));
        Log(Logger::Level::Info, QString("Session Desktop: %1").arg(sessionDesktop));
#else
        OSVERSIONINFO osvi;
        ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx(&osvi);

        Log(Logger::Level::Info, QString("Windows Version: %1.%2.%3").arg(osvi.dwMajorVersion).arg(osvi.dwMinorVersion).arg(osvi.dwBuildNumber));
#endif
        Log(Logger::Level::Info, QString("CPU Architecture: %1").arg(QSysInfo::currentCpuArchitecture()));
    }

    void setupTranslations(QTranslator &translator, QApplication &app)
    {
        QJsonObject general = Config::getValue("general").toJsonObject();
        if (general["language"].toString().isEmpty()) {
            const QStringList uiLanguages = QLocale::system().uiLanguages();
            for (const QString &locale : uiLanguages) {
                const QString baseName = QString::fromStdString(PROJECT_NAME) + "_" + QLocale(locale).name();
                if (translator.load(baseName)) {
                    app.installTranslator(&translator);
                    general["language"] = QLocale(locale).name();
                    Config::setValue("general", general);
                    break;
                }
            }
        } else {
            const QString baseName = QString::fromStdString(PROJECT_NAME) + "_" + general["language"].toString();
            if (translator.load(":/i18n/" + baseName)) {
                qApp->installTranslator(&translator);
            } else {
                Log(Logger::Level::Warning, "Failed to load translation");
            }
        }
    }

    void createTrayIcon(QSystemTrayIcon &trayIcon, MainWindow &mainWin, QApplication &app)
    {
        trayIcon.setIcon(qApp->style()->standardIcon(QStyle::SP_ComputerIcon));
        trayIcon.setToolTip(APP_NAME);

        QMenu* trayMenu = new QMenu();

        QAction* restoreAction = new QAction(QObject::tr("Open"), trayMenu);
        QObject::connect(restoreAction, &QAction::triggered, [&mainWin]() {
            mainWin.showNormal();
        });

        QAction* quitAction = new QAction(QObject::tr("Exit"), trayMenu);
        QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);

        trayMenu->addAction(restoreAction);
        trayMenu->addAction(quitAction);

        trayIcon.setContextMenu(trayMenu);

        QObject::connect(&trayIcon, &QSystemTrayIcon::activated, [&](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) {
                mainWin.isVisible() ? mainWin.hide() : mainWin.showNormal();
            }
        });
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName(APP_NAME);

    QString configPath = getAppSettingsPath();
    Logger::instance()->initInstance(configPath);
    Config::instance()->initInstance(configPath);
    Config::loadConfig("settings.json");

    logSystemInfo();

    QApplication app(argc, argv);

    QTranslator translator;
    setupTranslations(translator, app);

    MainWindow mainWin;
    QSystemTrayIcon trayIcon;
    createTrayIcon(trayIcon, mainWin, app);

    trayIcon.show();

    return app.exec();
}
