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

#include <QDir>
#include <QDateTime>
#include <QDebug>
#include "logger.h"

Logger *Logger::m_instance = nullptr;
Logger *Logger::instance()
{
    return m_instance;
}

Logger::Logger(QObject* parent) : QObject(parent) {}
Logger::~Logger()
{
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

void Logger::initInstance(const QString& configPath)
{
    if (!m_instance) {
        m_instance = new Logger();
    }

    m_instance->m_logsDirPath = configPath + "logs";
    QDir logDir(m_instance->m_logsDirPath);
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    m_instance->m_logFilePath = m_instance->m_logsDirPath + QDir::separator() + QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss") + ".txt";

    QStringList logFiles = logDir.entryList(QStringList() << "*.txt", QDir::Files);
    if (logFiles.size() > 9) {
        logFiles.sort();

        for (int i = 0; i < logFiles.size() - 9; i++) {
            QString oldLogFilePath = m_instance->m_logsDirPath + QDir::separator() + logFiles[i];
            QFile::remove(oldLogFilePath);
        }
    }

    QFile& logFile = m_instance->m_logFile;
    logFile.setFileName(m_instance->m_logFilePath);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_instance->write(Logger::Level::Warning, "Cannot open log file: " + m_instance->m_logFilePath);
    }
}

void Logger::destroyInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = nullptr;
    }
}

void Logger::write(Level level, const QString& message)
{
    QString logMessage = QString("[%1] %2").arg(levelToString(level), message);

    writeToConsole(level, logMessage);
    writeToFile(logMessage);

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    emit newLogMessage(QString("[%1] %2").arg(timestamp).arg(logMessage));
}

void Logger::writeToConsole(Level level, const QString& message)
{
    switch (level) {
    case Level::Debug:
        qDebug() << message;
        break;
    case Level::Info:
        qInfo() << message;
        break;
    case Level::Warning:
        qWarning() << message;
        break;
    case Level::Critical:
        qCritical() << message;
        break;
    }
}

void Logger::writeToFile(const QString& message)
{
    if (!m_logFile.isOpen()) return;

    QTextStream out(&m_logFile);
    out << QString("[%1] %2\n")
               .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
               .arg(message);
}

QString Logger::levelToString(Level level)
{
    switch (level) {
    case Level::Debug:      return "DEBUG";
    case Level::Info:       return "INFO";
    case Level::Warning:    return "WARNING";
    case Level::Critical:   return "CRITICAL";
    default:                return "UNKNOWN";
    }
}

void Log(const Logger::Level &level, const QString &message)
{
    Logger::instance()->write(level, message);
}
