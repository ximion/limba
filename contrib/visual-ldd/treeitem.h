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
#ifndef TREEITEM_H
#define TREEITEM_H

#include <QList>
#include <QColor>
#include <QVariant>

//! [0]
class TreeItem
{
public:
    TreeItem(const QString name, const QString dir, TreeItem *parent = 0);
    ~TreeItem();

    void appendChild(TreeItem *child);

    TreeItem *child(int row);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    int row() const;
    TreeItem *parent();
    void setParent(TreeItem *item);
    QString getData_Dir() const;
    QString getData_SOName() const;
    QColor  getColor() const;

private:
    QList<TreeItem*> childItems;
    QString d_soname;
    QString d_dir;
    QColor entryColor;
    TreeItem *parentItem;
};

#endif
