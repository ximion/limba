/*
* Copyright (C) 2010 Matthias Klumpp
*
* Authors:
*  Matthias Klumpp
*
* This unit is free software: you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation, version 3.
*
* This unit is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License v3
* along with this library. If not, see <http://www.gnu.org/licenses/>.
*/
#include "treeitem.h"
#include <iostream>

TreeItem::TreeItem(const QString name, const QString dir, TreeItem *parent)
{
    parentItem = parent;
    d_soname = name;
    d_dir = dir;
    //Paint all items light blue...
    entryColor = QColor(152,245,255);
    //..except they're essential system libs
    if (name.indexOf(QRegExp("libc.*")) > -1)
        entryColor = QColor(209,238,238);
    if (name.indexOf(QRegExp("ld-linux*")) > -1)
        entryColor = QColor(188,210,238);
}

TreeItem::~TreeItem()
{
    qDeleteAll(childItems);
}

void TreeItem::appendChild(TreeItem *item)
{
    item->setParent(this);
    childItems.append(item);
}

TreeItem *TreeItem::child(int row)
{
    return childItems.value(row);
}

void TreeItem::setParent(TreeItem *item)
{
    parentItem = item;
}

int TreeItem::childCount() const
{
    return childItems.count();
}

int TreeItem::columnCount() const
{
    return 2;
}

QColor TreeItem::getColor() const
{
    return entryColor;
}

QVariant TreeItem::data(int column) const
{
    switch (column)
    {
    case 0: return d_soname;
    case 1: return d_dir;
    default: return "<?>";
    }
}

QString TreeItem::getData_Dir() const
{
    return d_dir;
}

QString TreeItem::getData_SOName() const
{
    return d_soname;
}

TreeItem *TreeItem::parent()
{
    return parentItem;
}

int TreeItem::row() const
{
    if (parentItem)
        return parentItem->childItems.indexOf(const_cast<TreeItem*>(this));
    return 0;
}
