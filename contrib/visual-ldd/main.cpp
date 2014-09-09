/*
* Copyright (C) 2010 Matthias Klumpp
*
* Authors:
*  Filippos Papadopoulos
*  Matthias Klumpp
*
* This application is free software: you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation, version 3.
*
* This application is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License v3
* along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#include <QtGui/QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    if(argc > 2)
    {
        std::cerr << "Usage: " << argv[0] << " [ELFfilename]\n";
        exit(-1);
    }
    MainWindow w(argv[1]);
    w.show();

    return a.exec();
}
