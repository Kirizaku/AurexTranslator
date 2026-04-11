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

#include "metadatareader.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QJsonDocument>

#include "logger.h"

#ifdef Q_OS_WIN
#include <windows.h>
#elif defined(Q_OS_LINUX)
#include <elf.h>
#include <cstring>
#endif

MetaDataReader::MetaDataReader(const QString &filePath)
    : m_filePath(filePath)
    , m_isValid(false)
{
    QFileInfo info(filePath);
    if (!info.exists()) {
        Log(Logger::Level::Warning, "[metadata-reader] File not found");
        return;
    }

    m_isValid = read();
}

bool MetaDataReader::isValid() const
{
    return m_isValid;
}

QJsonObject MetaDataReader::metaData() const
{
    return m_metaData;
}

bool MetaDataReader::read()
{
#ifdef Q_OS_WIN
    DWORD size = GetFileVersionInfoSizeW((LPCWSTR)m_filePath.utf16(), nullptr);
    if (size == 0) {
        Log(Logger::Level::Warning, "[metadata-reader] No version info (GetFileVersionInfoSize)");
        return false;
    }

    QByteArray buffer(size, 0);
    if (!GetFileVersionInfoW((LPCWSTR)m_filePath.utf16(), 0, size, buffer.data())) {
        Log(Logger::Level::Warning, "GetFileVersionInfo failed");
        return false;
    }

    void *data = nullptr;
    UINT len = 0;

    if (VerQueryValueW(buffer.data(), L"\\StringFileInfo\\040904E4\\.note.at.metadata", &data, &len)) {
        QString metaString = QString::fromWCharArray((wchar_t*)data);
        QJsonDocument doc = QJsonDocument::fromJson(metaString.toUtf8());

        if (doc.isObject()) {
            m_metaData = doc.object();
            return true;
        } else {
            Log(Logger::Level::Warning, "[metadata-reader] Invalid JSON format in metadata");
            return false;
        }
    }

    m_errorString = "[metadata-reader] MetaData key not found in resources";
    return false;
}
#endif
#ifdef Q_OS_LINUX

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_errorString = "[metadata-reader] Cannot open file";
        Log(Logger::Level::Warning, "[metadata-reader] Cannot open file");
        return false;
    }

    QByteArray elfData = file.readAll();
    const char *data = elfData.constData();
    size_t fileSize = elfData.size();

    if (fileSize < EI_NIDENT || memcmp(data, ELFMAG, SELFMAG) != 0) {
        Log(Logger::Level::Warning, "[metadata-reader] Not a valid ELF file");
        return false;
    }

    unsigned char elfClass = data[EI_CLASS];
    const QString sectorName = ".note.at.metadata";

    if (elfClass == ELFCLASS32) {
        if (fileSize < sizeof(Elf32_Ehdr)) {
            Log(Logger::Level::Warning, "[metadata-reader] File too small for Elf32_Ehdr");
            return false;
        }

        const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *)data;

        if (ehdr->e_shoff + ehdr->e_shnum * sizeof(Elf32_Shdr) > fileSize) {
            Log(Logger::Level::Warning, "[metadata-reader] Invalid section header offset");
            return false;
        }

        const Elf32_Shdr *shdr = (const Elf32_Shdr *)(data + ehdr->e_shoff);

        if (ehdr->e_shstrndx >= ehdr->e_shnum) {
            Log(Logger::Level::Warning, "[metadata-reader] Invalid section header string table index");
            return false;
        }

        const char *shstrtab = data + shdr[ehdr->e_shstrndx].sh_offset;

        for (int i = 0; i < ehdr->e_shnum; ++i) {
            if (shdr[i].sh_name >= shdr[ehdr->e_shstrndx].sh_size) {
                continue;
            }

            const char *secName = shstrtab + shdr[i].sh_name;
            if (strcmp(secName, sectorName.toStdString().c_str()) == 0) {
                if (shdr[i].sh_size == 0) {
                    Log(Logger::Level::Warning, "[metadata-reader] Empty metadata section");
                    return false;
                }

                if (shdr[i].sh_offset + shdr[i].sh_size > fileSize) {
                    Log(Logger::Level::Warning, "[metadata-reader] Metadata section data out of file bounds");
                    return false;
                }

                const char *secData = data + shdr[i].sh_offset;
                size_t secSize = shdr[i].sh_size;

                if (secSize < 12) {
                    Log(Logger::Level::Warning, "[metadata-reader] Note too small");
                    return false;
                }

                uint32_t namesz = *((uint32_t*)(secData + 0));
                uint32_t descsz = *((uint32_t*)(secData + 4));

                if (namesz == 0 || descsz == 0) {
                    Log(Logger::Level::Warning, "[metadata-reader] Invalid sizes in note header");
                    return false;
                }

                size_t nameAligned = (namesz + 3) & ~3;
                size_t offset = 12 + nameAligned;

                if (offset + descsz > secSize) {
                    Log(Logger::Level::Warning, "[metadata-reader] Data overflow in note section");
                    return false;
                }

                QString metaString = QString::fromUtf8(secData + offset, descsz);
                QJsonDocument doc = QJsonDocument::fromJson(metaString.toUtf8());

                if (doc.isObject()) {
                    m_metaData = doc.object();
                    return true;
                } else {
                    Log(Logger::Level::Warning, "[metadata-reader] Invalid JSON format in metadata");
                    return false;
                }
            }
        }
    }
    else if (elfClass == ELFCLASS64) {
        if (fileSize < sizeof(Elf64_Ehdr)) {
            Log(Logger::Level::Warning, "[metadata-reader] File too small for Elf64_Ehdr");
            return false;
        }

        const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)data;

        if (ehdr->e_shoff + ehdr->e_shnum * sizeof(Elf64_Shdr) > fileSize) {
            Log(Logger::Level::Warning, "[metadata-reader] Invalid section header offset");
            return false;
        }

        const Elf64_Shdr *shdr = (const Elf64_Shdr *)(data + ehdr->e_shoff);

        if (ehdr->e_shstrndx >= ehdr->e_shnum) {
            Log(Logger::Level::Warning, "[metadata-reader] Invalid section header string table index");
            return false;
        }

        const char *shstrtab = data + shdr[ehdr->e_shstrndx].sh_offset;

        for (int i = 0; i < ehdr->e_shnum; ++i) {
            if (shdr[i].sh_name >= shdr[ehdr->e_shstrndx].sh_size) {
                continue;
            }

            const char *secName = shstrtab + shdr[i].sh_name;
            if (strcmp(secName, sectorName.toStdString().c_str()) == 0) {
                if (shdr[i].sh_size == 0) {
                    Log(Logger::Level::Warning, "[metadata-reader] Empty metadata section");
                    return false;
                }

                if (shdr[i].sh_offset + shdr[i].sh_size > fileSize) {
                    Log(Logger::Level::Warning, "[metadata-reader] Metadata section data out of file bounds");
                    return false;
                }

                const char *secData = data + shdr[i].sh_offset;
                size_t secSize = shdr[i].sh_size;

                if (secSize < 12) {
                    Log(Logger::Level::Warning, "[metadata-reader] Note too small");
                    return false;
                }

                uint32_t namesz = *((uint32_t*)(secData + 0));
                uint32_t descsz = *((uint32_t*)(secData + 4));

                if (namesz == 0 || descsz == 0) {
                    Log(Logger::Level::Warning, "[metadata-reader] Invalid sizes in note header");
                    return false;
                }

                size_t nameAligned = (namesz + 3) & ~3;
                size_t offset = 12 + nameAligned;

                if (offset + descsz > secSize) {
                    Log(Logger::Level::Warning, "[metadata-reader] Data overflow in note section");
                    return false;
                }

                QString metaString = QString::fromUtf8(secData + offset, descsz);
                QJsonDocument doc = QJsonDocument::fromJson(metaString.toUtf8());

                if (doc.isObject()) {
                    m_metaData = doc.object();
                    return true;
                } else {
                    Log(Logger::Level::Warning, "[metadata-reader] Invalid JSON format in metadata");
                    return false;
                }
            }
        }
    } else {
        Log(Logger::Level::Warning, "[metadata-reader] Unknown ELF class");
        return false;
    }

    Log(Logger::Level::Warning, "[metadata-reader] Metadata section not found");
    return false;
}
#endif