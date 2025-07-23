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

#ifndef LOGGER_H
#define LOGGER_H

#include <QFile>

class Logger : public QObject
{
    Q_OBJECT

public:
    enum class Level { Debug, Info, Warning, Critical };

    static Logger *instance();
    static void initInstance(const QString& configPath);
    static void destroyInstance();

    static QString getLogDirPath() { return instance()->m_logsDirPath; }
    static QString getLogFilePath() { return instance()->m_logFilePath; }

    void write(Level level, const QString& message);

signals:
    void newLogMessage(const QString& message);

private:
    Logger(QObject* parent = nullptr);
    ~Logger();

    static Logger *m_instance;

    void writeToConsole(Level level, const QString& message);
    void writeToFile(const QString& message);
    QString levelToString(Level level);
    QString m_logsDirPath = "";
    QString m_logFilePath = "";
    QFile m_logFile;
};

void Log(const Logger::Level &level, const QString &message);

#endif // LOGGER_H
