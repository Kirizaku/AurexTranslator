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

#include "hotkeys.h"

HotKeys::HotKeys(QObject *parent)
    : QObject(parent)
    , m_hotkey(new QHotkey(this))
{
    connect(m_hotkey, &QHotkey::activated, this, &HotKeys::activated);
}

HotKeys::~HotKeys()
{
    delete m_hotkey;
}

void HotKeys::setShortcut(const QKeySequence &sequence)
{
    m_hotkey->setRegistered(false);
    if (!sequence.isEmpty()) {
        m_hotkey->setShortcut(sequence, true);
    }
}

void HotKeys::resetShortcut()
{
    m_hotkey->resetShortcut();
}

bool HotKeys::isRegistered() const
{
    return m_hotkey->isRegistered();
}
