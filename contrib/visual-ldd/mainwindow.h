/*
* Copyright (C) 2010 Matthias Klumpp
*
* Authors:
*  Filippos Papadopoulos
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtGui>
#include <iostream>
#include <vector>
#include <algorithm>
#include "read_elf.h"
#include "treeitem.h"

using std::vector;
using std::string;
using std::cout;
using std::cerr;
using std::endl;

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString fname, QWidget *parent = 0);
    ~MainWindow();
    QString findFullPath(QString soname);
    void loadFile(QString filename);

public slots:
    void fileOpen();
    void showAboutBox();

private:
    Ui::MainWindow *ui;
    vector <QString> vectorLdConfig;
    QString lastFileName;
    QLabel *statusLabel;

    void findLibs(const QString& dirname);
    void resolveItem(TreeItem* itemChild);
};

#endif // MAINWINDOW_H
