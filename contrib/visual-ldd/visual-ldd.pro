#-------------------------------------------------
#
# Project file for Visual-LDD
# Created on D:2010-07-18 T:17:30:59
#
#-------------------------------------------------

QT += core gui

TARGET = visual-ldd
TEMPLATE = app


SOURCES += main.cpp\
	mainwindow.cpp \
	treeitem.cpp \
	treemodel.cpp

HEADERS += mainwindow.h \
	treemodel.h \
	treeitem.h

FORMS += mainwindow.ui


RESOURCES += \
	resources.qrc
