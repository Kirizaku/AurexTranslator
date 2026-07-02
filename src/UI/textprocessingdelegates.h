/******************************************************************************
    Copyright (C) 2025-2026 by Daniil Nabiulin

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

#ifndef TEXTPROCESSINGDELEGATES_H
#define TEXTPROCESSINGDELEGATES_H

#include <QAbstractItemModel>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QModelIndex>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOptionButton>
#include <QStyleOptionViewItem>

#include <functional>

class CheckBoxDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionButton opt;
        opt.state = QStyle::State_Enabled;
        opt.state |= (index.data(Qt::CheckStateRole).toInt() == Qt::Checked) ? QStyle::State_On : QStyle::State_Off;
        const QSize sz = QApplication::style()->sizeFromContents(QStyle::CT_CheckBox, &opt, QSize());
        opt.rect = QRect(option.rect.x() + (option.rect.width()  - sz.width())  / 2,
                         option.rect.y() + (option.rect.height() - sz.height()) / 2,
                         sz.width(), sz.height());
        QApplication::style()->drawControl(QStyle::CE_CheckBox, &opt, painter);
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &, const QModelIndex &index) override
    {
        if (event->type() == QEvent::MouseButtonRelease) {
            Qt::CheckState cur = index.data(Qt::CheckStateRole).value<Qt::CheckState>();
            model->setData(index, cur == Qt::Checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
            return true;
        }
        return false;
    }
};

class SourceComboDelegate : public QStyledItemDelegate
{
public:
    explicit SourceComboDelegate(std::function<QStringList()> supplier, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), m_supplier(std::move(supplier)) {}

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        auto *combo = new QComboBox(parent);
        combo->setEditable(true);
        combo->setInsertPolicy(QComboBox::NoInsert);
        combo->addItem(QString());
        combo->addItems(m_supplier());
        return combo;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox *>(editor))
            combo->setCurrentText(index.data(Qt::EditRole).toString());
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox *>(editor))
            model->setData(index, combo->currentText().trimmed(), Qt::EditRole);
    }

private:
    std::function<QStringList()> m_supplier;
};

#endif // TEXTPROCESSINGDELEGATES_H
