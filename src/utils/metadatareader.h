/******************************************************************************
    Copyright (C) 2026 by Daniil Nabiulin

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

#ifndef METADATAREADER_H
#define METADATAREADER_H

#include <QString>
#include <QJsonObject>

class MetaDataReader
{
public:
    explicit MetaDataReader(const QString &filePath);

    bool isValid() const;
    QJsonObject metaData() const;

private:
    QString m_filePath;
    QJsonObject m_metaData;
    QString m_errorString;
    bool m_isValid;

    bool read();
};

#endif // METADATAREADER_H
