#-------------------------------------------------
#
# Project created by QtCreator 2018-04-09T12:36:54
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = demo2
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    encode.cpp \
    md5.cpp

HEADERS  += mainwindow.h \
    encode.h \
    md5.h

FORMS    += mainwindow.ui
